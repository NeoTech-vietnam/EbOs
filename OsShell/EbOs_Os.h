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
 * $Filename__:EbOs_Os.h$
 *
 * $Author____:Nguyen Ba Vu Thach$
 *
 * $Function__:Configured ESP-IDF basic-task profile initialization and policy types.$
 *
 *************************************************************************
 * $Domain____:NeoTech$
 * $User______:Nguyen Ba Vu Thach$
 * $Date______:28.11.2026$
 * $Class_____:EbOs$
 * $Name_____:EbOs_Os$
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
 *   Add the explicitly limited ESP-IDF basic-task runtime profile.
 *
 * $
 *
 *************************************************************************
</NeoTechHead>*/

#ifndef EBOS_OS_H
#define EBOS_OS_H

/*
 **********************************************************************************************************************
 * Includes
 **********************************************************************************************************************
 */

#include "Os.h"
#include "Neo_Types.h"

/*
 **********************************************************************************************************************
 * Defines / Macros
 **********************************************************************************************************************
 */

/* No native kernel handles or project-selected values are public here. */

/*
 **********************************************************************************************************************
 * Types
 **********************************************************************************************************************
 */

/** @brief   TRACE[NEO_EBOS_OS_RUNTIME]
 *  @details Immutable configuration for one fully preemptive basic C task.
 *           The array index is its TaskType identifier. Stack size is in bytes;
 *           priority uses the ESP-IDF scale and must be unique in this runtime.
 *           activation_limit includes the executing activation and queued requests.
 *           Entry must finish with TerminateTask or ChainTask, release all native
 *           locks first, and must not block. Nonlocal exits require a C call chain
 *           without variable-length arrays or C++ objects requiring destruction.
 */
typedef struct
{
    const char *name;
    uint32 stack_bytes;
    uint32 priority;
    uint32 activation_limit;
    void (*entry)(void);
} EbOs_OsTaskConfigType;

/** @brief   TRACE[NEO_EBOS_OS_RUNTIME]
 *  @details Product-owned task table and one fixed ESP-IDF core assignment.
 *           Neither unpinned nor mixed-core configured tasks are supported.
 *           No task is activated during initialization. Configuration and names
 *           remain valid and immutable until successful deinitialization.
 *           missing_end_hook is an optional NeoTech diagnostic, called in the
 *           returning task with E_OS_MISSINGEND before logical termination;
 *           it is not ErrorHook.
 *           It may query identity/state but must return without blocking and
 *           must not call TerminateTask, ChainTask or Schedule.
 */
typedef struct
{
    const EbOs_OsTaskConfigType *tasks;
    uint32 task_count;
    sint32 core_affinity;
    void (*missing_end_hook)(TaskType task_id, StatusType error);
} EbOs_OsConfigType;

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

/** @brief   TRACE[NEO_EBOS_OS_RUNTIME]
 *  @details Validate and create dormant native workers for a configured task set.
 *           Initialization is all-or-nothing; it neither starts the kernel nor
 *           implements StartOS. The caller serializes lifecycle operations with
 *           every standard task call, and must have a running scheduler with
 *           interrupts enabled. Afterwards ActivateTask releases configured work.
 *  @param[in] config Product-owned immutable runtime configuration.
 *  @return  NEO_OK, NEO_E_PARAM, NEO_E_MEMORY or NEO_E_STATE.
 */
Neo_StatusType EbOs_OsInit(const EbOs_OsConfigType *config);

/** @brief   TRACE[NEO_EBOS_OS_RUNTIME]
 *  @details Delete only inactive workers confirmed blocked in their private
 *           notification wait. Lifecycle calls are externally serialized with
 *           all task services; no other code may notify or resume these workers.
 *           A busy result retains all workers and configuration for retry.
 *           This extension does not implement ShutdownOS.
 *  @return  NEO_OK, NEO_E_BUSY while active/not parked, or NEO_E_STATE for a
 *           prohibited calling context. An already deinitialized runtime is OK.
 */
Neo_StatusType EbOs_OsDeinit(void);

#ifdef __cplusplus
}
#endif

#endif /* EBOS_OS_H */
