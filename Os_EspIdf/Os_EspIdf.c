/*<NeoTechHead>
 *************************************************************************
 *                              NeoTech
 *************************************************************************
 *
 * /   / /////  ///  ///// /////  ///  /   /
 * //  / /     /   /   /   /     /   / /   /
 * / / / ///   /   /   /   ///   /     /////
 * /  // /     /   /   /   /     /   / /   /
 * /   / /////  ///    /   /////  ///  /   /
 *
 *************************************************************************
 * Administrative Information
 *************************************************************************
 *
 * $Filename__:Os_EspIdf.c$
 *
 * $Author____:Nguyen Ba Vu Thach$
 *
 * $Function__:R26-11 basic-task adaptation using ESP-IDF workers.$
 *
 *************************************************************************
 * $Domain____:NeoTech$
 * $User______:Nguyen Ba Vu Thach$
 * $Date______:28.11.2026$
 * $Class_____:EbOs$
 * $Name______:Os_EspIdf$
 * $Variant___:1.2.0$
 * $Revision__:1$
 * $Type______:C$
 * $State_____:PROTOTYPE$
 *
 *************************************************************************
 * List Of Changes
 *
 * $History
 *
 * 1.2.0; 1     28.11.2026 Nguyen Ba Vu Thach
 *   Add configured activations and six basic-task services.
 *
 * $
 *
 *************************************************************************
</NeoTechHead>*/

/*
 **********************************************************************************************************************
 * Includes
 **********************************************************************************************************************
 */

#include "EbOs_Os.h"

#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <setjmp.h>

/*
 **********************************************************************************************************************
 * Defines / Macros
 **********************************************************************************************************************
 */


/*
 **********************************************************************************************************************
 * Local types
 **********************************************************************************************************************
 */

/** @brief   TRACE[NEO_EBOS_OS_RUNTIME]
 *  @details The native worker lives across activations. Outstanding activations
 *           include its current execution; notification counts cover queued work.
 *           Only this worker jumps to its own activation frame.
 */
typedef struct
{
    const EbOs_OsTaskConfigType *config;
    TaskType id;
    TaskHandle_t handle;
    uint32 activations;
    boolean active;
    boolean in_missing_end_hook;
    TaskType successor;
    jmp_buf termination;
} Os_WorkerType;

/*
 **********************************************************************************************************************
 * Static variables
 **********************************************************************************************************************
 */

static portMUX_TYPE os_lock = portMUX_INITIALIZER_UNLOCKED;
static const EbOs_OsConfigType *os_config;
static Os_WorkerType *os_workers;

/*
 **********************************************************************************************************************
 * Static function prototypes
 **********************************************************************************************************************
 */

static StatusType call_context(void);
static Os_WorkerType *current_worker(TaskHandle_t handle);
static boolean valid_config(const EbOs_OsConfigType *config);
static boolean worker_parked(TaskHandle_t handle);
static void delete_dormant_workers(Os_WorkerType *workers, uint32 count);
static void worker_entry(void *argument);
static void retire_activation(Os_WorkerType *worker, TaskType successor);

/*
 **********************************************************************************************************************
 * Local functions
 **********************************************************************************************************************
 */

/** @brief   TRACE[SWS_Os_00088], TRACE[SWS_Os_00093], TRACE[NEO_EBOS_OS_RUNTIME]
 *  @details No native ISR is registered as an AUTOSAR category-2 ISR in this
 *           profile. Interrupt masking is checked before any spinlock is entered.
 *  @return  E_OK in an enabled, running task context; otherwise a profile error.
 */
static StatusType call_context(void)
{
    if (xPortInIsrContext())
    {
        return E_OS_CALLEVEL;
    }
    if (!xPortCanYield())
    {
        return E_OS_DISABLEDINT;
    }
    if (xTaskGetSchedulerState() != taskSCHEDULER_RUNNING)
    {
        return E_OS_CALLEVEL;
    }
    return E_OK;
}

