.. _sws-ebos:

Specification of EbOs
=====================

.. list-table:: Document metadata
   :header-rows: 1
   :class: specification-fields

   * - Field
     - Value
   * - Document ID
     - ``NEOCAM-SWS-EbOs``
   * - Owner
     - EbOs maintainers
   * - Status
     - Implemented native helpers and selected R26-11 basic-task service profile
   * - Source selection
     - This documentation follows the source revision containing it. A product's
       ``packages.yml`` and resolved commit report determine the consumed revision.
   * - Earlier baseline
     - ``ebos/1.0.0@neotech/prototype`` at
       `2380cea17d6c9fd013b214a2625d555f6aa97c0c <https://github.com/NeoTech-vietnam/EbOs/tree/2380cea17d6c9fd013b214a2625d555f6aa97c0c>`__
       provides mutexes and delays only.

.. list-table:: Document change history
   :header-rows: 1

   * - Date
     - Revision
     - Description
   * - 2026-11-22
     - 1.0
     - Initial specification of the manifest-selected implementation.
   * - 2026-11-28
     - 1.1
     - Reusable task configuration, cooperative retirement, diagnostics and
       AUTOSAR R25-11 compatibility profile.
   * - 2026-11-28
     - 1.2
     - R26-11 requirements/catalog, standard interface declarations and
       NeoTech source-level traceability. Native runtime behavior is preserved.
   * - 2026-11-28
     - 1.3
     - Classify standard contracts, shell interfaces and platform code into
       MMD-style source groups; preserve behavior and requirement identifiers.
   * - 2026-11-28
     - 1.4
     - Implement six configured basic-task services over ESP-IDF, with separate
       project configuration, bounded activations and explicit profile limits.

.. contents:: Contents
   :local:
   :depth: 2

1 Introduction and functional overview
--------------------------------------

EbOs provides task delays, opaque non-recursive mutexes, and configured native
tasks backed by ESP-IDF FreeRTOS. It reuses the kernel rather than implementing
a scheduler. A configured native task is a persistent thread; it is not an
AUTOSAR basic task with activation and termination semantics.

``Os/Os.h`` and ``Os/Os_Types.h`` define a separate configured basic-task profile.
Its six task services have an ESP-IDF implementation. ``StartOS``, ``ShutdownOS``
and ``GetActiveApplicationMode`` remain specified, not implemented. Existing
``EbOs_*`` native helpers are independent extensions; native thread creation is
not configured task activation. See :ref:`the requirements and service catalog
<ebos-requirements>` for the implemented profile and remaining standard gaps.

The first consumer is NeoCam's camera worker. Camera behavior stays in CamPipe,
task policy and wiring belong to the product, and EbOs contains no product
entry-function references. HTTP and Wi-Fi execution are outside this pilot.

2 Acronyms and abbreviations
----------------------------

.. list-table:: Terminology
   :header-rows: 1

   * - Term
     - Meaning
   * - Tick
     - Scheduler time unit.
   * - Mutex
     - Mutual-exclusion object with task ownership.
   * - ISR
     - Interrupt service routine.

3 Related documentation
-----------------------

* `NeoCam architecture and adaptation profile <https://neotech-vietnam.github.io/neocam_csw/architecture/architecture.html>`__
* `NeoCam runtime reference <https://neotech-vietnam.github.io/neocam_csw/operations/runtime.html>`__
* `AUTOSAR Classic R26-11, Requirements on Operating System, Document 8 <https://www.autosar.org/fileadmin/standards/R26-11/CP/AUTOSAR_CP_RS_OS.pdf>`__.
* `AUTOSAR Classic R26-11, Specification of Operating System, Document 34 <https://www.autosar.org/fileadmin/standards/R26-11/CP/AUTOSAR_CP_SWS_OS.pdf>`__.
* `OSEK/VDX Operating System Specification 2.2.3 <https://www.osek-vdx.org/mirror/os223.pdf>`__.
* `ESP-IDF 6.0.1 FreeRTOS <https://docs.espressif.com/projects/esp-idf/en/v6.0.1/esp32s3/api-reference/system/freertos_idf.html>`__.
* `ESP-IDF application entry point <https://docs.espressif.com/projects/esp-idf/en/v6.0.1/esp32s3/api-reference/system/freertos.html#application-entry-point>`__.

4 Constraints and assumptions
-----------------------------

