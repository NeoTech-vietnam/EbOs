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
 * $Filename__:Os.h$
 *
 * $Author____:Nguyen Ba Vu Thach$
 *
 * $Function__:R26-11 task services for the selected ESP-IDF basic-task profile.$
 *
 *************************************************************************
 * $Domain____:NeoTech$
 * $User______:Nguyen Ba Vu Thach$
 * $Date______:28.11.2026$
 * $Class_____:EbOs$
 * $Name_____:Os$
 * $Variant___:1.2.0$
 * $Revision__:1$
 * $Type_____:H$
 * $State_____:DRAFT$
 *
 *************************************************************************
 * List Of Changes
 *
 * $History
 *
 * 1.1.0; 1     28.11.2026 Nguyen Ba Vu Thach
 *   Define the R26-11 core task/lifecycle declaration profile.
 * 1.2.0; 1     28.11.2026 Nguyen Ba Vu Thach
 *   Implement the selected basic-task profile; retain lifecycle contracts.
 *
 * $
 *
 *************************************************************************
</NeoTechHead>*/

#ifndef OS_H
#define OS_H

/*
 **********************************************************************************************************************
 * Includes
 **********************************************************************************************************************
 */

#include "Os_Types.h"

/*
 **********************************************************************************************************************
 * Defines / Macros
 **********************************************************************************************************************
 */

/** @brief   TRACE[SWS_Os_00093], TRACE[SWS_Os_00088]
 *  @details Calls returning StatusType also have the R26-11 interrupt-state
 *           restriction: a prohibited call is ignored with E_OS_DISABLEDINT.
 *           The task profile checks masked task interrupts before any state change.
 *           Native ISR calls are rejected until a category 2 adapter exists.
 */

/*
 **********************************************************************************************************************
 * Types
 **********************************************************************************************************************
 */


/*
 **********************************************************************************************************************
 * Extern declarations
 **********************************************************************************************************************
 */

