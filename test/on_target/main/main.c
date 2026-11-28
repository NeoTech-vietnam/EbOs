/* Run only on a dedicated ESP32-S3 test board: this is not camera firmware. */
#include "EbOs.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include <assert.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>

#if configNUMBER_OF_CORES != 2
#error "The lifecycle acceptance test requires the dual-core configuration"
#endif

typedef struct {
    atomic_bool stop;
    atomic_uint executions;
} TestContext;

static void immediate_entry(void *argument)
{
    TestContext *context = argument;
    atomic_fetch_add_explicit(&context->executions, 1U, memory_order_relaxed);
}

static void cooperative_entry(void *argument)
{
    TestContext *context = argument;
    while (!atomic_load_explicit(&context->stop, memory_order_acquire)) {
        atomic_fetch_add_explicit(&context->executions, 1U, memory_order_relaxed);
        EbOs_DelayOneTick();
    }
}

static EbOs_TaskConfigType settings(sint32 core, void (*entry)(void *))
{
    return (EbOs_TaskConfigType){
        .name = "EbOsAcceptance",
        .stack_bytes = 4096U,
        .priority = 5U,
        .core_affinity = core,
        .entry = entry
    };
}

static void immediate_completion(sint32 core)
{
    TestContext context;
    atomic_init(&context.stop, false);
    atomic_init(&context.executions, 0U);
    EbOs_TaskConfigType config = settings(core, immediate_entry);
    EbOs_TaskType task = NULL_PTR;
    assert(EbOs_TaskCreate(&config, &context, &task) == NEO_OK);
    uint32 stack_free = 0U;
    assert(EbOs_TaskGetStackMinFree(task, &stack_free) == NEO_OK);
    assert(stack_free > 0U && stack_free <= config.stack_bytes);
    assert(EbOs_TaskJoinAndDelete(&task, 1000U) == NEO_OK && task == NULL_PTR);
    assert(atomic_load_explicit(&context.executions, memory_order_relaxed) == 1U);
}

static void timeout_and_retry(sint32 core)
{
    TestContext context;
    atomic_init(&context.stop, false);
    atomic_init(&context.executions, 0U);
    EbOs_TaskConfigType config = settings(core, cooperative_entry);
    EbOs_TaskType task = NULL_PTR;
    assert(EbOs_TaskCreate(&config, &context, &task) == NEO_OK);
    EbOs_TaskType original = task;
    assert(EbOs_TaskJoinAndDelete(&task, 0U) == NEO_E_TIMEOUT && task == original);
    assert(EbOs_TaskJoinAndDelete(&task, 20U) == NEO_E_TIMEOUT && task == original);
    uint32 stack_free = 0U;
    assert(EbOs_TaskGetStackMinFree(task, &stack_free) == NEO_OK && stack_free > 0U);
    atomic_store_explicit(&context.stop, true, memory_order_release);
    assert(EbOs_TaskJoinAndDelete(&task, 1000U) == NEO_OK && task == NULL_PTR);
    assert(EbOs_TaskJoinAndDelete(&task, 0U) == NEO_OK);
    assert(atomic_load_explicit(&context.executions, memory_order_relaxed) > 0U);
}

void test_os_tasks_on_target(void);

void app_main(void)
{
    test_os_tasks_on_target();
    /* Warm task/kernel allocation paths before observing the retained heap. */
    (void)esp_timer_get_time();
    immediate_completion(EBOS_CORE_ANY);
    const size_t initial_heap = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    for (unsigned iteration = 0; iteration < 100U; ++iteration) {
        sint32 core = iteration % 3U == 2U ? EBOS_CORE_ANY : (sint32)(iteration % 3U);
        immediate_completion(core);
        if (iteration % 10U == 0U) timeout_and_retry(core);
    }
    const size_t final_heap = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    assert(final_heap >= initial_heap);
    printf("PASS EBOS_DUAL_CORE_LIFECYCLE: 100 returns, 10 timeout/retries; heap=%u/%u\n",
           (unsigned)initial_heap, (unsigned)final_heap);
}
