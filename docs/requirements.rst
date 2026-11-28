.. _ebos-requirements:

R26-11 requirements and service contracts
=========================================

This page defines the selected standard interface and its trace context. The
nine services declared in ``Os/Os.h`` are **specified, not implemented**. The
working ``EbOs_*`` ESP-IDF adapter remains a separate NeoTech extension profile.

Why requirements and specification are separate
-----------------------------------------------

.. list-table:: Selected source documents
   :header-rows: 1
   :widths: 28 18 54

   * - Document
     - Release / identity
     - Purpose
   * - `Requirements on Operating System <https://www.autosar.org/fileadmin/standards/R26-11/CP/AUTOSAR_CP_RS_OS.pdf>`__
     - R26-11, UID 8
     - Describes OS capabilities, constraints and their rationale at the
       requirements level.
   * - `Specification of Operating System <https://www.autosar.org/fileadmin/standards/R26-11/CP/AUTOSAR_CP_SWS_OS.pdf>`__
     - R26-11, UID 34
     - Refines those requirements into detailed service behavior, configuration,
       call contexts and error rules.
   * - `OSEK/VDX Operating System Specification <https://www.osek-vdx.org/mirror/os223.pdf>`__
     - 2.2.3
     - Supplies the inherited base contracts for the selected core services.

Both AUTOSAR documents were published on 26 November 2026. They separate
**levels of detail**, not requirements from implementation. The SWS is still a
specification; it does not prescribe a FreeRTOS implementation.

For example, ``SRS_Os_00097`` in RS section 4.2.2 requires continuity with the
OSEK OS interface. ``SWS_Os_00001`` in SWS section 7.1.2 refines that requirement
and explicitly records it as an upstream requirement. This gives the development
chain:

.. code-block:: text

   RS capability and rationale
      -> SWS/OSEK service contract
      -> source declaration
      -> runtime implementation
      -> behavioral verification

The requirements filename is ``AUTOSAR_CP_RS_OS.pdf``, but the published IDs
retain the ``SRS_Os_*`` namespace. Do not rename them to ``RS_Os_*``.

Declaring an inherited service does not prove that ``SWS_Os_00001`` is satisfied.
Its ``TRACE[...]`` occurrence identifies the design context. The implementation
and behavioral evidence are explicitly absent for this interface-first stage.

Source layout and comments
--------------------------

Production files follow the repository's ``neo-h`` / ``neo-c`` templates:
NeoTech administrative header, change history, section banners, declaration
groups and Doxygen comments. CUCEL's ``Platform_Types.h`` is the visual reference;
its Platform-specific IDs and metadata values are not copied into OS files.

Public function comments identify verified requirements, service behavior,
parameters, return/error meanings, execution context and restrictions. Details
use concise descriptions rather than reproducing complete standard clauses.
Standard-service declarations are marked as contracts without implementations.
Custom native-adapter functions use independently defined ``NEO_EBOS_*`` IDs.
Formatting or adding a trace marker does not change a requirement's status.

Standard contracts live under ``Os``; the native-helper interface lives under
``OsShell``; its ESP-IDF implementation lives under ``Os_EspIdf``. The project
mirrors native shell ownership under ``Conf/EbOs/OsShell``. These groups follow
MMD's responsibility split without copying supplier-specific package names or
creating unused groups. Package-level requirements and verification remain
shared. Exported include directories preserve ``#include "Os.h"`` and
``#include "EbOs.h"`` for callers.

.. _ebos-service-catalog:

Selected standard service catalog
---------------------------------

The six task services originate in OSEK OS 2.2.3 sections 13.2.3.1 through
13.2.3.6. Lifecycle services originate in sections 13.7.2.1 through 13.7.2.3.
R26-11 SWS identifies the OSEK-origin APIs in its interface summary; it must not
be used to invent individual API-definition IDs for inherited services.

.. code-block:: c

   StatusType ActivateTask(TaskType TaskID);
   StatusType TerminateTask(void);
   StatusType ChainTask(TaskType TaskID);
   StatusType Schedule(void);
   StatusType GetTaskID(TaskRefType TaskID);
   StatusType GetTaskState(TaskType TaskID, TaskStateRefType State);
   AppModeType GetActiveApplicationMode(void);
   void StartOS(AppModeType Mode);
   void ShutdownOS(StatusType Error);