* Use from task context. The locking task releases its own mutex.
* Creation and deletion require serialized ownership and quiescent users.
* Task creation, join/deletion and stack sampling have one serialized owner.
  Do not sample through a handle while another caller joins and deletes it.
* A native-helper task entry returns after its module's cooperative stop request. EbOs
  parks the native task and the owner retires it; no force-stop or resume
  operation is exposed. The entry must not delete or suspend itself.
* Configured basic-task entries instead finish through ``TerminateTask`` or
  ``ChainTask``. They are C-only, fully preemptive, nonblocking entries, with
  distinct priorities on one explicitly pinned core.
* Basic-task profile initialization/deinitialization is externally serialized
  with all six task service calls. Project configuration is immutable and remains
  alive until successful deinitialization.
* ESP-IDF starts FreeRTOS before ``app_main``. EbOs does not call
  ``vTaskStartScheduler`` or ``vTaskEndScheduler``.

5 Dependencies and file structure
---------------------------------

.. list-table:: Dependencies
   :header-rows: 1

   * - Dependency
     - Purpose
   * - `NeoContracts <https://neotech-vietnam.github.io/NeoContracts/specification.html>`__
     - Supplies status and platform types.
   * - ESP-IDF FreeRTOS and heap capabilities
     - Provide native tasks, semaphores, tick delays and internal-memory allocation.
   * - ESP Timer
     - Supplies the monotonic time base for bounded task joins.

**Source files**

The package uses MMD-style groups rather than placing C sources and headers at
its root:

.. code-block:: text

   EbOs/
      Os/Os.h                 standard service declarations
      Os/Os_Types.h           standard contract types/profile values
      OsShell/EbOs.h          reusable native-helper interface
      OsShell/EbOs_Os.h       basic-task profile configuration/lifecycle
      Os_EspIdf/EbOs_EspIdf.c  native-helper implementation
      Os_EspIdf/Os_EspIdf.c   configured basic-task service implementation
      module.cmake            package source/include metadata
      docs/                   package documentation
      requirements/           reviewed trace catalog
      test/                   shared verification

``OsShell/EbOs.h`` declares the portable native-task/mutex interfaces and
configuration types. ``Os_EspIdf/EbOs_EspIdf.c`` implements them using the SDK.
``Os/Os.h`` and ``Os/Os_Types.h`` define the separate standard task contract;
``Os_EspIdf/Os_EspIdf.c`` implements the six supported profile services using
configuration types from ``OsShell/EbOs_Os.h``. ``module.cmake`` exports the
classified include directories, so public include directives remain
``#include "EbOs.h"`` and ``#include "Os.h"``.

Read source and documentation from the same revision. The product keeps native
shell policy and entry bindings in ``Conf/EbOs/OsShell`` and configured basic-task
policy in ``Conf/EbOs/Os``. Package-level build
metadata, tests and the machine-readable ``requirements/traceability.json``
remain at the package root. No empty groups for unimplemented MMD features are
introduced.

6 Requirements traceability
---------------------------

The complete :ref:`trace matrix <ebos-trace-matrix>` distinguishes standard
interface context from implemented NeoTech behavior. The summary below describes
the working adapter; its IDs are not AUTOSAR-issued requirements.

.. list-table:: Implemented adapter requirements
   :header-rows: 1

   * - Requirement
     - Description
     - Satisfied by
     - Verification evidence
   * - ``NEO_EBOS_00001`` / ``NEO_EBOS_00002``
     - Provide bounded waits and an explicit indefinite mutex wait.
     - :ref:`Mutex and delay behavior <sws-ebos-mutex>`
     - ``Os_EspIdf/EbOs_EspIdf.c``. Portable integration tests use a backend fake.
   * - ``NEO_EBOS_00007`` / ``NEO_EBOS_00012``
     - Validate project configuration and create opaque native tasks without
       product-specific dependencies.
     - :ref:`Configured task behavior <sws-ebos-tasks>`
     - Module tests compile the native adapter against a controlled backend.
   * - ``NEO_EBOS_00008``
     - Preserve an unfinished task on timeout and retire only a completed,
       suspended native task.
     - :ref:`Configured task behavior <sws-ebos-tasks>`
     - Immediate completion, timeout/retry and suspension-transition tests;
       hardware acceptance remains separate.

7 Functional specification
--------------------------

.. _sws-ebos-mutex:

