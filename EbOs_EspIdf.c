#include "EbOs.h"

#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <stdatomic.h>
#include <stdbool.h>

struct EbOs_Mutex {
    SemaphoreHandle_t handle;
};

struct EbOs_Task {
    TaskHandle_t handle;
    void (*entry)(void *argument);
    void *argument;
    atomic_bool complete;
    uint32 final_stack_min_free;
};

static TickType_t ticks_for(uint32 ms)
{
    uint64 ticks = ((uint64)ms * configTICK_RATE_HZ + 999U) / 1000U;
    return ticks >= portMAX_DELAY ? portMAX_DELAY - 1U : (TickType_t)ticks;
}

Neo_StatusType EbOs_MutexCreate(EbOs_MutexType *out)
{
    if (out == NULL_PTR) return NEO_E_PARAM;
    *out = NULL_PTR;
    EbOs_MutexType mutex = heap_caps_calloc(1, sizeof(*mutex), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (mutex == NULL_PTR) return NEO_E_MEMORY;
    mutex->handle = xSemaphoreCreateMutex();
    if (mutex->handle == NULL_PTR) {
        heap_caps_free(mutex);
        return NEO_E_MEMORY;
    }
    *out = mutex;
    return NEO_OK;
}

Neo_StatusType EbOs_MutexLock(EbOs_MutexType mutex, uint32 timeout_ms)
{
    if (mutex == NULL_PTR) return NEO_E_PARAM;
    TickType_t wait = timeout_ms == EBOS_WAIT_FOREVER ? portMAX_DELAY : ticks_for(timeout_ms);
    return xSemaphoreTake(mutex->handle, wait) == pdTRUE ? NEO_OK : NEO_E_TIMEOUT;
}

void EbOs_MutexUnlock(EbOs_MutexType mutex)
{
    if (mutex != NULL_PTR) (void)xSemaphoreGive(mutex->handle);
}

void EbOs_MutexDelete(EbOs_MutexType mutex)
{
    if (mutex == NULL_PTR) return;
    vSemaphoreDelete(mutex->handle);
    heap_caps_free(mutex);
}

void EbOs_Delay(uint32 ms)
{
    vTaskDelay(ticks_for(ms));
}

void EbOs_DelayOneTick(void)
{
    vTaskDelay(1);
}

static void task_entry(void *argument)
{
    EbOs_TaskType task = argument;
    task->entry(task->argument);
    task->final_stack_min_free = (uint32)uxTaskGetStackHighWaterMark(NULL);
    atomic_store_explicit(&task->complete, true, memory_order_release);
    /* No metadata or caller context is accessed after publishing completion.
     * Join also waits for native suspension: completion alone does not prove
     * the other core has stopped using this task's stack. */
    for (;;) vTaskSuspend(NULL);
}

Neo_StatusType EbOs_TaskCreate(const EbOs_TaskConfigType *config, void *argument, EbOs_TaskType *out)
{
    if (out == NULL_PTR) return NEO_E_PARAM;
    *out = NULL_PTR;
    if (xPortInIsrContext()) return NEO_E_STATE;
    if (config == NULL_PTR || config->name == NULL_PTR || config->name[0] == '\0' ||
        config->entry == NULL_PTR || config->stack_bytes < configMINIMAL_STACK_SIZE ||
        config->priority == 0U || config->priority >= configMAX_PRIORITIES ||
        config->core_affinity < EBOS_CORE_ANY || config->core_affinity >= configNUMBER_OF_CORES)
        return NEO_E_PARAM;
    EbOs_TaskType task = heap_caps_calloc(1, sizeof(*task), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (task == NULL_PTR) return NEO_E_MEMORY;
    task->entry = config->entry;
    task->argument = argument;
    atomic_init(&task->complete, false);
    BaseType_t core = config->core_affinity == EBOS_CORE_ANY ? tskNO_AFFINITY : config->core_affinity;
    if (xTaskCreatePinnedToCore(task_entry, config->name, config->stack_bytes, task,
                              config->priority, &task->handle, core) != pdPASS) {
        heap_caps_free(task);
        return NEO_E_MEMORY;
    }
    /* Do not publish a RUNNING flag here: entry may already have completed. */
    *out = task;
    return NEO_OK;
}

Neo_StatusType EbOs_TaskJoinAndDelete(EbOs_TaskType *value, uint32 timeout_ms)
{
    if (value == NULL_PTR) return NEO_E_PARAM;
    if (xPortInIsrContext()) return NEO_E_STATE;
    EbOs_TaskType task = *value;
    if (task == NULL_PTR) return NEO_OK;
    if (task->handle == xTaskGetCurrentTaskHandle()) return NEO_E_STATE;
    const uint64 started = (uint64)esp_timer_get_time();
    const uint64 duration = (uint64)timeout_ms * 1000U;
    while (!atomic_load_explicit(&task->complete, memory_order_acquire) ||
           eTaskGetState(task->handle) != eSuspended) {
        if (timeout_ms != EBOS_WAIT_FOREVER && (uint64)esp_timer_get_time() - started >= duration)
            return NEO_E_TIMEOUT;
        vTaskDelay(1);
    }
    /* ESP-IDF 6.0.1 checks running-on-either-core before reporting eSuspended.
     * With no resume API, this state is stable until the owner deletes it. */
    vTaskDelete(task->handle);
    heap_caps_free(task);
    *value = NULL_PTR;
    return NEO_OK;
}

Neo_StatusType EbOs_TaskGetStackMinFree(EbOs_TaskType task, uint32 *out_bytes)
{
    if (out_bytes == NULL_PTR) return NEO_E_PARAM;
    *out_bytes = 0U;
    if (xPortInIsrContext()) return NEO_E_STATE;
    if (task == NULL_PTR) return NEO_E_PARAM;
    uint32 native_minimum = (uint32)uxTaskGetStackHighWaterMark(task->handle);
    /* Parking itself may consume more stack than the entry did. The native
     * handle remains alive until serialized Join/Delete, so sample it too. */
    *out_bytes = atomic_load_explicit(&task->complete, memory_order_acquire) &&
        task->final_stack_min_free < native_minimum ? task->final_stack_min_free : native_minimum;
    return NEO_OK;
}
