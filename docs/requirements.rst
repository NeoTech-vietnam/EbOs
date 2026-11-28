.. _ebos-requirements:

R26-11 requirements and service contracts
=========================================

The six task services declared in ``Os/Os.h`` have an implemented ESP-IDF
**basic-task profile**. ``StartOS``, ``ShutdownOS`` and
``GetActiveApplicationMode`` remain specified, not implemented. The selected
profile and its tests do not establish AUTOSAR OS conformance.

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
     - OS capabilities, constraints and their rationale.
   * - `Specification of Operating System <https://www.autosar.org/fileadmin/standards/R26-11/CP/AUTOSAR_CP_SWS_OS.pdf>`__
     - R26-11, UID 34
     - Detailed service behavior, configuration, call contexts and errors.
   * - `OSEK/VDX Operating System Specification <https://www.osek-vdx.org/mirror/os223.pdf>`__
     - 2.2.3
     - Inherited base task/lifecycle contracts.

Both AUTOSAR documents were published on 26 November 2026. They separate
**levels of detail**; the SWS remains a specification and does not prescribe a
FreeRTOS implementation. The requirements filename is ``AUTOSAR_CP_RS_OS.pdf``;
its published IDs retain the ``SRS_Os_*`` namespace.

For example, ``SRS_Os_00097`` in RS section 4.2.2 requires OSEK interface
compatibility. ``SWS_Os_00001`` in SWS section 7.1.2 refines that requirement and
records it upstream. Development adds further evidence:

.. code-block:: text

   RS capability and rationale
      -> SWS/OSEK service contract
      -> source interface and implementation
      -> behavioral verification
      -> platform acceptance

A source ``TRACE[...]`` marker identifies this context; it does not claim that
the complete standard requirement is satisfied. The catalog keeps every
AUTOSAR requirement contextual or partial while recording the six services as
``implemented_profile``. Earlier revisions contained declarations only; that
historical status does not describe the current task backend.

Ownership and coding style
--------------------------

Production files use the ``neo-h`` / ``neo-c`` NeoTech administrative banner,
change history, section order and Doxygen comments. CUCEL's
``Platform_Types.h`` supplies the style reference. Its Platform-specific trace IDs
and metadata are not copied to OS code.

.. code-block:: text

   EbOs/Os/Os.h                     standard task interface
   EbOs/Os/Os_Types.h               portable types and error encodings
   EbOs/OsShell/EbOs_Os.h           profile configuration types and lifecycle
   EbOs/Os_EspIdf/Os_EspIdf.c       task-service implementation
   EbOs/OsShell/EbOs.h              independent native-helper interface
   EbOs/Os_EspIdf/EbOs_EspIdf.c     native-helper implementation

   Conf/EbOs/Os/Os_Cfg.h            project task IDs
   Conf/EbOs/Os/Os_Cfg.c            project task table, policy and entry wiring
   Conf/EbOs/OsShell/EbOs_Cfg.*      existing camera native-thread policy/wiring

EbOs receives configuration by argument and includes no project configuration
headers. Product names, priorities, stack sizes, activation limits and entry
bindings stay under ``Conf/EbOs``. Package source/include metadata remains in
``module.cmake``; no forwarding headers or loose production C files belong at
the EbOs root.

.. _ebos-basic-task-profile:

Implemented basic-task profile
------------------------------

The product passes an immutable ``EbOs_OsConfigType`` to ``EbOs_OsInit``.
Initialization validates the complete table and allocates dormant native
workers; it does not activate entries, start a kernel or implement ``StartOS``.
Task IDs are table indices. Names, table storage and function bindings remain
valid until successful ``EbOs_OsDeinit``.

The current NeoCam table contains ``Os_Task_ProjectCheck``, a dormant example.
It queries task identity/state and terminates. Existing camera, HTTP and Wi-Fi
startup behavior stays unchanged until project composition explicitly chooses
to initialize and activate this profile:

.. code-block:: c

   #include "EbOs_Os.h"
   #include "Os_Cfg.h"

   /* In serialized product composition, with the scheduler running. */
   if (EbOs_OsInit(&Os_Config) == NEO_OK)
   {
       StatusType status = ActivateTask(Os_Task_ProjectCheck);
       /* Product composition handles status and owns the later deinit. */
       (void)status;
   }

Supported entries are **fully preemptive basic C tasks**, all pinned to one
explicit core, with distinct native priorities. Stacks use ESP-IDF byte units.
A positive activation limit includes the current activation and queued requests.
Entries must not block or retain native locks when terminating. There is no
extended-task event service, non-preemptive policy, internal resource, AUTOSAR
resource/spinlock service, OS-Application protection or equal-priority scheduling
implementation in this profile.

