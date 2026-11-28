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
 * $Filename__:test_os_contracts.c$
 *
 * $Author____:Nguyen Ba Vu Thach$
 *
 * $Function__:Compile-only checks for the R26-11 declaration profile.$
 *
 *************************************************************************
 * $Domain____:NeoTech$
 * $User______:Nguyen Ba Vu Thach$
 * $Date______:28.11.2026$
 * $Class_____:EbOs$
 * $Name_____:test_os_contracts$
 * $Variant___:1.1.0$
 * $Revision__:1$
 * $Type_____:C$
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

/*
 **********************************************************************************************************************
 * Includes
 **********************************************************************************************************************
 */

#include "Os.h"

/*
 **********************************************************************************************************************
 * Defines / Macros
 **********************************************************************************************************************
 */

#define OS_SIGNATURE_MATCH(Service, Signature) _Generic(&(Service), Signature: 1, default: 0)

/*
 **********************************************************************************************************************
 * Local types
 **********************************************************************************************************************
 */

typedef StatusType (*TaskArgumentService)(TaskType);
typedef StatusType (*VoidArgumentService)(void);
typedef StatusType (*TaskReferenceService)(TaskRefType);
typedef StatusType (*TaskStateService)(TaskType, TaskStateRefType);
typedef void (*StartService)(AppModeType);
typedef void (*ShutdownService)(StatusType);
typedef AppModeType (*ModeService)(void);

/*
 **********************************************************************************************************************
 * Static variables
 **********************************************************************************************************************
 */


/*
 **********************************************************************************************************************
 * Static function prototypes
 **********************************************************************************************************************
 */


/*
 **********************************************************************************************************************
 * Local functions
 **********************************************************************************************************************
 */


/*
 **********************************************************************************************************************
 * Public functions
 **********************************************************************************************************************
 */

/* Unevaluated type checks do not call or link any unimplemented service. */
_Static_assert(OS_SIGNATURE_MATCH(ActivateTask, TaskArgumentService), "ActivateTask signature");
_Static_assert(OS_SIGNATURE_MATCH(TerminateTask, VoidArgumentService), "TerminateTask signature");
_Static_assert(OS_SIGNATURE_MATCH(ChainTask, TaskArgumentService), "ChainTask signature");
_Static_assert(OS_SIGNATURE_MATCH(Schedule, VoidArgumentService), "Schedule signature");
_Static_assert(OS_SIGNATURE_MATCH(GetTaskID, TaskReferenceService), "GetTaskID signature");
_Static_assert(OS_SIGNATURE_MATCH(GetTaskState, TaskStateService), "GetTaskState signature");
_Static_assert(OS_SIGNATURE_MATCH(StartOS, StartService), "StartOS signature");
_Static_assert(OS_SIGNATURE_MATCH(ShutdownOS, ShutdownService), "ShutdownOS signature");
_Static_assert(OS_SIGNATURE_MATCH(GetActiveApplicationMode, ModeService), "mode signature");

_Static_assert(sizeof(TaskType) == 4U && sizeof(AppModeType) == 4U, "32-bit profile IDs");
_Static_assert(sizeof(TaskStateType) == 1U, "8-bit profile state");
_Static_assert(_Generic((TaskRefType)0, TaskType *: 1, default: 0), "task reference");
_Static_assert(_Generic((TaskStateRefType)0, TaskStateType *: 1, default: 0), "state reference");
_Static_assert(_Generic((StatusType)0, unsigned char: 1, default: 0), "CUCEL StatusType reused");
_Static_assert(E_OK == 0U, "E_OK reused");
_Static_assert(E_OS_ACCESS == 1U && E_OS_CALLEVEL == 2U && E_OS_ID == 3U, "OSEK status profile");
_Static_assert(E_OS_LIMIT == 4U && E_OS_NOFUNC == 5U && E_OS_RESOURCE == 6U, "OSEK status profile");
_Static_assert(E_OS_STATE == 7U && E_OS_VALUE == 8U, "OSEK status profile");
_Static_assert(E_OS_DISABLEDINT > E_OS_VALUE && E_OS_SPINLOCK > E_OS_VALUE, "R26-specific status range");
_Static_assert(RUNNING != READY && RUNNING != WAITING && RUNNING != SUSPENDED, "state distinction");
_Static_assert(READY != WAITING && READY != SUSPENDED && WAITING != SUSPENDED, "state distinction");
_Static_assert(INVALID_TASK == (TaskType)0xFFFFFFFFUL, "reserved task ID");
_Static_assert(OSDEFAULTAPPMODE != DONOTCARE, "mode distinction");
