/* Real ESP-IDF acceptance fixture. Run separately from camera firmware. */
#include "EbOs_Os.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <assert.h>
#include <stdatomic.h>
#include <stdio.h>

static atomic_uint first_runs;
static atomic_uint second_runs;
static atomic_uint missing_ends;
static atomic_uint phase;

static void terminate_nested(void)
{
    (void)TerminateTask();
    assert(!"TerminateTask returned");
}

static void first_entry(void)
{
    TaskType id = INVALID_TASK;
    TaskStateType state = SUSPENDED;
    assert(GetTaskID(&id) == E_OK && id == 0U);
    assert(GetTaskState(id, &state) == E_OK && state == RUNNING);
    assert(Schedule() == E_OK);
    unsigned run = atomic_fetch_add_explicit(&first_runs, 1U, memory_order_relaxed) + 1U;
    if (atomic_load_explicit(&phase, memory_order_acquire) == 1U)
    {
        if (run == 1U)
        {
            assert(ActivateTask(0U) == E_OK);
            assert(ActivateTask(0U) == E_OS_LIMIT);
            assert(ActivateTask(1U) == E_OK);
            assert(ChainTask(1U) == E_OS_LIMIT);
            assert(GetTaskID(&id) == E_OK && id == 0U);
            terminate_nested();
        }
        else if (run == 2U)
        {
            (void)ChainTask(0U);
            assert(!"Self ChainTask returned");
        }
        else
        {
            assert(run == 3U);
            terminate_nested();
        }
    }
    else
    {
        (void)ChainTask(1U);
        assert(!"Successor ChainTask returned");
    }
}

static void second_entry(void)
{
    TaskType id = INVALID_TASK;
    assert(GetTaskID(&id) == E_OK && id == 1U);
    atomic_fetch_add_explicit(&second_runs, 1U, memory_order_relaxed);
    terminate_nested();
}

static void returning_entry(void)
{
    /* Deliberate missing termination: the runtime must diagnose and retire it. */
}

static void missing_end(TaskType id, StatusType error)
{
    assert(error == E_OS_MISSINGEND);
    TaskType current = INVALID_TASK;
    TaskStateType state = SUSPENDED;
    assert(id == 2U);
    assert(GetTaskID(&current) == E_OK && current == id);
    assert(GetTaskState(id, &state) == E_OK && state == RUNNING);
    assert(TerminateTask() == E_OS_CALLEVEL);
    assert(ChainTask(0U) == E_OS_CALLEVEL);
    assert(Schedule() == E_OS_CALLEVEL);
    atomic_fetch_add_explicit(&missing_ends, 1U, memory_order_release);
}

static void await_suspended(TaskType id)
{
    const int64_t deadline = esp_timer_get_time() + 3000000;
    TaskStateType state = RUNNING;
    do
    {
        assert(GetTaskState(id, &state) == E_OK);
        if (state == SUSPENDED)
        {
            return;
        }
        assert(esp_timer_get_time() < deadline);
        vTaskDelay(1);
    } while (1);
}

void test_os_tasks_on_target(void)
{
    static const EbOs_OsTaskConfigType tasks[] = {
        {"BasicFirst", 4096U, 5U, 2U, first_entry},
        {"BasicSecond", 4096U, 4U, 1U, second_entry},
        {"BasicReturning", 4096U, 3U, 1U, returning_entry}
    };
    for (sint32 core = 0; core < configNUMBER_OF_CORES; ++core)
    {
        EbOs_OsConfigType config = {tasks, 3U, core, missing_end};
        for (unsigned repeat = 0; repeat < 50U; ++repeat)
        {
            atomic_store(&first_runs, 0U);
            atomic_store(&second_runs, 0U);
            atomic_store(&missing_ends, 0U);
            atomic_store(&phase, 1U);
            assert(EbOs_OsInit(&config) == NEO_OK);
            assert(GetTaskID(NULL_PTR) == E_OS_ILLEGAL_ADDRESS);
            assert(GetTaskState(0U, NULL_PTR) == E_OS_ILLEGAL_ADDRESS);
            TaskType unchanged = 123U;
            portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
            portENTER_CRITICAL(&mux);
            StatusType masked_result = GetTaskID(&unchanged);
            portEXIT_CRITICAL(&mux);
            assert(masked_result == E_OS_DISABLEDINT && unchanged == 123U);
            assert(ActivateTask(0U) == E_OK);
            await_suspended(0U);
            await_suspended(1U);
            assert(atomic_load(&first_runs) == 3U && atomic_load(&second_runs) == 1U);
            atomic_store_explicit(&phase, 2U, memory_order_release);
            assert(ActivateTask(0U) == E_OK);
            await_suspended(0U);
            await_suspended(1U);
            assert(atomic_load(&first_runs) == 4U && atomic_load(&second_runs) == 2U);
            assert(ActivateTask(2U) == E_OK);
            await_suspended(2U);
            assert(atomic_load_explicit(&missing_ends, memory_order_acquire) == 1U);
            const int64_t deadline = esp_timer_get_time() + 3000000;
            Neo_StatusType result;
            while ((result = EbOs_OsDeinit()) == NEO_E_BUSY)
            {
                assert(esp_timer_get_time() < deadline);
                vTaskDelay(1);
            }
            assert(result == NEO_OK);
        }
    }
    printf("PASS EBOS_CORE_TASK_SERVICES: 50 activation/chain/return cycles per core\n");
}
