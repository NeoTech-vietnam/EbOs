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
 * $Filename__:EbOs.h$
 *
 * $Author____:Nguyen Ba Vu Thach$
 *
 * $Function__:NeoTech native-task, mutex and delay extension contracts.$
 *
 *************************************************************************
 * $Domain____:NeoTech$
 * $User______:Nguyen Ba Vu Thach$
 * $Date______:28.11.2026$
 * $Class_____:EbOs$
 * $Name______:EbOs$
 * $Variant___:1.0.0$
 * $Revision__:3$
 * $Type______:H$
 * $State_____:PROTOTYPE$
 *
 *************************************************************************
 * List Of Changes
 *
 * $History
 *
 * 1.0.0; 1     21.11.2026 Nguyen Ba Vu Thach
 *   Initial independent module with mutex and delay interfaces.
 * 1.0.0; 2     28.11.2026 Nguyen Ba Vu Thach
 *   Add configured native tasks and cooperative retirement.
 * 1.0.0; 3     28.11.2026 Nguyen Ba Vu Thach
 *   Apply NeoTech source layout and extension requirement trace comments.
 *
 * $
 *
 *************************************************************************
</NeoTechHead>*/

#ifndef EBOS_H
#define EBOS_H

/*
 **********************************************************************************************************************
 * Includes
 **********************************************************************************************************************
 */

#include "Neo_Types.h"

/*
 **********************************************************************************************************************
 * Defines / Macros
 **********************************************************************************************************************
 */

/** @brief   TRACE[NEO_EBOS_00013]
 *  @details Selects an indefinite native mutex wait or unbounded managed-task join.
 *           This NeoTech sentinel is not an AUTOSAR OS time or status value.
 */
#define EBOS_WAIT_FOREVER UINT32_MAX

/** @brief   TRACE[NEO_EBOS_00014]
 *  @details Requests an unpinned native task; nonnegative values select an available core.
 *           Affinity is a NeoTech task-configuration extension.
 */
#define EBOS_CORE_ANY (-1)

/*
 **********************************************************************************************************************
 * Types
 **********************************************************************************************************************
 */

/** @brief   TRACE[NEO_EBOS_00010]
 *  @details Opaque non-recursive, task-owned mutex. Keep alive while users can access it.
 */
typedef struct EbOs_Mutex *EbOs_MutexType;

/** @brief   TRACE[NEO_EBOS_00011]
 *  @details Opaque dynamically created native task, distinct from AUTOSAR TaskType.
 *           The serialized owner retires it through EbOs_TaskJoinAndDelete.
 */
typedef struct EbOs_Task *EbOs_TaskType;

/** @brief   TRACE[NEO_EBOS_00012]
 *  @details Product task settings and entry binding. Stack size uses bytes and priority
 *           uses the native FreeRTOS scale. This does not define AUTOSAR activation semantics.
 */
typedef struct
{
    const char *name;                 /**< Diagnostic name; not null or empty. */
    uint32 stack_bytes;               /**< Native stack bytes; at least the backend minimum. */
    uint32 priority;                  /**< Greater than zero; below the backend priority limit. */
    sint32 core_affinity;             /**< EBOS_CORE_ANY or an available nonnegative core index. */
    void (*entry)(void *argument);    /**< Cooperative entry; no self-delete or self-suspend. */
} EbOs_TaskConfigType;

/*
 **********************************************************************************************************************
 * Extern declarations
 **********************************************************************************************************************
 */


/*
 **********************************************************************************************************************
 * Function prototypes
 **********************************************************************************************************************
 */

/** @brief   TRACE[NEO_EBOS_00001]
 *  @details Creates the opaque wrapper and native non-recursive mutex.
 *           Creation/deletion require serialized lifecycle ownership and quiescent users.
 *  @param[out] out Created handle, or NULL_PTR after allocation failure.
 *  @return  NEO_OK; NEO_E_PARAM for null output; NEO_E_MEMORY on allocation failure.
 *  @pre     Called from task context with serialized lifecycle ownership.
 *  @note    NeoTech extension; it does not create an AUTOSAR resource.
 */
Neo_StatusType EbOs_MutexCreate(EbOs_MutexType *out);

/** @brief   TRACE[NEO_EBOS_00002]
 *  @details Takes the live mutex. Finite milliseconds round up to ticks and saturate
 *           below the indefinite-wait value. Zero requests a nonblocking take.
 *  @param[in] mutex Live mutex handle.
 *  @param[in] timeout_ms Finite milliseconds or EBOS_WAIT_FOREVER.
 *  @return  NEO_OK; NEO_E_PARAM for null handle; NEO_E_TIMEOUT if not acquired.
 *  @pre     Task context. Keep the mutex alive throughout the operation.
 *  @note    Tasks may contend. Priority inheritance is not AUTOSAR priority ceiling.
 */