Mutex and delay behavior
~~~~~~~~~~~~~~~~~~~~~~~~

Finite durations round up to scheduler ticks and saturate below ``portMAX_DELAY``. Zero is a nonblocking take. ``EBOS_WAIT_FOREVER`` requests the backend's maximum mutex wait. The wrapper lives in internal memory.

Error classification
~~~~~~~~~~~~~~~~~~~~

Create rejects a null output and reports allocation failure. A null lock handle returns ``NEO_E_PARAM``. A failed semaphore take maps to ``NEO_E_TIMEOUT``. Unlock and delete ignore null handles.

.. _sws-ebos-tasks:

Configured task behavior
~~~~~~~~~~~~~~~~~~~~~~~~

The product supplies a task name, stack size in bytes, priority, core affinity
and entry function. ``EBOS_CORE_ANY`` (``-1``) means unpinned; an explicit core must exist in the
selected SDK configuration. Entry and configuration must be valid before native
creation. Priorities use the selected FreeRTOS priority scale. They do not assert
AUTOSAR scheduling guarantees.

Creation requires a nonempty name, a non-null entry, a stack of at least the
selected SDK's ``configMINIMAL_STACK_SIZE`` bytes, priority from ``1`` through
``configMAX_PRIORITIES - 1``, and affinity ``EBOS_CORE_ANY`` or an available core.

EbOs initializes all metadata before creating the native task: a fast task can
finish before creation returns. Its trampoline calls the configured entry and,
after return, caches the stack minimum, publishes completion and suspends itself.
It never self-deletes. Joining requires both published completion and a native
suspended state before deleting the task and releasing its opaque wrapper.

The pinned kernel's state query checks whether a task is still executing on
either core before reporting suspension. This matters during the cross-core
transition from entry return to suspension. See the
`ESP-IDF 6.0.1 kernel source <https://github.com/espressif/esp-idf/blob/v6.0.1/components/freertos/FreeRTOS-Kernel/tasks.c>`__.

A finite join uses a monotonic deadline and yields while waiting.
``EBOS_WAIT_FOREVER`` selects an indefinite join. Timeout preserves the caller's
handle and dependencies for retry. A task cannot join itself or perform these
operations from ISR context. The entry must return cooperatively: a blocked or
unfinished camera operation is never deleted to satisfy a timeout.

Stack reporting returns the minimum unused stack in bytes through the stable
native handle. After completion it combines that measurement with the
entry-return snapshot, taking the lower value so that deeper suspension-path
stack use is included. The native handle remains alive until successful join.
Lifecycle and sampling operations are serialized. An actual one-tick delay is provided separately from
millisecond conversion so that a low tick rate cannot turn the producer yield
into a zero-tick call.

8 API specification
-------------------

.. list-table:: EbOs_MutexType
   :header-rows: 1
   :class: specification-fields

   * - Field
     - Value
   * - Name
     - ``EbOs_MutexType``
   * - Kind
     - Opaque pointer
   * - Representation / range
     - ``struct EbOs_Mutex *``
   * - Description
     - Opaque wrapper for a native mutex.
   * - Available via
     - ``EbOs.h``

.. list-table:: EBOS_WAIT_FOREVER
   :header-rows: 1
   :class: specification-fields

   * - Field
     - Value
   * - Name
     - ``EBOS_WAIT_FOREVER``
   * - Kind
     - Timeout sentinel
   * - Value
     - ``UINT32_MAX``
   * - Available via
     - ``EbOs.h``

.. list-table:: EbOs_MutexCreate
   :header-rows: 1
   :class: specification-fields

   * - Field
     - Value
   * - Service name
     - ``EbOs_MutexCreate``
   * - Syntax
     - ``Neo_StatusType EbOs_MutexCreate(EbOs_MutexType *out)``
   * - Execution and reentrancy
     - Synchronous. The composition owner serializes lifecycle calls.
   * - Parameters (in)
     - None.
   * - Parameters (inout)
     - None.
   * - Parameters (out)
     - ``out`` created handle, or null after allocation failure.
   * - Return value
     - ``NEO_OK``, ``NEO_E_PARAM``, ``NEO_E_MEMORY``
   * - Description
     - Creates the wrapper and native mutex.
   * - Available via
     - ``EbOs.h``