/** @brief   TRACE[NEO_EBOS_OS_RUNTIME]
 *  @details Resolve native identity while os_lock is held. An inactive dispatcher
 *           does not impersonate an executing configured task.
 *  @param[in] handle Calling native task.
 *  @return  Active worker, or NULL_PTR for an unregistered task.
 */
static Os_WorkerType *current_worker(TaskHandle_t handle)
{
    for (uint32 index = 0U; index < os_config->task_count; ++index)
    {
        if (os_workers[index].handle == handle && os_workers[index].active)
        {
            return &os_workers[index];
        }
    }
    return NULL_PTR;
}

/** @brief   TRACE[NEO_EBOS_OS_RUNTIME]
 *  @details This stage accepts fully preemptive basic C tasks with distinct
 *           native priorities on one fixed core. No project headers are consumed.
 *  @param[in] config Immutable product-owned configuration.
 *  @return  TRUE for a configuration supported by this profile.
 */
static boolean valid_config(const EbOs_OsConfigType *config)
{
    if (config == NULL_PTR || config->tasks == NULL_PTR || config->task_count == 0U ||
        config->task_count >= configMAX_PRIORITIES || config->core_affinity < 0 ||
        config->core_affinity >= configNUMBER_OF_CORES)
    {
        return FALSE;
    }
    for (uint32 index = 0U; index < config->task_count; ++index)
    {
        const EbOs_OsTaskConfigType *task = &config->tasks[index];
        if (task->name == NULL_PTR || task->name[0] == '\0' || task->entry == NULL_PTR ||
            task->stack_bytes < configMINIMAL_STACK_SIZE || task->priority == 0U ||
            task->priority >= configMAX_PRIORITIES || task->activation_limit == 0U)
        {
            return FALSE;
        }
        for (uint32 earlier = 0U; earlier < index; ++earlier)
        {
            if (task->priority == config->tasks[earlier].priority)
            {
                return FALSE;
            }
        }
    }
    return TRUE;
}

/** @brief   TRACE[NEO_EBOS_OS_RUNTIME]
 *  @details A worker without pending activations can only block at its own
 *           notification gate. ESP-IDF checks execution on either core before
 *           reporting a blocked/suspended state.
 *  @param[in] handle Dormant native worker.
 *  @return  TRUE when deletion cannot race with worker stack execution.
 */
static boolean worker_parked(TaskHandle_t handle)
{
    eTaskState state = eTaskGetState(handle);
    return state == eBlocked || state == eSuspended;
}

/** @brief   TRACE[NEO_EBOS_OS_RUNTIME]
 *  @details Roll back failed initialization. Workers have never received an
 *           activation, so they reach their gate without executing product code.
 *  @param[in] workers Private allocation not yet published to service callers.
 *  @param[in] count Number of native workers successfully created.
 *  @return  None.
 */
static void delete_dormant_workers(Os_WorkerType *workers, uint32 count)
{
    for (uint32 index = 0U; index < count; ++index)
    {
        while (!worker_parked(workers[index].handle))
        {
            vTaskDelay(1);
        }
        vTaskDelete(workers[index].handle);
    }
    heap_caps_free(workers);
}

/** @brief   TRACE[SWS_Os_00001], TRACE[SRS_Os_00097], TRACE[NEO_EBOS_OS_RUNTIME]
 *  @details Atomically complete an activation and, when requested, make its
 *           successor ready. Self-chaining replaces rather than adds a request.
 *           The caller holds os_lock; native notifications are deferred until
 *           after the nonlocal exit has abandoned the product call frames.
 *  @param[in] worker Current active worker.
 *  @param[in] successor Next configured task, or INVALID_TASK for termination.
 *  @return  None.
 */
static void retire_activation(Os_WorkerType *worker, TaskType successor)
{
    if (successor != worker->id)
    {
        --worker->activations;
        if (successor != INVALID_TASK)
        {
            ++os_workers[successor].activations;
        }
    }
    worker->active = FALSE;
    worker->successor = successor;
}

