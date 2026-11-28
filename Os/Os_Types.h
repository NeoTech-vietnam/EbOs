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
 * $Filename__:Os_Types.h$
 *
 * $Author____:Nguyen Ba Vu Thach$
 *
 * $Function__:R26-11 OS types for the selected ESP-IDF basic-task profile.$
 *
 *************************************************************************
 * $Domain____:NeoTech$
 * $User______:Nguyen Ba Vu Thach$
 * $Date______:28.11.2026$
 * $Class_____:EbOs$
 * $Name_____:Os_Types$
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
 *   Add missing-end and illegal-address errors for the task profile.
 *
 * $
 *
 *************************************************************************
</NeoTechHead>*/

#ifndef OS_TYPES_H
#define OS_TYPES_H

/*
 **********************************************************************************************************************
 * Includes
 **********************************************************************************************************************
 */

#include "Std_Types.h"

#ifndef STATUSTYPEDEFINED
#error "The OS contract requires CUCEL Std_Types.h with StatusType and STATUSTYPEDEFINED."
#endif

/*
 **********************************************************************************************************************
 * Defines / Macros
 **********************************************************************************************************************
 */

/** @brief   TRACE[SWS_Os_00001], TRACE[SRS_Os_00097]
 *  @details Selected OSEK-compatible profile values; see OSEK OS 2.2.3,
 *           section 13.1. E_OK and StatusType are supplied by CUCEL.
 *           Only the documented basic-task profile is implemented.
 */
#define E_OS_ACCESS   ((StatusType)1U)
#define E_OS_CALLEVEL ((StatusType)2U)
#define E_OS_ID       ((StatusType)3U)
#define E_OS_LIMIT    ((StatusType)4U)
#define E_OS_NOFUNC   ((StatusType)5U)
#define E_OS_RESOURCE ((StatusType)6U)
#define E_OS_STATE    ((StatusType)7U)
#define E_OS_VALUE    ((StatusType)8U)

/** @brief   TRACE[SWS_Os_91025], TRACE[NEO_EBOS_OS_TYPES]
 *  @details R26-11-specific error values are implementation assigned.
 *           This profile reserves values above the inherited OSEK range.
 */
#define E_OS_DISABLEDINT ((StatusType)0x10U)
#define E_OS_SPINLOCK    ((StatusType)0x11U)
#define E_OS_MISSINGEND  ((StatusType)0x12U)
#define E_OS_ILLEGAL_ADDRESS ((StatusType)0x13U)

/** @brief   TRACE[NEO_EBOS_OS_TYPES]
 *  @details OSEK section 13.2.4 supplies the state names. Numeric encodings
 *           are NeoTech profile choices, not AUTOSAR-mandated values.
 */
#define RUNNING   ((TaskStateType)0U)
#define READY     ((TaskStateType)1U)
#define WAITING   ((TaskStateType)2U)
#define SUSPENDED ((TaskStateType)3U)

/** @brief   TRACE[NEO_EBOS_OS_TYPES]
 *  @details The profile reserves the greatest TaskType value for no task.
 *           OSEK section 13.2.4 specifies its role, not this encoding.
 */
#define INVALID_TASK ((TaskType)0xFFFFFFFFUL)

/** @brief   TRACE[NEO_EBOS_OS_TYPES]
 *  @details Default-mode value selected for this declaration profile.
 *           OSEK section 13.7.3 defines the constant's role.
 */
#define OSDEFAULTAPPMODE ((AppModeType)0U)

/** @brief   TRACE[SWS_Os_91007], TRACE[NEO_EBOS_OS_TYPES]
 *  @details A core may inherit another core's application mode. The value
 *           below is NeoTech's encoding; multicore startup is not implemented.
 */
#define DONOTCARE ((AppModeType)0xFFFFFFFFUL)

/*
 **********************************************************************************************************************
 * Types
 **********************************************************************************************************************
 */

/** @brief   TRACE[NEO_EBOS_OS_TYPES]
 *  @details Task identifier representation selected for this profile.
 *           OSEK sections 12.1 and 13.2.1 leave representation to the provider.
 */
typedef uint32 TaskType;

/** @brief   TRACE[NEO_EBOS_OS_TYPES]
 *  @details Output reference to a configured task identifier.
 */
typedef TaskType *TaskRefType;

/** @brief   TRACE[NEO_EBOS_OS_TYPES]
 *  @details Storage for the selected task-state symbols, not a native RTOS enum.
 */
typedef uint8 TaskStateType;

/** @brief   TRACE[NEO_EBOS_OS_TYPES]
 *  @details Output reference to the selected task-state representation.
 */
typedef TaskStateType *TaskStateRefType;

/** @brief   TRACE[SWS_Os_91007], TRACE[NEO_EBOS_OS_TYPES]
 *  @details Application-mode representation selected for this profile.
 *           The uint32 width is a NeoTech choice, not a mandated OS width.
 */
typedef uint32 AppModeType;

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


#endif /* OS_TYPES_H */
