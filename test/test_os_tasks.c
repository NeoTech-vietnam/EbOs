/* Exercise the real configured-task backend through its public interfaces. */
#include "EbOs_Os.h"
#ifdef NEO_TEST_PROJECT_CONFIG
#include "Os_Cfg.h"
#endif
#include "esp_heap_caps.h"
#include "freertos/task.h"
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct FakeTask {
    TaskFunction_t entry;
    void *argument;
    eTaskState state;
    uint32_t notifications;
    UBaseType_t priority;
    unsigned serial;
};
typedef struct FakeFrame {
    jmp_buf blocked;
    struct FakeFrame *previous;
} FakeFrame;

static struct FakeTask caller;
static TaskHandle_t current = &caller;
static TaskHandle_t workers[32];
static FakeFrame *frame;
static unsigned live_tasks, live_allocations, creates, deletes, allocations, critical_depth;
static unsigned fail_allocation, fail_create, block_calls, delay_calls;
static bool in_isr, interrupts_enabled = true, run_on_create = true, preempt_on_notify, defer_park;
static BaseType_t scheduler_state = taskSCHEDULER_RUNNING;

static void run_worker(TaskHandle_t task)
{
    TaskHandle_t previous = current;
    FakeFrame invocation;
    invocation.previous = frame;
    frame = &invocation;
    if (previous != &caller && previous->state == eRunning) previous->state = eReady;
    current = task;
    task->state = eRunning;
    if (setjmp(invocation.blocked) == 0) {
        task->entry(task->argument);
        assert(!"Persistent worker returned to native scheduler");
    }
    assert(critical_depth == 0U);
    current = previous;
    if (previous != &caller) previous->state = eRunning;
    frame = invocation.previous;
}

static void run_ready(void)
{
    unsigned dispatches = 0U;
    for (;;) {
        TaskHandle_t selected = NULL;
        for (unsigned index = 0U; index < 32U; ++index) {
            TaskHandle_t candidate = workers[index];
            if (candidate != NULL && candidate->notifications != 0U &&
                (selected == NULL || candidate->priority > selected->priority)) {
                selected = candidate;
            }
        }
        if (selected == NULL) break;
        assert(++dispatches < 100U);
        run_worker(selected);
    }
}