.. list-table:: Core service contracts
   :header-rows: 1
   :widths: 25 14 37 24

   * - Service
     - OSEK clause
     - Contract summary
     - Base return / error contract
   * - ``ActivateTask``
     - 13.2.3.1
     - Request a configured task activation; a higher-priority task may run
       before the caller resumes.
     - ``E_OK`` or ``E_OS_LIMIT``; extended status adds ``E_OS_ID``.
   * - ``TerminateTask``
     - 13.2.3.2
     - End the caller's task activation. Successful termination does not return.
     - Extended-status errors: ``E_OS_RESOURCE``, ``E_OS_CALLEVEL``.
   * - ``ChainTask``
     - 13.2.3.3
     - End the current activation and activate the specified task.
     - Success does not return; ``E_OS_LIMIT``; extended status adds
       ``E_OS_ID``, ``E_OS_RESOURCE``, ``E_OS_CALLEVEL``.
   * - ``Schedule``
     - 13.2.3.4
     - Offer a scheduling point under the configured task scheduling rules.
     - ``E_OK``; extended status adds ``E_OS_CALLEVEL``, ``E_OS_RESOURCE``.
   * - ``GetTaskID``
     - 13.2.3.5
     - Write the executing task ID, or ``INVALID_TASK`` when none applies.
     - ``E_OK``.
   * - ``GetTaskState``
     - 13.2.3.6
     - Write the selected task's observable state.
     - ``E_OK``; extended status adds ``E_OS_ID``.
   * - ``GetActiveApplicationMode``
     - 13.7.2.1
     - Read the active application mode.
     - ``AppModeType``; no status result.
   * - ``StartOS``
     - 13.7.2.2
     - Enter the selected application mode from outside the OS.
     - No result; control need not return.
   * - ``ShutdownOS``
     - 13.7.2.3
     - Shut down the calling core, invoking its configured shutdown hook first.
     - No normal status return.

Every row has the status **specified, not implemented**. These base conditions
are not a complete AUTOSAR extended-status/error catalog. The R26-11 SWS adds
service-protection, application-access, multicore and other constraints; a future
implementation must implement the applicable refinements before claiming those
services as supported.

.. list-table:: Selected R26-11 refinements
   :header-rows: 1
   :widths: 31 69

   * - SWS requirement context
     - Intended contract; runtime is not implemented
   * - ``SWS_Os_00093``
     - Applicable interrupt-lock checks report ``E_OS_DISABLEDINT``.
   * - ``SWS_Os_00612``
     - Termination/chaining must account for occupied spinlocks; extended
       status includes ``E_OS_SPINLOCK``.
   * - ``SWS_Os_00624``
     - Scheduling while holding a spinlock requires the specified error handling.
   * - ``SWS_Os_00424``
     - The initial ``StartOS`` call does not return.
   * - ``SWS_Os_00425`` / ``SWS_Os_00617``
     - ``ShutdownOS`` acts on the calling core; after a returning shutdown hook,
       its specified path disables interrupts and remains in a loop.
   * - ``SWS_Os_91025``
     - Error classification remains a standard-contract concern; defining error
       constants alone does not implement its checks or error handling.

Base call contexts
~~~~~~~~~~~~~~~~~~

.. list-table:: Inherited call-context summary
   :header-rows: 1
   :widths: 37 63

   * - Service group
     - Base context
   * - ``ActivateTask``
     - Task or category-2 ISR.
   * - ``TerminateTask``, ``ChainTask``, ``Schedule``
     - Task context, subject to resource and call-level restrictions.
   * - ``GetTaskID``, ``GetTaskState``
     - Task, category-2 ISR, ErrorHook and Pre/PostTaskHook contexts.
   * - ``GetActiveApplicationMode``
     - Task, category-2 ISR and the standard startup/shutdown/error/task hooks.
   * - ``StartOS``
     - Outside the running OS; it is not a callable replacement for native
       object initialization after ESP-IDF has started scheduling.
   * - ``ShutdownOS``
     - Task, category-2 ISR, ErrorHook or StartupHook, and internal OS handling.

R26-11's call-context and service-protection tables remain authoritative for
extensions and trusted-function cases. An ESP-IDF ISR is not automatically an
AUTOSAR category-2 ISR. These context labels describe the intended standard
contract; there is no implemented context dispatcher in this stage.

Types and representation choices
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

``StatusType`` and ``E_OK`` are reused from CUCEL's ``Std_Types.h`` and its
``STATUSTYPEDEFINED`` contract. They are distinct from ``Neo_StatusType``.
The integration profile selects the existing CUCEL commit
``0bde70c9eb73ae203a52b590c6aaf3f93c4f850e`` that supplies this contract.
The original ``cucel/1.0.0@neotech/prototype`` snapshot lacks ``StatusType``;
it cannot compile the new standard headers. EbOs does not add a fallback typedef.
``TaskRefType`` points to ``TaskType``; ``TaskStateRefType`` points to
``TaskStateType``.

The profile chooses ``uint32`` for task IDs and application modes and ``uint8``
for task-state storage. State codes, ID sentinels and default-mode values are
documented implementation choices in ``Os/Os_Types.h``; they are not claims that
AUTOSAR requires those widths or all numeric representations. ``DONOTCARE`` has
the R26-11 ``AppModeType`` context recorded by ``SWS_Os_91007``. A constant does
not implement multicore startup or mode reconciliation. The published type
description has enumeration semantics; the draft's ``uint32`` representation
choice alone is not proof of satisfying that complete type contract.

No native FreeRTOS handles or ESP-IDF types are exposed by the standard headers.
``E_OK`` is not redefined. The inherited base error profile uses values 1 through
8; the draft assigns ``0x10`` and ``0x11`` to ``E_OS_DISABLEDINT`` and
``E_OS_SPINLOCK``. Those additional numeric values are explicitly NeoTech
representation choices, not AUTOSAR-mandated numbers.

