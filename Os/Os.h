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
 * $Function__:AUTOSAR CP R26-11 core OS service contracts; declarations only.$
 *
 *************************************************************************
 * $Domain____:NeoTech$
 * $User______:Nguyen Ba Vu Thach$
 * $Date______:28.11.2026$
 * $Class_____:EbOs$
 * $Name_____:Os$
 * $Variant___:1.1.0$
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

/** @brief   TRACE[SWS_Os_00093]
 *  @details Calls returning StatusType also have the R26-11 interrupt-state
 *           restriction: a prohibited call is ignored with E_OS_DISABLEDINT.
 *           This is a specified contract, not behavior provided by EbOs today.
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

/** @brief   TRACE[SWS_Os_00001], TRACE[SRS_Os_00097]
 *  @details Activate a configured task; origin: OSEK OS 2.2.3 section 13.2.3.1.
 *           This does not create a native task.
 *  @param[in] TaskID Configured task identifier.
 *  @return  E_OK or E_OS_LIMIT; extended base check: E_OS_ID.
 *  @pre     Call context must satisfy R26-11 SWS OS section 7.7.3.3.
 *  @note    R26-11 refinements are catalogued separately. Specified, not implemented.
 */
StatusType ActivateTask(TaskType TaskID);

/** @brief   TRACE[SWS_Os_00001], TRACE[SRS_Os_00097], TRACE[SWS_Os_00612]
 *  @details End the current activation; OSEK section 13.2.3.2.
 *  @return  Success does not return. Extended base errors: E_OS_RESOURCE,
 *           E_OS_CALLEVEL; R26-11 adds E_OS_SPINLOCK when a spinlock is held.
 *  @pre     Task context; release resources and spinlocks before termination.
 *  @note    Specified, not implemented. Returning from an EbOs entry is not this service.
 */
StatusType TerminateTask(void);

/** @brief   TRACE[SWS_Os_00001], TRACE[SRS_Os_00097], TRACE[SWS_Os_00612]
 *  @details End this activation and activate its successor; OSEK section 13.2.3.3.
 *  @param[in] TaskID Successor task identifier.
 *  @return  Success does not return. Base error: E_OS_LIMIT; extended base errors:
 *           E_OS_ID, E_OS_RESOURCE, E_OS_CALLEVEL; R26-11 adds E_OS_SPINLOCK.
 *  @pre     Task context; release resources and spinlocks before chaining.
 *  @note    Specified, not implemented; no dynamic create/delete alias is supplied.
 */
StatusType ChainTask(TaskType TaskID);

/** @brief   TRACE[SWS_Os_00001], TRACE[SRS_Os_00097], TRACE[SWS_Os_00624]
 *  @details Explicit scheduling point; origin: OSEK section 13.2.3.4.
 *  @return  E_OK; extended base errors: E_OS_CALLEVEL, E_OS_RESOURCE.
 *           R26-11 adds E_OS_SPINLOCK if a spinlock remains occupied.
 *  @pre     Task context; resource restrictions of the service apply.
 *  @note    Specified, not implemented; it is not a FreeRTOS yield alias.
 */
StatusType Schedule(void);

/** @brief   TRACE[SWS_Os_00001], TRACE[SRS_Os_00097]
 *  @details Read the current/interrupted task identity; OSEK section 13.2.3.5.
 *  @param[out] TaskID Task identifier, or INVALID_TASK when none is running.
 *  @return  Base service: E_OK. R26-11 call-context/interrupt restrictions also apply.
 *  @pre     Valid output storage and a calling context allowed by SWS OS table 7.1.
 *  @note    Specified, not implemented. R26-11 section 7.9.8 defines the local-core view.
 */
StatusType GetTaskID(TaskRefType TaskID);

/** @brief   TRACE[SWS_Os_00001], TRACE[SRS_Os_00097]
 *  @details Read a task-state snapshot; origin: OSEK section 13.2.3.6.
 *  @param[in] TaskID Configured task identifier.
 *  @param[out] State State snapshot; concurrent execution may change it afterward.
 *  @return  E_OK; extended base error: E_OS_ID. Applicable R26-11 checks remain specified.
 *  @pre     Valid output storage and a calling context allowed by SWS OS table 7.1.
 *  @note    Specified, not implemented; native task states are not exposed here.
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