void *heap_caps_calloc(size_t count, size_t size, unsigned capabilities)
{
    assert(capabilities == (MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    if (++allocations == fail_allocation) return NULL;
    void *allocation = calloc(count, size);
    assert(allocation != NULL);
    ++live_allocations;
    return allocation;
}

void heap_caps_free(void *allocation)
{
    assert(allocation != NULL && live_allocations != 0U);
    --live_allocations;
    free(allocation);
}

BaseType_t xPortInIsrContext(void) { return in_isr ? pdTRUE : pdFALSE; }
bool xPortCanYield(void) { return interrupts_enabled; }
BaseType_t xTaskGetSchedulerState(void) { return scheduler_state; }
TaskHandle_t xTaskGetCurrentTaskHandle(void) { return current; }

void fakePortEnterCritical(portMUX_TYPE *lock)
{
    assert(lock != NULL);
    ++lock->depth;
    ++critical_depth;
}

void fakePortExitCritical(portMUX_TYPE *lock)
{
    assert(lock != NULL && lock->depth != 0U && critical_depth != 0U);
    --lock->depth;
    --critical_depth;
}

BaseType_t xTaskCreatePinnedToCore(TaskFunction_t entry, const char *name, uint32_t stack,
    void *argument, UBaseType_t priority, TaskHandle_t *handle, BaseType_t core)
{
    assert(entry != NULL && name != NULL && name[0] != '\0' && handle != NULL);
    assert(stack >= configMINIMAL_STACK_SIZE && priority > 0U && priority < configMAX_PRIORITIES);
    assert(core >= 0 && core < configNUMBER_OF_CORES && critical_depth == 0U);
    if (++creates == fail_create) return pdFALSE;
    unsigned slot;
    for (slot = 0U; slot < 32U && workers[slot] != NULL; ++slot) { }
    assert(slot < 32U);
    TaskHandle_t task = calloc(1U, sizeof(*task));
    assert(task != NULL);
    *task = (struct FakeTask){entry, argument, eReady, 0U, priority, creates};
    workers[slot] = task;
    ++live_tasks;
    /* A real new task may execute before the creating call writes its handle. */
    if (run_on_create) run_worker(task);
    *handle = task;
    return pdPASS;
}

uint32_t ulTaskNotifyTake(BaseType_t clear_on_exit, TickType_t timeout)
{
    assert(current != &caller && frame != NULL && critical_depth == 0U);
    assert(clear_on_exit == pdFALSE && timeout == portMAX_DELAY);
    if (current->notifications != 0U) return current->notifications--;
    ++block_calls;
    current->state = defer_park ? eRunning : eBlocked;
    longjmp(frame->blocked, 1);
}

BaseType_t xTaskNotifyGive(TaskHandle_t task)
{
    assert(task != NULL && task != &caller && critical_depth == 0U);
    ++task->notifications;
    if (task->state == eBlocked) task->state = eReady;
    if (preempt_on_notify && task != current && task->priority > current->priority) run_worker(task);
    return pdPASS;
}

eTaskState eTaskGetState(TaskHandle_t task)
{
    assert(task != NULL);
    return task->state;
}

void vTaskDelete(TaskHandle_t task)
{
    assert(task != NULL && task != current && task->state == eBlocked);
    assert(task->notifications == 0U && critical_depth == 0U && live_tasks != 0U);
    for (unsigned index = 0U; index < 32U; ++index) {
        if (workers[index] == task) workers[index] = NULL;
    }
    --live_tasks;
    ++deletes;
    free(task);
}

void vTaskDelay(TickType_t ticks)
{
    assert(ticks == 1U && critical_depth == 0U);
    ++delay_calls;
    /* Let newly created dormant workers reach their first private wait. */
    for (unsigned index = 0U; index < 32U; ++index) {
        if (workers[index] != NULL && workers[index]->state == eReady) run_worker(workers[index]);
    }
}

typedef enum {
    FINISH, QUEUE_SELF, CHAIN_SELF, CHAIN_OTHER, CHAIN_FULL, RETURN_WITHOUT_END, CHECK_CONTEXT,
    PREEMPT_OTHER, CHECK_PREEMPTED
} Action;
static Action actions[3];
static unsigned entries[3], missing_end_calls, completed_returns;
static TaskType expected_id;

static void terminate_from_nested_function(void)
{
    assert(TerminateTask() != E_OK);
    assert(!"Successful TerminateTask returned");
}

static void check_running(TaskType expected)
{
    TaskType id = INVALID_TASK;
    TaskStateType state = SUSPENDED;
    assert(GetTaskID(&id) == E_OK && id == expected);
    assert(GetTaskState(expected, &state) == E_OK && state == RUNNING);
}

static void task_body(TaskType id)
{
    ++entries[id];
    check_running(id);
    switch (actions[id]) {
    case QUEUE_SELF:
        if (entries[id] == 1U) {
            assert(ActivateTask(id) == E_OK);
            assert(ActivateTask(id) == E_OK);
            assert(ActivateTask(id) == E_OS_LIMIT);
        }
        break;
    case CHAIN_SELF:
        if (entries[id] == 1U) {
            (void)ChainTask(id);
            assert(!"Successful self ChainTask returned");
        }
        break;
    case CHAIN_OTHER:
        (void)ChainTask(1U);
        assert(!"Successful ChainTask returned");
        break;
    case CHAIN_FULL:
        assert(ChainTask(1U) == E_OS_LIMIT);
        check_running(id);
        ++completed_returns;
        break;
    case RETURN_WITHOUT_END:
        return;
    case CHECK_CONTEXT:
        assert(EbOs_OsDeinit() == NEO_E_BUSY);
        assert(ChainTask(99U) == E_OS_ID);
        assert(Schedule() == E_OK);
        assert(GetTaskID(NULL) == E_OS_ILLEGAL_ADDRESS);
        assert(GetTaskState(id, NULL) == E_OS_ILLEGAL_ADDRESS);
        break;
    case PREEMPT_OTHER:
        assert(ActivateTask(1U) == E_OK);
        assert(entries[1] == 1U);
        check_running(id);
        break;
    case CHECK_PREEMPTED: {
        TaskStateType state = SUSPENDED;
        assert(GetTaskState(0U, &state) == E_OK && state == READY);
        break;
    }
    case FINISH:
        break;
    }
    terminate_from_nested_function();
    assert(!"Terminated task resumed its entry");
}

static void task_zero(void) { task_body(0U); }
static void task_one(void) { task_body(1U); }
static void task_two(void) { task_body(2U); }

static void missing_end(TaskType id, StatusType error)
{
    assert(error == E_OS_MISSINGEND);
    ++missing_end_calls;
    assert(id == expected_id);
    check_running(id);
    assert(TerminateTask() == E_OS_CALLEVEL);
    assert(ChainTask(id) == E_OS_CALLEVEL);
    assert(Schedule() == E_OS_CALLEVEL);
}

static EbOs_OsTaskConfigType tasks[] = {
    {"TaskZero", 4096U, 5U, 1U, task_zero},
    {"TaskOne", 3072U, 6U, 1U, task_one},
    {"TaskTwo", 2048U, 7U, 1U, task_two}
};

static EbOs_OsConfigType configuration(void)
{
    return (EbOs_OsConfigType){tasks, 3U, configNUMBER_OF_CORES - 1, missing_end};
}

static void reset(void)
{
    assert(EbOs_OsDeinit() == NEO_OK);
    assert(live_tasks == 0U && live_allocations == 0U && critical_depth == 0U);
    memset(workers, 0, sizeof(workers));
    memset(entries, 0, sizeof(entries));
    memset(actions, 0, sizeof(actions));
    allocations = creates = deletes = fail_allocation = fail_create = 0U;
    missing_end_calls = completed_returns = block_calls = delay_calls = 0U;
    in_isr = defer_park = preempt_on_notify = false;
    interrupts_enabled = run_on_create = true;
    scheduler_state = taskSCHEDULER_RUNNING;
    current = &caller;
    for (unsigned index = 0U; index < 3U; ++index) tasks[index].activation_limit = 1U;
}

static void expect_state(TaskType id, TaskStateType expected)
{
    TaskStateType state = WAITING;
    assert(GetTaskState(id, &state) == E_OK && state == expected);
}

static void context_and_configuration(void)
{
    reset();
    TaskType id = 123U;
    TaskStateType state = WAITING;
    assert(ActivateTask(0U) == E_OS_STATE);
    assert(TerminateTask() == E_OS_STATE);
    assert(ChainTask(0U) == E_OS_STATE);
    assert(Schedule() == E_OS_STATE);
    assert(GetTaskID(&id) == E_OS_STATE);
    assert(GetTaskState(0U, &state) == E_OS_STATE);
    EbOs_OsConfigType cfg = configuration();
    assert(EbOs_OsInit(NULL) == NEO_E_PARAM);
    for (unsigned test = 0U; test < 14U; ++test) {
        EbOs_OsTaskConfigType altered[3];
        memcpy(altered, tasks, sizeof(altered));
        cfg = configuration();
        cfg.tasks = altered;
        switch (test) {
        case 0U: cfg.tasks = NULL; break;
        case 1U: cfg.task_count = 0U; break;
        case 2U: cfg.core_affinity = -1; break;
        case 3U: cfg.core_affinity = configNUMBER_OF_CORES; break;
        case 4U: altered[0].name = NULL; break;
        case 5U: altered[0].name = ""; break;
        case 6U: altered[0].stack_bytes = 0U; break;
        case 7U: altered[0].stack_bytes = configMINIMAL_STACK_SIZE - 1U; break;
        case 8U: altered[0].priority = 0U; break;
        case 9U: altered[0].priority = configMAX_PRIORITIES; break;
        case 10U: altered[0].priority = altered[1].priority; break;
        case 11U: altered[0].activation_limit = 0U; break;
        case 12U: altered[0].entry = NULL; break;
        default: cfg.task_count = (uint32)INVALID_TASK; break;
        }
        assert(EbOs_OsInit(&cfg) == NEO_E_PARAM);
        assert(live_tasks == 0U && live_allocations == 0U);
    }
    cfg = configuration();
    in_isr = true;
    assert(EbOs_OsInit(&cfg) == NEO_E_STATE);
    assert(EbOs_OsDeinit() == NEO_E_STATE);
    in_isr = false;
    interrupts_enabled = false;
    assert(EbOs_OsInit(&cfg) == NEO_E_STATE);
    interrupts_enabled = true;
    scheduler_state = taskSCHEDULER_SUSPENDED;
    assert(EbOs_OsInit(&cfg) == NEO_E_STATE);
    scheduler_state = taskSCHEDULER_RUNNING;
    assert(EbOs_OsInit(&cfg) == NEO_OK);
    assert(EbOs_OsInit(&cfg) == NEO_E_STATE);
    assert(block_calls == 3U && creates == 3U && live_tasks == 3U);
    assert(GetTaskID(&id) == E_OK && id == INVALID_TASK);
    assert(ActivateTask(99U) == E_OS_ID);
    assert(GetTaskState(99U, &state) == E_OS_ID);
    assert(GetTaskID(NULL) == E_OS_ILLEGAL_ADDRESS);
    assert(GetTaskState(0U, NULL) == E_OS_ILLEGAL_ADDRESS);
    assert(TerminateTask() == E_OS_CALLEVEL);
    assert(ChainTask(0U) == E_OS_CALLEVEL);
    assert(Schedule() == E_OS_CALLEVEL);
    for (unsigned prohibited = 0U; prohibited < 3U; ++prohibited) {
        in_isr = prohibited == 0U;
        interrupts_enabled = prohibited != 1U;
        scheduler_state = prohibited == 2U ? taskSCHEDULER_SUSPENDED : taskSCHEDULER_RUNNING;
        StatusType result = prohibited == 1U ? E_OS_DISABLEDINT : E_OS_CALLEVEL;
        assert(ActivateTask(0U) == result);
        assert(TerminateTask() == result);
        assert(ChainTask(0U) == result);
        assert(Schedule() == result);
        assert(GetTaskID(&id) == result);
        assert(GetTaskState(0U, &state) == result);
    }
    in_isr = false;
    interrupts_enabled = true;
    scheduler_state = taskSCHEDULER_RUNNING;
    expect_state(0U, SUSPENDED);
    actions[0] = CHECK_CONTEXT;
    assert(ActivateTask(0U) == E_OK);
    expect_state(0U, READY);
    assert(EbOs_OsDeinit() == NEO_E_BUSY);
    run_ready();
    assert(entries[0] == 1U);
    expect_state(0U, SUSPENDED);
    assert(EbOs_OsDeinit() == NEO_OK);
}

static void allocation_failures(void)
{
    reset();
    EbOs_OsConfigType cfg = configuration();
    fail_allocation = 1U;
    assert(EbOs_OsInit(&cfg) == NEO_E_MEMORY);
    assert(live_tasks == 0U && live_allocations == 0U);
    fail_allocation = 0U;
    for (unsigned failure = 1U; failure <= 3U; ++failure) {
        creates = 0U;
        fail_create = failure;
        run_on_create = failure != 3U;
        assert(EbOs_OsInit(&cfg) == NEO_E_MEMORY);
        assert(live_tasks == 0U && live_allocations == 0U);
        assert(ActivateTask(0U) == E_OS_STATE);
    }
    assert(delay_calls != 0U);
    fail_create = 0U;
    run_on_create = true;
    assert(EbOs_OsInit(&cfg) == NEO_OK);
    assert(EbOs_OsDeinit() == NEO_OK);
}

static void activation_and_chaining(void)
{
    reset();
    EbOs_OsConfigType cfg = configuration();
    tasks[0].activation_limit = 3U;
    actions[0] = QUEUE_SELF;
    assert(EbOs_OsInit(&cfg) == NEO_OK);
    TaskHandle_t original = workers[0];
    unsigned serial = original->serial;
    assert(ActivateTask(0U) == E_OK);
    run_ready();
    assert(entries[0] == 3U && workers[0] == original && workers[0]->serial == serial);
    assert(creates == 3U && deletes == 0U);
    expect_state(0U, SUSPENDED);
    actions[0] = FINISH;
    assert(ActivateTask(0U) == E_OK);
    run_ready();
    assert(entries[0] == 4U && workers[0] == original);
    assert(EbOs_OsDeinit() == NEO_OK);

    reset();
    cfg = configuration();
    actions[0] = CHAIN_SELF;
    assert(EbOs_OsInit(&cfg) == NEO_OK);
    assert(ActivateTask(0U) == E_OK);
    run_ready();
    assert(entries[0] == 2U && creates == 3U && deletes == 0U);
    expect_state(0U, SUSPENDED);
    assert(EbOs_OsDeinit() == NEO_OK);

    reset();
    cfg = configuration();
    actions[0] = CHAIN_FULL;
    assert(EbOs_OsInit(&cfg) == NEO_OK);
    assert(ActivateTask(1U) == E_OK);
    assert(ActivateTask(0U) == E_OK);
    /* Run the lower-priority caller first to keep the successor at its limit. */
    run_worker(workers[0]);
    assert(entries[0] == 1U && completed_returns == 1U && entries[1] == 0U);
    expect_state(0U, SUSPENDED);
    expect_state(1U, READY);
    run_ready();
    assert(entries[1] == 1U);
    assert(EbOs_OsDeinit() == NEO_OK);

    reset();
    cfg = configuration();
    actions[0] = CHAIN_OTHER;
    assert(EbOs_OsInit(&cfg) == NEO_OK);
    preempt_on_notify = true;
    assert(ActivateTask(0U) == E_OK);
    run_ready();
    assert(entries[0] == 1U && entries[1] == 1U && creates == 3U);
    expect_state(0U, SUSPENDED);
    expect_state(1U, SUSPENDED);
    assert(EbOs_OsDeinit() == NEO_OK);

    reset();
    cfg = configuration();
    actions[0] = PREEMPT_OTHER;
    actions[1] = CHECK_PREEMPTED;
    assert(EbOs_OsInit(&cfg) == NEO_OK);
    preempt_on_notify = true;
    assert(ActivateTask(0U) == E_OK);
    assert(entries[0] == 1U && entries[1] == 1U);
    expect_state(0U, SUSPENDED);
    assert(EbOs_OsDeinit() == NEO_OK);
}

static void missing_end_and_park_retry(void)
{
    reset();
    EbOs_OsConfigType cfg = configuration();
    expected_id = 2U;
    actions[2] = RETURN_WITHOUT_END;
    assert(EbOs_OsInit(&cfg) == NEO_OK);
    assert(ActivateTask(2U) == E_OK);
    run_ready();
    assert(missing_end_calls == 1U && entries[2] == 1U);
    expect_state(2U, SUSPENDED);
    assert(ActivateTask(2U) == E_OK);
    defer_park = true;
    run_ready();
    assert(missing_end_calls == 2U && entries[2] == 2U);
    expect_state(2U, SUSPENDED);
    assert(EbOs_OsDeinit() == NEO_E_BUSY && live_tasks == 3U);
    workers[2]->state = eBlocked;
    defer_park = false;
    assert(EbOs_OsDeinit() == NEO_OK);
    assert(live_tasks == 0U && live_allocations == 0U);

    cfg.missing_end_hook = NULL;
    assert(EbOs_OsInit(&cfg) == NEO_OK);
    assert(ActivateTask(2U) == E_OK);
    run_ready();
    assert(missing_end_calls == 2U && entries[2] == 3U);
    expect_state(2U, SUSPENDED);
    assert(EbOs_OsDeinit() == NEO_OK);
}

#ifdef NEO_TEST_PROJECT_CONFIG
static void project_configuration(void)
{
    reset();
    assert(EbOs_OsInit(&Os_Config) == NEO_OK);
    assert(Os_ProjectCheckResult() == E_OS_STATE);
    assert(ActivateTask(Os_Task_ProjectCheck) == E_OK);
    run_ready();
    assert(Os_ProjectCheckResult() == E_OK);
    expect_state(Os_Task_ProjectCheck, SUSPENDED);
    assert(EbOs_OsDeinit() == NEO_OK);
}
#endif

int main(void)
{
    context_and_configuration();
    allocation_failures();
    activation_and_chaining();
    missing_end_and_park_retry();
#ifdef NEO_TEST_PROJECT_CONFIG
    project_configuration();
#endif
    reset();
    printf("PASS configured Os task services (%u cores): activation, termination, chaining, state, failure cleanup\n",
           (unsigned)configNUMBER_OF_CORES);
    return 0;
}