.. list-table:: EbOs_MutexLock
   :header-rows: 1
   :class: specification-fields

   * - Field
     - Value
   * - Service name
     - ``EbOs_MutexLock``
   * - Syntax
     - ``Neo_StatusType EbOs_MutexLock(EbOs_MutexType mutex, uint32 timeout_ms)``
   * - Execution and reentrancy
     - Synchronous task-context call. Keep the object alive throughout the call.
   * - Parameters (in)
     - ``mutex`` handle, ``timeout_ms`` finite milliseconds or the indefinite sentinel.
   * - Parameters (inout)
     - None.
   * - Parameters (out)
     - None.
   * - Return value
     - ``NEO_OK``, ``NEO_E_PARAM``, ``NEO_E_TIMEOUT``
   * - Description
     - Takes the mutex.
   * - Available via
     - ``EbOs.h``

.. list-table:: EbOs_MutexUnlock
   :header-rows: 1
   :class: specification-fields

   * - Field
     - Value
   * - Service name
     - ``EbOs_MutexUnlock``
   * - Syntax
     - ``void EbOs_MutexUnlock(EbOs_MutexType mutex)``
   * - Execution and reentrancy
     - Synchronous task-context call. Keep the object alive throughout the call.
   * - Parameters (in)
     - ``mutex`` handle owned by the current task.
   * - Parameters (inout)
     - None.
   * - Parameters (out)
     - None.
   * - Return value
     - None.
   * - Description
     - Releases the native mutex.
   * - Available via
     - ``EbOs.h``

.. list-table:: EbOs_MutexDelete
   :header-rows: 1
   :class: specification-fields

   * - Field
     - Value
   * - Service name
     - ``EbOs_MutexDelete``
   * - Syntax
     - ``void EbOs_MutexDelete(EbOs_MutexType mutex)``
   * - Execution and reentrancy
     - Synchronous. The composition owner serializes lifecycle calls.
   * - Parameters (in)
     - ``mutex`` quiescent handle.
   * - Parameters (inout)
     - None.
   * - Parameters (out)
     - None.
   * - Return value
     - None.
   * - Description
     - Frees the native mutex and wrapper.
   * - Available via
     - ``EbOs.h``

.. list-table:: EbOs_Delay
   :header-rows: 1
   :class: specification-fields

   * - Field
     - Value
   * - Service name
     - ``EbOs_Delay``
   * - Syntax
     - ``void EbOs_Delay(uint32 ms)``
   * - Execution and reentrancy
     - Synchronous. Different tasks may delay independently.
   * - Parameters (in)
     - ``ms`` duration in milliseconds.
   * - Parameters (inout)
     - None.
   * - Parameters (out)
     - None.
   * - Return value
     - None.
   * - Description
     - Delays the current task for the computed tick count.
   * - Available via
     - ``EbOs.h``

9 Sequence diagrams
-------------------

#. The owner creates a mutex and publishes the handle.
#. A client takes the mutex, performs protected work, and releases it.
#. After all clients are quiescent, the owner deletes the mutex.

Configured task lifecycle:

.. code-block:: text

   serialized owner                  EbOs trampoline
        |                                  |
     TaskCreate ----------------------> call product entry
        |                             run until cooperative return
        |                             cache final stack minimum
        |                             publish COMPLETE
        |                             suspend native task
     TaskJoinAndDelete <---------------+
        | waits for COMPLETE and suspended state
        | delete native task, free wrapper, clear owner handle

On timeout the last three retirement operations do not occur. The owner retains
the task handle and may join again after the entry completes.

10 Configuration specification
------------------------------

.. list-table:: Scheduler tick frequency
   :header-rows: 1
   :class: specification-fields

   * - Field
     - Value
   * - Owner
     - ``sdkconfig.defaults``
   * - Type
     - ESP-IDF SDK configuration
   * - Default
     - The firmware inherits the SDK default. The local ESP32-S3 configuration resolves to 100 Hz.
   * - Units or allowed values
     - Ticks per second, from ``configTICK_RATE_HZ``.
   * - Effective
     - At firmware build time.

Project-owned task configuration
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

``EbOs_TaskConfigType`` is defined by EbOs. A product supplies values and entry
bindings in ``Conf/EbOs/OsShell`` and passes configuration into EbOs. The implementation
does not include product configuration headers or know any product symbols.