Successful ``TerminateTask`` and ``ChainTask`` leave the application call chain
through a same-worker C nonlocal exit and restart later activations at the entry.
A task or helper in that chain must not use variable-length arrays, cross C++
objects requiring destruction, or depend on automatic cleanup after termination.
The SDK worker persists across activations; no native task allocation occurs in
``ActivateTask``.

Initialization and deinitialization must be externally serialized with **all six
task services**, not only with each other. The configuration is immutable during
its lifetime. Deinit returns ``NEO_E_BUSY`` while any activation is outstanding or
a worker has not reached its private parked wait, preserving everything for a
later retry. It never force-deletes active work. No outside code may notify,
resume or otherwise manipulate profile workers.

The optional ``missing_end_hook`` receives the task ID and ``E_OS_MISSINGEND``
as a NeoTech diagnostic before retirement. It
may query identity/state and must return without blocking; it must not call
``TerminateTask``, ``ChainTask`` or ``Schedule``. It is not standard ``ErrorHook``
or Pre/PostTaskHook. Tasks normally finish through explicit termination/chaining.

.. _ebos-service-catalog:

Selected task service catalog
-----------------------------

The service origins are OSEK OS 2.2.3 sections 13.2.3.1 through 13.2.3.6,
retained through ``SWS_Os_00001``. The SWS interface summary does not supply new
individual API-definition IDs for these inherited functions.

.. list-table:: Implemented service behavior within the profile
   :header-rows: 1
   :widths: 22 52 26

   * - Service
     - Profile behavior
     - Specific results
   * - ``ActivateTask(TaskID)``
     - Queue one activation; limit failure leaves all state unchanged.
     - ``E_OK``, ``E_OS_ID``, ``E_OS_LIMIT``.
   * - ``TerminateTask()``
     - End the current activation, retaining pending requests and the worker.
     - Success never returns; invalid caller returns ``E_OS_CALLEVEL``.
   * - ``ChainTask(TaskID)``
     - Retire caller and release successor. Self-chain replaces the activation
       without consuming additional capacity; failed chaining preserves caller.
     - Success never returns; ``E_OS_ID`` or ``E_OS_LIMIT`` on rejected target.
   * - ``Schedule()``
     - Check context and return. Fully preemptive entries without internal
       resources need no explicit native yield.
     - ``E_OK`` for an eligible configured caller.
   * - ``GetTaskID(TaskID)``
     - Write configured identity, or ``INVALID_TASK`` for an unregistered SDK task.
     - ``E_OK`` or ``E_OS_ILLEGAL_ADDRESS`` for null output.
   * - ``GetTaskState(TaskID, State)``
     - Write a snapshot: outstanding work ready/running, no work suspended.
       ``WAITING`` diagnoses unsupported native blocking; it is not event support.
     - ``E_OK``, ``E_OS_ID`` or ``E_OS_ILLEGAL_ADDRESS`` for null output.

All six additionally check initialization, native task context, interrupt state
and scheduler state. Prohibited calls have no requested effect and leave output
storage unchanged. The profile uses ``E_OS_STATE`` before initialization,
``E_OS_CALLEVEL`` for native ISR or suspended-scheduler calls, and
``E_OS_DISABLEDINT`` for masked task interrupts. Missing-end hook restrictions
apply to termination, chaining and scheduling. Read-only hook queries remain
available.

These results describe the implemented profile, not the complete standard
extended-status catalog. The inherited task-only services require resources to
be released and R26-11 adds spinlock restrictions. Those services and ownership
checks are not implemented: release every native lock before ending an entry.
Native mutex priority inheritance does not implement AUTOSAR resource ceilings.

R26-11 call contexts include category 2 ISRs for activation and read-only queries,
and selected standard hooks for queries. The current adapter rejects **all native
ISRs** because it has no category 2 bridge. Native ISR identity is not synthesized
from global task state. ``StartOS``, ``ShutdownOS`` and
``GetActiveApplicationMode`` retain their declarations without runtime bodies;
profile init/deinit does not implement them.

Types and representation
-------------------------

``StatusType``, ``E_OK`` and ``STATUSTYPEDEFINED`` come from CUCEL
``Std_Types.h``; they are distinct from ``Neo_StatusType`` used by extension
lifecycle calls. The integration selects the compatible CUCEL commit
``0bde70c9eb73ae203a52b590c6aaf3f93c4f850e``. The original
``cucel/1.0.0@neotech/prototype`` lacks ``StatusType``. No local fallback typedef
is supplied.

Task IDs and application-mode storage use ``uint32``; task states use ``uint8``.
The chosen widths, state encodings, sentinels and default mode are NeoTech
representation decisions, not complete AUTOSAR type-conformance claims.
``DONOTCARE`` has the ``SWS_Os_91007`` context, but mode reconciliation is not
implemented. No native handles appear in the public interfaces.