/** @brief   TRACE[SWS_Os_00001], TRACE[SWS_Os_00052], TRACE[SWS_Os_00069],
 *           TRACE[NEO_EBOS_OS_RUNTIME]
 *  @details Reuse the native stack while restarting each configured C entry from
 *           its first instruction. Returning entries invoke the optional product
 *           diagnostic callback before retirement. This is not full ErrorHook.
 *  @param[in] argument Fully initialized worker metadata.
 *  @return  Never returns; a serialized owner deletes a dormant worker.
 */
static void worker_entry(void *argument)
{
    Os_WorkerType *worker = argument;
    for (;;)
    {
        (void)ulTaskNotifyTake(pdFALSE, portMAX_DELAY);
        portENTER_CRITICAL(&os_lock);
        worker->active = TRUE;
        worker->successor = INVALID_TASK;
        portEXIT_CRITICAL(&os_lock);

        if (setjmp(worker->termination) == 0)
        {
            worker->config->entry();
            portENTER_CRITICAL(&os_lock);
            worker->in_missing_end_hook = TRUE;
            portEXIT_CRITICAL(&os_lock);
            if (os_config->missing_end_hook != NULL_PTR)
            {
                os_config->missing_end_hook(worker->id, E_OS_MISSINGEND);
            }
            portENTER_CRITICAL(&os_lock);
            worker->in_missing_end_hook = FALSE;
            retire_activation(worker, INVALID_TASK);
            portEXIT_CRITICAL(&os_lock);
        }
        /* Only metadata reached through the unchanged worker pointer is read
         * after longjmp. No modified automatic local survives the jump. */
        if (worker->successor != INVALID_TASK)
        {
            (void)xTaskNotifyGive(os_workers[worker->successor].handle);
        }
    }
}

/*
 **********************************************************************************************************************
 * Public functions
 **********************************************************************************************************************
 */

/** @brief   TRACE[NEO_EBOS_OS_RUNTIME]
 *  @details Validate all product configuration, create dormant native workers,
 *           then publish the complete runtime. This is not AUTOSAR StartOS.
 *  @param[in] config Immutable configuration retained until successful deinit.
 *  @return  NEO_OK, NEO_E_PARAM, NEO_E_STATE or NEO_E_MEMORY.
 */