.. list-table:: Task configuration
   :header-rows: 1
   :widths: 22 45 33

   * - Field
     - Meaning
     - Pilot value
   * - ``name``
     - Diagnostic task name
     - Product-selected camera worker name
   * - ``stack_bytes``
     - Native stack allocation in bytes
     - ``8192``
   * - ``priority``
     - Priority on the selected FreeRTOS scale
     - ``5``
   * - ``core_affinity``
     - ``-1`` for unpinned, otherwise an available core
     - ``-1`` unless the existing producer benchmark override is selected
   * - ``entry``
     - Product-bound ``void (*)(void *)`` entry
     - Product task wiring calls the CamPipe producer

The product keeps its startup/shutdown ordering in its composition. Entry
functions and argument data remain alive until successful join. Typed ``const``
C configuration is independent of the allocation policy: this pilot retains
native dynamic task allocation.

11 Task API additions
---------------------

All handles are opaque. The additions use the existing ``Neo_StatusType``;
existing mutex and delay signatures retain their behavior.

.. list-table:: Managed task operations
   :header-rows: 1
   :widths: 30 70

   * - Interface
     - Contract
   * - ``EbOs_TaskType``
     - Opaque managed-task handle. Never reinterpret it as a native task handle.
   * - ``EbOs_TaskCreate(config, argument, out_task)``
     - Validate configuration, allocate metadata and create the native task.
       Report bad parameters/context or allocation failure without publishing a
       partial task. A fast entry may already be complete on successful return.
   * - ``EbOs_TaskJoinAndDelete(task, timeout_ms)``
     - ``task`` is a pointer to the owner's handle. Wait for cooperative
       completion and suspension, delete and clear it on success. Timeout
       preserves it. A null owned handle is already joined and succeeds.
       No arbitrary task deletion is exposed.
   * - ``EbOs_TaskGetStackMinFree(task, out_bytes)``
     - Report the minimum unused stack in bytes while the handle remains valid.
       Serialize the call against join and deletion.
   * - ``EbOs_DelayOneTick()``
     - Delay the current task by one actual scheduler tick, independent of
       millisecond rounding. At the pilot's 100 Hz this is a 10 ms tick.

Invalid pointers/configuration return ``NEO_E_PARAM``; ISR context or self-join
returns ``NEO_E_STATE``; allocation failure returns ``NEO_E_MEMORY``; an unfinished
join returns ``NEO_E_TIMEOUT``. Success is ``NEO_OK``. The header and tests in the
same source revision are authoritative for exact parameter types.

.. _sws-ebos-compatibility:

12 AUTOSAR R26-11 compatibility profile
---------------------------------------

This is a staged adaptation profile, not a declaration of AUTOSAR OS conformance.
The selected references are Classic Platform R26-11 OS requirements (Document 8)
and specification (Document 34). The six task services have an implementation
for the :ref:`basic-task profile <ebos-basic-task-profile>`. The three lifecycle
services retain declarations only. The catalog records selected runtime evidence
and keeps all AUTOSAR requirement statuses contextual or partial.

.. list-table:: Compatibility matrix
   :header-rows: 1
   :widths: 23 28 49

   * - Concept / reference
     - Status in this milestone
     - Difference or required future evidence
   * - Configured native tasks
     - Typed C configuration and native persistent tasks
     - Native-helper lifecycle remains separate from configured task activations.
       The existing persistent camera worker stays in this profile.
   * - Basic task activation / termination
     - Six services implemented within a limited profile
     - Fully preemptive basic C entries, one pinned core, unique priorities and
       bounded running/queued activations. No equal-priority or non-preemptive
       scheduling claim; native ISR calls rejected without category 2 dispatch.
   * - Missing-end handling
     - Entry retirement and optional NeoTech diagnostic
     - Diagnostic precedes retirement but does not implement standard ErrorHook
       or Pre/PostTaskHook. Resource/interrupt recovery remains outside scope.
   * - Extended-task events
     - Not implemented
     - Preserve wait-mask matching, retained flags and explicit clearing;
       notification counting is not automatically event semantics.
   * - Resources
     - Task mutexes with FreeRTOS priority inheritance
     - AUTOSAR resources use priority ceiling and same-core rules. Mutex APIs
       are not presented as ``GetResource`` / ``ReleaseResource``.
   * - Alarms, counters and schedule tables
     - Not implemented
     - Future native timer callbacks release work to tasks; specify counter
       ranges, expiry, overflow and missed-release behavior separately.
   * - OS startup
     - Standard lifecycle interfaces specified; runtime not implemented
     - ESP-IDF starts the kernel before product entry. Native object
       initialization does not implement ``StartOS`` or ``ShutdownOS``.
   * - OS applications, memory/timing protection and scalability classes
     - Not implemented
     - MMD's SC3 selection does not transfer to ESP-IDF. Full compatibility
       requires a separate platform and conformance feasibility assessment.