.. _ebos-trace-matrix:

Trace matrix and evidence
-------------------------

The machine-readable catalog is ``requirements/traceability.json``. Its selected
AUTOSAR entries identify the document release and verified requirement IDs.
NeoTech extension entries identify independently defined module behavior. Keep
the catalog and source comments in the same change.

Run ``python test/test_traceability.py`` from the module to check source traces,
template sections, signatures and the declaration-only status. The checks reject
unknown IDs, native service aliases and false implementation claims. Add
``--integration <workspace>`` to include project-owned configuration comments.
With ``pypdf`` available and the official PDFs cached under
``_build/standards/R26-11``, add ``--source-cache _build/standards/R26-11`` to
verify document hashes, releases and requirement IDs on their recorded pages.
The PDFs are not redistributed; normal CI validates the reviewed catalog offline.

.. list-table:: Standard-interface trace context
   :header-rows: 1
   :widths: 27 33 20 20

   * - Requirement context
     - Declaration scope
     - Status
     - Evidence
   * - ``SRS_Os_00097`` -> ``SWS_Os_00001``
     - Selected OSEK task/lifecycle service declarations in ``Os/Os.h``.
     - Specified; runtime not implemented.
     - Origin/signature catalog and compile-only interface checks. No runtime
       conformance evidence.
   * - ``SWS_Os_91007``
     - ``AppModeType`` / ``DONOTCARE`` context in ``Os/Os_Types.h``.
     - Representation specified; multicore startup not implemented.
     - Type/constant checks only.
   * - ``SWS_Os_00093``, ``SWS_Os_00424``, ``SWS_Os_00425``,
       ``SWS_Os_00612``, ``SWS_Os_00617``, ``SWS_Os_00624``, ``SWS_Os_91025``
     - The selected R26-11 refinement contexts recorded by the headers and
       catalog; summarized above.
     - Specified; runtime not implemented.
     - Source/contract checks only; behavioral tests remain future work.

.. list-table:: Implemented NeoTech extension trace map
   :header-rows: 1
   :widths: 25 35 40

   * - ID
     - Source scope
     - Status and evidence
   * - ``NEO_EBOS_00001``
     - ``EbOs_MutexCreate``
     - Implemented; validation and allocation-cleanup tests.
   * - ``NEO_EBOS_00002``
     - ``EbOs_MutexLock``
     - Implemented; nonblocking, bounded and indefinite wait tests.
   * - ``NEO_EBOS_00003`` / ``NEO_EBOS_00004``
     - ``EbOs_MutexUnlock`` / ``EbOs_MutexDelete``
     - Implemented; ownership/lifecycle and null-handle tests.
   * - ``NEO_EBOS_00005`` / ``NEO_EBOS_00006``
     - ``EbOs_Delay`` / ``EbOs_DelayOneTick``
     - Implemented; rounding/saturation and actual-tick tests.
   * - ``NEO_EBOS_00007``
     - ``EbOs_TaskCreate``
     - Implemented; configuration, allocation and fast-completion tests.
   * - ``NEO_EBOS_00008``
     - ``EbOs_TaskJoinAndDelete`` and native completion trampoline.
     - Implemented; completion/suspension, timeout/retry and cleanup tests.
   * - ``NEO_EBOS_00009``
     - ``EbOs_TaskGetStackMinFree``
     - Implemented; native/cached-minimum and context tests.
   * - ``NEO_EBOS_00010`` / ``NEO_EBOS_00011`` / ``NEO_EBOS_00012``
     - Opaque mutex/task handles and native task configuration.
     - Implemented; independent headers and a second-product configuration.
   * - ``NEO_EBOS_00013`` / ``NEO_EBOS_00014``
     - Indefinite-wait and any-core constants.
     - Implemented; wait conversion and one-/two-core configuration tests.
   * - ``NEO_EBOS_OS_TYPES``
     - Standard-contract profile representations.
     - Specified; compile-only type/signature checks.
   * - ``NEO_EBOS_CFG_00001`` / ``NEO_EBOS_CFG_00002``
     - Integration-owned task policy and runnable wiring in ``Conf/EbOs/OsShell``.
     - Implemented; camera backend/project-wiring regression. These are
       project configuration requirements, not AUTOSAR-issued OS requirements.

``EbOs_TaskCreate`` is dynamic native-thread creation, not ``ActivateTask``.
``EbOs_TaskJoinAndDelete`` is owner-driven cooperative retirement, not
``TerminateTask``. The two profiles must not be connected through macro aliases,
success-returning stubs or requirement markers that imply runtime equivalence.

Verification boundaries
-----------------------

Interface checks compile the standard headers and verify function-pointer types
without calling or linking the unimplemented services. Trace checks compare
source markers with the recorded catalog. Strict Sphinx builds check document
structure and internal references. None of these establishes AUTOSAR runtime
conformance.

The existing native-adapter regressions continue to check implemented behavior.
Its physical dual-core/camera acceptance gate remains pending until the target
evidence described in :ref:`the module specification <sws-ebos>` is recorded.
This interface/style revision does not add or execute hardware flashing.