Neo_StatusType EbOs_OsInit(const EbOs_OsConfigType *config)
{
    if (call_context() != E_OK)
    {
        return NEO_E_STATE;
    }
    portENTER_CRITICAL(&os_lock);
    boolean initialized = os_config != NULL_PTR;
    portEXIT_CRITICAL(&os_lock);
    if (initialized)
    {
        return NEO_E_STATE;
    }
    if (!valid_config(config))
    {
        return NEO_E_PARAM;
    }
    Os_WorkerType *workers = heap_caps_calloc(config->task_count, sizeof(*workers),
                                             MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (workers == NULL_PTR)
    {
        return NEO_E_MEMORY;
    }
    for (uint32 index = 0U; index < config->task_count; ++index)
    {
        workers[index].config = &config->tasks[index];
        workers[index].id = index;
        workers[index].successor = INVALID_TASK;
    }
    for (uint32 index = 0U; index < config->task_count; ++index)
    {
        const EbOs_OsTaskConfigType *task = workers[index].config;
        if (xTaskCreatePinnedToCore(worker_entry, task->name, task->stack_bytes, &workers[index],
                                  task->priority, &workers[index].handle,
                                  config->core_affinity) != pdPASS)
        {
            delete_dormant_workers(workers, index);
            return NEO_E_MEMORY;
        }
    }
    portENTER_CRITICAL(&os_lock);
    os_workers = workers;
    os_config = config;
    portEXIT_CRITICAL(&os_lock);
    return NEO_OK;
}

/** @brief   TRACE[NEO_EBOS_OS_RUNTIME]
 *  @details Delete only workers already dormant at their notification gate.
 *           Lifecycle calls must be serialized with every task service call.
 *           A busy result retains the complete runtime for a later retry.
 *  @return  NEO_OK when uninitialized/deleted, NEO_E_STATE for wrong context,
 *           or NEO_E_BUSY until every activation and dispatcher has retired.
 */
Neo_StatusType EbOs_OsDeinit(void)
{
    if (call_context() != E_OK)
    {
        return NEO_E_STATE;
    }
    portENTER_CRITICAL(&os_lock);
    if (os_config == NULL_PTR)
    {
        portEXIT_CRITICAL(&os_lock);
        return NEO_OK;
    }
    for (uint32 index = 0U; index < os_config->task_count; ++index)
    {
        if (os_workers[index].activations != 0U || os_workers[index].active)
        {
            portEXIT_CRITICAL(&os_lock);
            return NEO_E_BUSY;
        }
    }
    portEXIT_CRITICAL(&os_lock);
    for (uint32 index = 0U; index < os_config->task_count; ++index)
    {
        if (!worker_parked(os_workers[index].handle))
        {
            return NEO_E_BUSY;
        }
    }
    Os_WorkerType *workers = os_workers;
    uint32 count = os_config->task_count;
    portENTER_CRITICAL(&os_lock);
    os_config = NULL_PTR;
    os_workers = NULL_PTR;
    portEXIT_CRITICAL(&os_lock);
    delete_dormant_workers(workers, count);
    return NEO_OK;
}

/** @brief   TRACE[SWS_Os_00001], TRACE[SRS_Os_00097], TRACE[SWS_Os_00093]
 *  @details Queue a configured activation without allocating or creating a task.
 */
StatusType ActivateTask(TaskType TaskID)
{
    StatusType status = call_context();
    if (status != E_OK)
    {
        return status;
    }
    portENTER_CRITICAL(&os_lock);
    if (os_config == NULL_PTR)
    {
        status = E_OS_STATE;
    }
    else if (TaskID >= os_config->task_count)
    {
        status = E_OS_ID;
    }
    else if (os_workers[TaskID].activations >= os_workers[TaskID].config->activation_limit)
    {
        status = E_OS_LIMIT;
    }
    else
    {
        ++os_workers[TaskID].activations;
    }
    portEXIT_CRITICAL(&os_lock);
    if (status == E_OK)
    {
        (void)xTaskNotifyGive(os_workers[TaskID].handle);
    }
    return status;
}

/** @brief   TRACE[SWS_Os_00001], TRACE[SRS_Os_00097], TRACE[SWS_Os_00093]
 *  @details A successful service abandons all configured C task call frames.
 *           No native worker is deleted. Resources/spinlocks are unsupported.
 */
StatusType TerminateTask(void)
{
    StatusType status = call_context();
    if (status != E_OK)
    {
        return status;
    }
    TaskHandle_t handle = xTaskGetCurrentTaskHandle();
    portENTER_CRITICAL(&os_lock);
    Os_WorkerType *worker = os_config == NULL_PTR ? NULL_PTR : current_worker(handle);
    if (worker == NULL_PTR || worker->in_missing_end_hook)
    {
        status = os_config == NULL_PTR ? E_OS_STATE : E_OS_CALLEVEL;
        portEXIT_CRITICAL(&os_lock);
        return status;
    }
    retire_activation(worker, INVALID_TASK);
    portEXIT_CRITICAL(&os_lock);
    longjmp(worker->termination, 1);
}

/** @brief   TRACE[SWS_Os_00001], TRACE[SRS_Os_00097], TRACE[SWS_Os_00093]
 *  @details Capacity checking and caller retirement form one atomic transition.
 *           Self-chaining replaces the current activation even at its limit.
 */
StatusType ChainTask(TaskType TaskID)
{
    StatusType status = call_context();
    if (status != E_OK)
    {
        return status;
    }
    TaskHandle_t handle = xTaskGetCurrentTaskHandle();
    portENTER_CRITICAL(&os_lock);
    Os_WorkerType *worker = os_config == NULL_PTR ? NULL_PTR : current_worker(handle);
    if (worker == NULL_PTR || worker->in_missing_end_hook)
    {
        status = os_config == NULL_PTR ? E_OS_STATE : E_OS_CALLEVEL;
    }
    else if (TaskID >= os_config->task_count)
    {
        status = E_OS_ID;
    }
    else if (TaskID != worker->id &&
             os_workers[TaskID].activations >= os_workers[TaskID].config->activation_limit)
    {
        status = E_OS_LIMIT;
    }
    if (status != E_OK)
    {
        portEXIT_CRITICAL(&os_lock);
        return status;
    }
    retire_activation(worker, TaskID);
    portEXIT_CRITICAL(&os_lock);
    longjmp(worker->termination, 1);
}

/** @brief   TRACE[SWS_Os_00001], TRACE[SRS_Os_00097], TRACE[SWS_Os_00093]
 *  @details Supported tasks are fully preemptive and own no internal resource;
 *           consequently this call has no additional scheduling effect.
 */
StatusType Schedule(void)
{
    StatusType status = call_context();
    if (status != E_OK)
    {
        return status;
    }
    TaskHandle_t handle = xTaskGetCurrentTaskHandle();
    portENTER_CRITICAL(&os_lock);
    Os_WorkerType *worker = os_config == NULL_PTR ? NULL_PTR : current_worker(handle);
    if (worker == NULL_PTR || worker->in_missing_end_hook)
    {
        status = os_config == NULL_PTR ? E_OS_STATE : E_OS_CALLEVEL;
    }
    portEXIT_CRITICAL(&os_lock);
    return status;
}

/** @brief   TRACE[SWS_Os_00001], TRACE[SRS_Os_00097], TRACE[SWS_Os_00093]
 *  @details An SDK task outside the registry has no configured task identity.
 */
StatusType GetTaskID(TaskRefType TaskID)
{
    StatusType status = call_context();
    if (status != E_OK)
    {
        return status;
    }
    if (TaskID == NULL_PTR)
    {
        return E_OS_ILLEGAL_ADDRESS;
    }
    TaskHandle_t handle = xTaskGetCurrentTaskHandle();
    portENTER_CRITICAL(&os_lock);
    if (os_config == NULL_PTR)
    {
        status = E_OS_STATE;
    }
    else
    {
        Os_WorkerType *worker = current_worker(handle);
        *TaskID = worker == NULL_PTR ? INVALID_TASK : worker->id;
    }
    portEXIT_CRITICAL(&os_lock);
    return status;
}

/** @brief   TRACE[SWS_Os_00001], TRACE[SRS_Os_00097], TRACE[SWS_Os_00093]
 *  @details Report logical activation state, not the dormant worker's lifetime.
 *           Native blocking by product basic-task code is unsupported; WAITING
 *           diagnoses such blocking rather than offering extended-task events.
 */
StatusType GetTaskState(TaskType TaskID, TaskStateRefType State)
{
    StatusType status = call_context();
    if (status != E_OK)
    {
        return status;
    }
    if (State == NULL_PTR)
    {
        return E_OS_ILLEGAL_ADDRESS;
    }
    portENTER_CRITICAL(&os_lock);
    if (os_config == NULL_PTR)
    {
        status = E_OS_STATE;
    }
    else if (TaskID >= os_config->task_count)
    {
        status = E_OS_ID;
    }
    else
    {
        Os_WorkerType *worker = &os_workers[TaskID];
        if (worker->activations == 0U)
        {
            *State = SUSPENDED;
        }
        else if (!worker->active)
        {
            *State = READY;
        }
        else
        {
            eTaskState native_state = eTaskGetState(worker->handle);
            *State = native_state == eRunning ? RUNNING :
                native_state == eReady ? READY : WAITING;
        }
    }
    portEXIT_CRITICAL(&os_lock);
    return status;
}