#ifdef __cplusplus
extern "C" {
#endif

/*
 **********************************************************************************************************************
 * Function prototypes
 **********************************************************************************************************************
 */

/** @brief   TRACE[SWS_Os_00001], TRACE[SRS_Os_00097], TRACE[NEO_EBOS_OS_RUNTIME]
 *  @details Queue one configured basic-task activation; inherited OSEK section
 *           13.2.3.1. The limit includes the executing activation and pending work.
 *           No native worker is created here. Limit failure changes no state.
 *  @param[in] TaskID Configured task-table index.
 *  @return  E_OK, E_OS_LIMIT, E_OS_ID; profile checks may return E_OS_STATE,
 *           E_OS_CALLEVEL or E_OS_DISABLEDINT.
 *  @pre     Initialized profile; task context with interrupts enabled. Native
 *           ISR entry, resources, events and protected OS-Applications are outside
 *           this implementation profile. See EbOs_Os.h for lifecycle restrictions.
 */
StatusType ActivateTask(TaskType TaskID);

/** @brief   TRACE[SWS_Os_00001], TRACE[SRS_Os_00097], TRACE[SWS_Os_00612]
 *  @details End this configured activation; inherited OSEK section 13.2.3.2.
 *           Successful termination never returns, including from nested C helpers.
 *           The native worker remains allocated for later activations.
 *  @return  Success does not return; E_OS_CALLEVEL, E_OS_STATE or E_OS_DISABLEDINT.
 *  @pre     Active configured C task outside the missing-end diagnostic hook.
 *           Release every native lock first. Resource/spinlock services and their
 *           E_OS_RESOURCE/E_OS_SPINLOCK checks remain outside the current profile.
 */
StatusType TerminateTask(void);

/** @brief   TRACE[SWS_Os_00001], TRACE[SRS_Os_00097], TRACE[SWS_Os_00612]
 *  @details Atomically retire this activation and queue its successor; inherited
 *           OSEK section 13.2.3.3. Self-chain replaces the current activation and
 *           cannot exceed its own limit. Failed chaining leaves the caller active.
 *  @param[in] TaskID Configured successor index.
 *  @return  Success does not return; E_OS_LIMIT, E_OS_ID, E_OS_CALLEVEL,
 *           E_OS_STATE or E_OS_DISABLEDINT.
 *  @pre     Active configured C task, outside the diagnostic hook, no held locks.
 *           Resource/spinlock checks and OS-Application access are outside this profile.
 */
StatusType ChainTask(TaskType TaskID);

/** @brief   TRACE[SWS_Os_00001], TRACE[SRS_Os_00097], TRACE[SWS_Os_00624]
 *  @details Explicit scheduling point; inherited OSEK section 13.2.3.4. This
 *           fully preemptive profile has no internal resources to release, so
 *           after context checks this service returns E_OK without a native yield.
 *  @return  E_OK, E_OS_CALLEVEL, E_OS_STATE or E_OS_DISABLEDINT.
 *  @pre     Active configured task outside the diagnostic hook, no held locks.
 *           Non-preemptive tasks and resource/spinlock checks are not supported.
 */
StatusType Schedule(void);

/** @brief   TRACE[SWS_Os_00001], TRACE[SRS_Os_00097], TRACE[SWS_Os_00566]
 *  @details Read the current configured activation identity; inherited OSEK
 *           section 13.2.3.5. An unregistered SDK task receives INVALID_TASK.
 *           Native ISR queries are rejected; interrupted-task lookup needs a
 *           future category 2 adapter. Writable-address protection is not supplied.
 *  @param[out] TaskID Current configured task, or INVALID_TASK.
 *  @return  E_OK, E_OS_ILLEGAL_ADDRESS for NULL, or profile E_OS_STATE,
 *           E_OS_CALLEVEL or E_OS_DISABLEDINT. Errors leave output unchanged.
 *  @pre     Valid writable output storage; initialized profile and task context.
 */
StatusType GetTaskID(TaskRefType TaskID);

/** @brief   TRACE[SWS_Os_00001], TRACE[SRS_Os_00097], TRACE[SWS_Os_00566]
 *  @details Read a configured task-state snapshot; inherited OSEK section 13.2.3.6.
 *           Pending work is READY, native execution of an activation is RUNNING,
 *           and no outstanding work is SUSPENDED. WAITING diagnoses unsupported
 *           native blocking; this profile supplies no extended-task event service.
 *  @param[in] TaskID Configured task-table index.
 *  @param[out] State State snapshot; execution may change it after this call.
 *  @return  E_OK, E_OS_ID, E_OS_ILLEGAL_ADDRESS for NULL, or profile E_OS_STATE,
 *           E_OS_CALLEVEL or E_OS_DISABLEDINT. Errors leave output unchanged.
 *  @pre     Valid writable output storage; initialized profile and task context.
 */
StatusType GetTaskState(TaskType TaskID, TaskStateRefType State);

/** @brief   TRACE[SWS_Os_00001], TRACE[SRS_Os_00097], TRACE[SWS_Os_00424]
 *  @details Enter OS execution in Mode; origin: OSEK section 13.7.2.2.
 *           R26-11 requires the first start call not to return.
 *  @param[in] Mode Configured application mode; see OSDEFAULTAPPMODE and DONOTCARE.
 *  @return  None.
 *  @pre     Startup context outside OS execution; R26-11 startup rules apply.
 *  @note    Specified, not implemented. ESP-IDF's running scheduler is not StartOS.
 */
void StartOS(AppModeType Mode);

/** @brief   TRACE[SWS_Os_00001], TRACE[SRS_Os_00097],
 *           TRACE[SWS_Os_00425] & TRACE[SWS_Os_00617]
 *  @details Shut down the calling core; origin: OSEK section 13.7.2.3.
 *           After a returning ShutdownHook, R26-11 requires disabled interrupts
 *           and an endless loop.
 *  @param[in] Error Shutdown reason.
 *  @return  None; the specified shutdown path does not return.
 *  @pre     Calling context and OS-Application permissions must permit shutdown.
 *  @note    Specified, not implemented. It does not shut down every core.
 */
void ShutdownOS(StatusType Error);

/** @brief   TRACE[SWS_Os_00001], TRACE[SRS_Os_00097]
 *  @details Obtain the selected application mode; OSEK section 13.7.2.1.
 *  @return  Current application mode.
 *  @pre     Calling context must satisfy R26-11 SWS OS table 7.1.
 *  @note    Specified, not implemented; no synthetic mode is returned by EbOs.
 */
AppModeType GetActiveApplicationMode(void);

#ifdef __cplusplus
}
#endif

#endif /* OS_H */