Future timers should use existing ESP-IDF facilities rather than a new tick
interrupt. Long camera work and blocking network operations must not execute
inside serialized timer callbacks. See
`ESP Timer 6.0.1 <https://docs.espressif.com/projects/esp-idf/en/v6.0.1/esp32s3/api-reference/system/esp_timer.html>`__.

13 Verification and acceptance
------------------------------

Module tests compile the real EbOs implementation against a controlled native
backend, exercising configuration rejection, allocation failure, immediate
completion, the completion-to-suspension transition, timeout/retry, repeated
start/stop and stack reporting. A second-product fixture supplies different task
configuration without adding product references to EbOs. Existing integration
tests continue to cover mutex/delay consumers.

Run ``python test/run_host_tests.py`` from this repository. The included
``test/fixtures/second_project.c`` supplies a second product's configuration
without requiring NeoCam headers or symbols.

Firmware builds on Windows and Ubuntu establish SDK integration; strict Sphinx
builds establish documentation consistency. They do not establish timing or
safe cross-core retirement on the physical target. The hardware acceptance gate
requires dual-core lifecycle stress, camera startup/stream/photo/shutdown,
timeout/retry behavior and stack observations. Until such a run is recorded,
hardware acceptance remains pending.

The dedicated ``test/on_target`` app builds the real adapter for ESP32-S3 and
exercises pinned/unpinned tasks, immediate returns, cooperative timeout/retry,
stack sampling and retained heap. Follow ``test/README.md`` for build, flash and
monitor commands. Hardware evidence must include
``PASS EBOS_DUAL_CORE_LIFECYCLE`` from the monitor and a separate normal NeoCam
camera-behavior check. Building the app alone does not satisfy this gate.

Standard-interface checks validate declaration signatures, independent header
compilation and source trace/catalog consistency. Those interface checks do not
link the three unimplemented lifecycle services. Separately,
``test/test_os_tasks.c`` executes the real configured-task backend against a
controlled native implementation to verify admission, nonreturn termination,
chaining, queries, allocation rollback and deinit retry. Host results do not
establish physical scheduling or AUTOSAR OS conformance.

14 Configured basic-task lifecycle
----------------------------------

``EbOs_OsInit`` and ``EbOs_OsDeinit`` are explicit NeoTech extensions declared
in ``OsShell/EbOs_Os.h``. Configuration validation and native worker allocation
are all-or-nothing, and workers cannot enter project code before initialization
completes. Initialization does not activate any task. Product composition must
request work with ``ActivateTask`` after successful initialization.

The project owns IDs and immutable ``Os_Config`` storage under ``Conf/EbOs/Os``.
The dormant ``Os_Task_ProjectCheck`` example exercises queries and termination
without joining camera/HTTP/Wi-Fi startup. EbOs references no product symbols.
See :ref:`the profile configuration example <ebos-basic-task-profile>`.

The owner serializes lifecycle operations with every standard task call and
keeps configuration alive until successful deinit. Active or not-yet-parked
workers produce ``NEO_E_BUSY`` without deletion or lost state. The owner retries
after quiescence. This mechanism does not implement ``StartOS`` or ``ShutdownOS``.

Successful termination/chaining abandon the current C call chain through a live,
same-worker nonlocal exit. Entries must not cross variable-length arrays or C++
destruction frames, must not block and must release native locks first. The
native worker remains allocated for future activations. These limitations also
apply to helpers called by an entry. No force-stop API is supplied.

Native ISR calls are rejected. Task calls with masked interrupts return
``E_OS_DISABLEDINT`` before mutation; suspended-scheduler calls return
``E_OS_CALLEVEL``. Query output pointers are null-checked but not memory-protected.
The optional missing-end diagnostic receives the task ID and ``E_OS_MISSINGEND``
before activation retirement and may
query identity/state; termination, chaining and scheduling from that hook are
rejected. It is a NeoTech callback, not standard ErrorHook.