Neo_StatusType EbOs_MutexLock(EbOs_MutexType mutex, uint32 timeout_ms);

/** @brief   TRACE[NEO_EBOS_00003]
 *  @details Releases a mutex owned by the current task. A null handle is ignored.
 *           The native release result is not propagated by this compatibility interface.
 *  @param[in] mutex Live handle owned by the current task, or NULL_PTR.
 *  @return  None.
 *  @pre     Task context; deletion cannot run concurrently with release.
 */
void EbOs_MutexUnlock(EbOs_MutexType mutex);

/** @brief   TRACE[NEO_EBOS_00004]
 *  @details Deletes a native mutex and its wrapper. A null handle is ignored.
 *  @param[in] mutex Quiescent handle, or NULL_PTR.
 *  @return  None.
 *  @pre     Task context; the serialized owner has quiesced all users and waiters.
 */
void EbOs_MutexDelete(EbOs_MutexType mutex);

/** @brief   TRACE[NEO_EBOS_00005]
 *  @details Delays the current task using upward-rounded finite ticks, saturated
 *           below portMAX_DELAY. Zero retains the native zero-tick behavior.
 *  @param[in] ms Duration in milliseconds.
 *  @return  None.
 *  @pre     Task context. Different tasks may delay independently.
 */
void EbOs_Delay(uint32 ms);

/** @brief   TRACE[NEO_EBOS_00006]
 *  @details Delays for one actual scheduler tick without millisecond conversion.
 *           The tick duration depends on the product scheduler frequency.
 *  @return  None.
 *  @pre     Task context. Different tasks may delay independently.
 */
void EbOs_DelayOneTick(void);

/** @brief   TRACE[NEO_EBOS_00007]
 *  @details Validates product settings and creates a native task with private metadata.
 *           Entry may already be complete on return. This is dynamic task creation,
 *           not AUTOSAR ActivateTask for a statically configured task.
 *  @param[in] config Product configuration and entry binding.
 *  @param[in] argument Opaque entry argument.
 *  @param[out] out Created handle; cleared before validation/allocation errors.
 *  @return  NEO_OK; NEO_E_PARAM for invalid configuration/output; NEO_E_STATE in ISR
 *           context; NEO_E_MEMORY for metadata/native task allocation failure.
 *  @pre     Serialize creation, join/deletion and stack sampling for the handle.
 *           Keep configuration and argument data alive until entry returns. Entry must
 *           return cooperatively; it must not delete or suspend itself.
 *  @note    No partial handle is published on failure. No cancellation API is provided.
 */
Neo_StatusType EbOs_TaskCreate(const EbOs_TaskConfigType *config, void *argument, EbOs_TaskType *out);

/** @brief   TRACE[NEO_EBOS_00008]
 *  @details Waits for entry completion and confirmed suspension, deletes the native
 *           task and metadata, and clears the owned handle. Timeout preserves the handle
 *           and dependencies for retry. A null owned handle is already joined.
 *  @param[in,out] task Address of the owner handle.
 *  @param[in] timeout_ms Finite milliseconds or EBOS_WAIT_FOREVER.
 *  @return  NEO_OK; NEO_E_PARAM for null handle address; NEO_E_STATE in ISR context
 *           or on self-join; NEO_E_TIMEOUT before safe retirement.
 *  @pre     Task context and serialized lifecycle ownership; no concurrent sampling.
 *  @note    Running work is not force-deleted. This is not AUTOSAR TerminateTask.
 */
Neo_StatusType EbOs_TaskJoinAndDelete(EbOs_TaskType *task, uint32 timeout_ms);

/** @brief   TRACE[NEO_EBOS_00009]
 *  @details Reports the minimum unused native stack in bytes. After entry return,
 *           the lower of the entry-return snapshot and current native watermark includes
 *           additional stack used by the suspension path.
 *  @param[in] task Live handle, including a completed task not yet joined.
 *  @param[out] out_bytes Minimum unused stack, or zero after a checked error.
 *  @return  NEO_OK; NEO_E_PARAM for null handle/output; NEO_E_STATE in ISR context.
 *  @pre     Task context; serialize sampling against creation and join/deletion.
 */
Neo_StatusType EbOs_TaskGetStackMinFree(EbOs_TaskType task, uint32 *out_bytes);

#endif /* EBOS_H */
