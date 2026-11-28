/* Real EbOs implementation, deterministic fake ESP-IDF scheduling. */
#include "EbOs.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct FakeTask {
    TaskFunction_t entry;
    void *argument;
    eTaskState state;
    uint32 stack_min;
};
struct FakeMutex { boolean held; };
static struct FakeTask caller = {0};
static TaskHandle_t current = &caller;
static TaskHandle_t latest;
static jmp_buf parked;
static unsigned live_metadata, live_tasks, live_mutexes, deletes, delay_calls;
static boolean isr, fail_metadata, fail_task, fail_mutex, run_immediately, defer_suspend, finish_on_delay;
static uint64 now_us;
static TickType_t last_delay, last_take;
static uint32 last_stack, last_priority;
static BaseType_t last_core;
static unsigned calls;
extern const EbOs_TaskConfigType SecondProject_TaskConfig;

void *heap_caps_calloc(size_t count, size_t size, unsigned capabilities)
{
    assert(capabilities == (MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    if (fail_metadata) return NULL;
    void *value = calloc(count, size);
    assert(value);
    ++live_metadata;
    return value;
}
void heap_caps_free(void *value)
{
    assert(value && live_metadata);
    --live_metadata;
    free(value);
}
BaseType_t xPortInIsrContext(void) { return isr; }
int64_t esp_timer_get_time(void) { return (int64_t)now_us; }
TaskHandle_t xTaskGetCurrentTaskHandle(void) { return current; }

static void run_task(TaskHandle_t task)
{
    TaskHandle_t previous = current;
    current = task;
    task->state = eRunning;
    if (!setjmp(parked)) {
        task->entry(task->argument);
        assert(!"Native task entry returned");
    }
    current = previous;
}
BaseType_t xTaskCreatePinnedToCore(TaskFunction_t entry, const char *name, uint32_t stack,
    void *argument, UBaseType_t priority, TaskHandle_t *handle, BaseType_t core)
{
    assert(name && name[0] && entry && handle);
    last_stack = stack;
    last_priority = priority;
    last_core = core;
    if (fail_task) return pdFALSE;
    latest = calloc(1, sizeof(*latest));
    assert(latest);
    ++live_tasks;
    latest->entry = entry;
    latest->argument = argument;
    latest->state = eReady;
    latest->stack_min = 736U;
    /* Exercise completion before create returns or writes its output handle. */
    if (run_immediately) run_task(latest);
    *handle = latest;
    return pdPASS;
}
UBaseType_t uxTaskGetStackHighWaterMark(TaskHandle_t task)
{
    if (!task) task = current;
    assert(task && task != &caller && task->state != eDeleted);
    return task->stack_min;
}
void vTaskSuspend(TaskHandle_t task)
{
    assert(task == NULL && current != &caller);
    current->state = defer_suspend ? eRunning : eSuspended;
    longjmp(parked, 1);
}
eTaskState eTaskGetState(TaskHandle_t task)
{
    assert(task && task->state != eDeleted);
    return task->state;
}
void vTaskDelete(TaskHandle_t task)
{
    assert(task && task != current && task->state == eSuspended);
    assert(live_tasks);
    --live_tasks;
    ++deletes;
    task->state = eDeleted;
    if (latest == task) latest = NULL;
    free(task);
}
void vTaskDelay(TickType_t ticks)
{
    last_delay = ticks;
    ++delay_calls;
    now_us += (uint64)ticks * 1000000U / configTICK_RATE_HZ;
    if (finish_on_delay) {
        finish_on_delay = FALSE;
        run_task(latest);
    } else if (latest && latest->state == eRunning) {
        latest->state = eSuspended;
    }
}
SemaphoreHandle_t xSemaphoreCreateMutex(void)
{
    if (fail_mutex) return NULL;
    SemaphoreHandle_t mutex = calloc(1, sizeof(*mutex));
    assert(mutex);
    ++live_mutexes;
    return mutex;
}
BaseType_t xSemaphoreTake(SemaphoreHandle_t mutex, TickType_t ticks)
{
    assert(mutex);
    last_take = ticks;
    if (mutex->held) return pdFALSE;
    mutex->held = TRUE;
    return pdTRUE;
}
BaseType_t xSemaphoreGive(SemaphoreHandle_t mutex)
{
    assert(mutex && mutex->held);
    mutex->held = FALSE;
    return pdTRUE;
}
void vSemaphoreDelete(SemaphoreHandle_t mutex)
{
    assert(mutex && !mutex->held && live_mutexes);
    --live_mutexes;
    free(mutex);
}
static void entry(void *argument) { assert(argument == &calls); ++calls; }
static EbOs_TaskConfigType config(void)
{
    return (EbOs_TaskConfigType){"UnrelatedWorker", 4096U, 5U, EBOS_CORE_ANY, entry};
}

static void invalid_config_and_failure(void)
{
    EbOs_TaskConfigType cfg = config();
    EbOs_TaskType task = NULL;
    assert(EbOs_TaskCreate(&cfg, &calls, NULL) == NEO_E_PARAM);
    assert(EbOs_TaskCreate(NULL, &calls, &task) == NEO_E_PARAM && !task);
    for (unsigned test = 0; test < 9; ++test) {
        cfg = config();
        switch (test) {
        case 0: cfg.name = NULL; break;
        case 1: cfg.name = ""; break;
        case 2: cfg.entry = NULL; break;
        case 3: cfg.stack_bytes = 0; break;
        case 4: cfg.stack_bytes = configMINIMAL_STACK_SIZE - 1U; break;
        case 5: cfg.priority = 0; break;
        case 6: cfg.priority = configMAX_PRIORITIES; break;
        case 7: cfg.core_affinity = -2; break;
        default: cfg.core_affinity = configNUMBER_OF_CORES; break;
        }
        assert(EbOs_TaskCreate(&cfg, &calls, &task) == NEO_E_PARAM && !task);
        assert(live_metadata == 0U && live_tasks == 0U);
    }
    cfg = config();
    fail_metadata = TRUE;
    assert(EbOs_TaskCreate(&cfg, &calls, &task) == NEO_E_MEMORY && !task);
    fail_metadata = FALSE;
    fail_task = TRUE;
    assert(EbOs_TaskCreate(&cfg, &calls, &task) == NEO_E_MEMORY && !task);
    fail_task = FALSE;
    isr = TRUE;
    assert(EbOs_TaskCreate(&cfg, &calls, &task) == NEO_E_STATE && !task);
    assert(EbOs_TaskJoinAndDelete(&task, 0) == NEO_E_STATE);
    isr = FALSE;
    assert(!live_metadata && !live_tasks);
}

static void live_timeout_retry_and_stats(void)
{
    EbOs_TaskConfigType cfg = config();
    cfg.core_affinity = configNUMBER_OF_CORES - 1;
    EbOs_TaskType task = NULL;
    uint32 bytes = 0;
    assert(EbOs_TaskCreate(&cfg, &calls, &task) == NEO_OK);
    assert(last_stack == 4096U && last_priority == 5U && last_core == cfg.core_affinity);
    assert(EbOs_TaskGetStackMinFree(task, &bytes) == NEO_OK && bytes == 736U);
    assert(EbOs_TaskGetStackMinFree(NULL, &bytes) == NEO_E_PARAM && !bytes);
    assert(EbOs_TaskGetStackMinFree(task, NULL) == NEO_E_PARAM);
    isr = TRUE;
    assert(EbOs_TaskGetStackMinFree(task, &bytes) == NEO_E_STATE);
    isr = FALSE;
    current = latest;
    assert(EbOs_TaskJoinAndDelete(&task, 0) == NEO_E_STATE);
    current = &caller;
    assert(EbOs_TaskJoinAndDelete(&task, 0) == NEO_E_TIMEOUT && task);
    unsigned count = delay_calls;
    assert(EbOs_TaskJoinAndDelete(&task, 15U) == NEO_E_TIMEOUT && task);
    assert(delay_calls == count + 2U && live_tasks == 1U && live_metadata == 1U);
    finish_on_delay = TRUE;
    assert(EbOs_TaskJoinAndDelete(&task, EBOS_WAIT_FOREVER) == NEO_OK && !task);
    assert(EbOs_TaskJoinAndDelete(&task, 0) == NEO_OK);
    assert(EbOs_TaskJoinAndDelete(NULL, 0) == NEO_E_PARAM);
    assert(!live_tasks && !live_metadata);
}

static void completion_and_suspension_handshake(void)
{
    EbOs_TaskConfigType cfg = config();
    run_immediately = TRUE;
    for (unsigned i = 0; i < 4; ++i) {
        EbOs_TaskType task = NULL;
        defer_suspend = TRUE;
        assert(EbOs_TaskCreate(&cfg, &calls, &task) == NEO_OK);
        assert(EbOs_TaskJoinAndDelete(&task, 0) == NEO_E_TIMEOUT && task);
        uint32 bytes;
        assert(EbOs_TaskGetStackMinFree(task, &bytes) == NEO_OK && bytes == 736U);
        unsigned count = delay_calls;
        assert(EbOs_TaskJoinAndDelete(&task, 10U) == NEO_OK && !task);
        assert(delay_calls == count + 1U);
        defer_suspend = FALSE;
    }
    uint32 frames = 0;
    EbOs_TaskType screen = NULL;
    assert(EbOs_TaskCreate(&SecondProject_TaskConfig, &frames, &screen) == NEO_OK);
    assert(frames == 1U && last_stack == 3072U && last_priority == 3U && last_core == tskNO_AFFINITY);
    /* Native suspension can deepen the stack after the entry's final sample. */
    assert(latest->state == eSuspended);
    latest->stack_min = 608U;
    uint32 bytes;
    assert(EbOs_TaskGetStackMinFree(screen, &bytes) == NEO_OK && bytes == 608U);
    assert(EbOs_TaskJoinAndDelete(&screen, 0) == NEO_OK && !screen);
    run_immediately = FALSE;
    assert(!live_tasks && !live_metadata);
}

static void legacy_mutex_and_delay_contract(void)
{
    EbOs_MutexType mutex = NULL;
    assert(EbOs_MutexCreate(NULL) == NEO_E_PARAM);
    fail_mutex = TRUE;
    assert(EbOs_MutexCreate(&mutex) == NEO_E_MEMORY && !mutex && !live_metadata);
    fail_mutex = FALSE;
    assert(EbOs_MutexCreate(&mutex) == NEO_OK);
    assert(EbOs_MutexLock(NULL, 0) == NEO_E_PARAM);
    assert(EbOs_MutexLock(mutex, 1U) == NEO_OK && last_take == 1U);
    assert(EbOs_MutexLock(mutex, 0) == NEO_E_TIMEOUT && last_take == 0U);
    EbOs_MutexUnlock(mutex);
    assert(EbOs_MutexLock(mutex, EBOS_WAIT_FOREVER) == NEO_OK && last_take == portMAX_DELAY);
    EbOs_MutexUnlock(mutex);
    assert(EbOs_MutexLock(mutex, EBOS_WAIT_FOREVER - 1U) == NEO_OK && last_take < portMAX_DELAY);
    EbOs_MutexUnlock(mutex);
    EbOs_MutexDelete(mutex);
    EbOs_MutexDelete(NULL);
    EbOs_MutexUnlock(NULL);
    EbOs_Delay(0);
    assert(last_delay == 0U);
    EbOs_Delay(1U);
    assert(last_delay == 1U);
    EbOs_Delay(11U);
    assert(last_delay == 2U);
    EbOs_DelayOneTick();
    assert(last_delay == 1U);
    assert(!live_metadata && !live_mutexes);
}

int main(void)
{
    invalid_config_and_failure();
    live_timeout_retry_and_stats();
    completion_and_suspension_handshake();
    legacy_mutex_and_delay_contract();
    assert(calls == 5U && deletes == 6U);
    puts("PASS EbOs task lifecycle, legacy contracts and second-project configuration");
    return 0;
}