Inherited errors retain values 1 through 8. AUTOSAR-specific encodings are
implementation assigned under ``SWS_Os_91025``: ``E_OS_DISABLEDINT`` is ``0x10``,
``E_OS_SPINLOCK`` is ``0x11``, ``E_OS_MISSINGEND`` is ``0x12`` and
``E_OS_ILLEGAL_ADDRESS`` is ``0x13``. Defining a code does not implement every
standard condition that can report it.

.. _ebos-trace-matrix:

Trace matrix and evidence
-------------------------

``requirements/traceability.json`` records source release, pages, service
origins, implementation paths, profile restrictions and verification files.
Every AUTOSAR entry remains contextual or partial; none is marked satisfied.

.. list-table:: Selected standard trace context
   :header-rows: 1
   :widths: 32 44 24

   * - Requirement context
     - Implementation status and gap
     - Evidence
   * - ``SRS_Os_00097`` / ``SWS_Os_00001``
     - Six profile services implemented; complete OSEK/AUTOSAR compatibility not
       established.
     - Contract compilation and ``test/test_os_tasks.c``.
   * - ``SWS_Os_00088`` / ``SWS_Os_00093``
     - Task/ISR/hook and masked-interrupt checks; standard category 2 dispatch,
       OS-Application protection and interrupt services absent.
     - Context rejection without state mutation.
   * - ``SWS_Os_00566``
     - Null references checked. Protected writable-address validation remains
       absent; a null check is not memory protection.
     - Output-reference regression cases.
   * - ``SWS_Os_00052`` / ``SWS_Os_00069``
     - Returning entries retired; optional NeoTech diagnostic runs first.
       Standard ErrorHook and Pre/PostTaskHook semantics absent.
     - Return-without-termination diagnostic and identity checks.
   * - ``SWS_Os_00612`` / ``SWS_Os_00624``
     - Context only: resource/spinlock mechanisms and ownership checks absent.
     - Contract/source checks, no satisfaction claim.
   * - ``SWS_Os_00424`` / ``SWS_Os_00425`` / ``SWS_Os_00617``
     - Context only: OS startup/shutdown unimplemented.
     - Declarations only; no lifecycle runtime claim.
   * - ``SWS_Os_91007`` / ``SWS_Os_91025``
     - Representation context; error encodings selected, complete application
       modes and type compatibility unverified.
     - Type/constant and signature checks.

.. list-table:: Independent NeoTech requirements
   :header-rows: 1
   :widths: 30 40 30

   * - IDs
     - Source scope
     - Evidence
   * - ``NEO_EBOS_00001`` through ``NEO_EBOS_00006``
     - Native mutexes, delays and one-tick yield.
     - ``test/test_ebos.c``.
   * - ``NEO_EBOS_00007`` through ``NEO_EBOS_00014``
     - Native task create/join/stack metrics, opaque types and configuration.
     - Native-helper tests and second-product fixture.
   * - ``NEO_EBOS_OS_TYPES``
     - Portable profile representations.
     - Independent header/type/signature checks.
   * - ``NEO_EBOS_OS_RUNTIME``
     - Configured activation/lifecycle profile in ``Os_EspIdf/Os_EspIdf.c``;
       product supplies ``Conf/EbOs/Os`` policy.
     - ``test/test_os_tasks.c``; physical acceptance pending.
   * - ``NEO_EBOS_CFG_00001`` / ``NEO_EBOS_CFG_00002``
     - Existing camera policy and native-thread wiring in ``Conf/EbOs/OsShell``.
     - Camera backend/project-wiring regression.

``EbOs_TaskCreate`` creates a native thread; ``ActivateTask`` queues a configured
activation. ``EbOs_TaskJoinAndDelete`` retires an owner's cooperative native
thread; ``TerminateTask`` ends the caller's current configured activation. The
profiles remain separate and may not be joined through macro aliases.

Verification boundaries
-----------------------

Run ``python test/run_host_tests.py``. Real task-service source executes against
a controlled native backend to check activation limits, self/other chaining,
nonreturn exits, entry restart, missing-end diagnostics, context checks,
configuration/allocation failures and safe deinit/retry. Header checks separately
verify all nine standard signatures without linking the three unimplemented
lifecycle services.

``python test/test_traceability.py`` checks known IDs, NeoTech templates,
signatures and the allowed implementation paths. It rejects aliases, duplicate
or misplaced task bodies, lifecycle bodies and unsupported conformance claims.
Use ``--integration <workspace>`` to include project configuration. With
``pypdf`` and cached official PDFs, add
``--source-cache _build/standards/R26-11`` to verify hashes, release and recorded
requirement pages. Normal CI uses the reviewed offline catalog.

Host tests and Windows/Ubuntu SDK builds establish logic and integration
coverage. Physical ESP32-S3 scheduling, lifetime and camera-behavior acceptance
remain pending until monitor evidence is recorded. Strict Sphinx builds verify
document structure; source traces and document formatting do not prove runtime
conformance.
