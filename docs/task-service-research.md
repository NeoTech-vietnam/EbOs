# Core task service research

Reference release: AUTOSAR Classic Platform R26-11. Research date: 2026-11-28.

This note separates requirements inherited from OSEK from AUTOSAR refinements and
NeoTech implementation decisions. It does not establish AUTOSAR OS conformance.

## Inherited task contracts

AUTOSAR `SWS_Os_00001` preserves the OSEK interface. The six task services therefore
inherit OSEK OS 2.2.3 sections 13.2.3.1 through 13.2.3.6.

| Service | Essential inherited behavior |
| --- | --- |
| `ActivateTask` | Record an activation; a suspended task becomes ready and starts at its entry. Excess requests return `E_OS_LIMIT` without activation; invalid IDs return `E_OS_ID` in extended status. |
| `TerminateTask` | Complete the current activation and reschedule. Success never returns; errors return to the caller. Pending activations remain eligible. |
| `ChainTask` | Terminate the caller before releasing its successor. Errors leave the caller active. Self-chaining replaces the current activation without increasing its activation count. |
| `Schedule` | Release/reacquire an internal resource around an eligible higher-priority task. It has no scheduling effect for fully preemptive tasks without internal resources. |
| `GetTaskID` | Return the currently executing task identity, or `INVALID_TASK` when none applies. |
| `GetTaskState` | Return a state snapshot; a running activation takes precedence over queued activations. |

Multiple activations are serialized, restart at the entry, and count against the
configured maximum together with the active activation. Equal-priority requests
are ordered by activation; self-chaining joins the back of that priority queue.
These rules come from OSEK sections 4.3, 4.7 and 13.2.3, not from native FreeRTOS
task creation.

Source: [OSEK OS 2.2.3](https://www.osek-vdx.org/mirror/os223.pdf).

## R26-11 refinements relevant to this implementation

- `SWS_Os_00088`, section 7.7.3.3: reject calls from prohibited contexts without
  performing the requested action. Table 7.1 permits task and category 2 ISR
  activation; termination, chaining and scheduling are task-context services.
  Read-only identity/state services have additional hook contexts. A native IDF ISR
  is not automatically a configured AUTOSAR category 2 ISR.
- `SWS_Os_00093`, section 7.7.3.4: a prohibited non-interrupt service with interrupts
  disabled/suspended has no effect and returns `E_OS_DISABLEDINT` when it has a
  `StatusType` result.
- `SWS_Os_00566`, section 7.1.2.2: extended-status services reject forbidden null
  pointer arguments with `E_OS_ILLEGAL_ADDRESS`. This supports null checks for
  both output references. `SWS_Os_00051` adds protected OS-Application address
  validation, which requires a protection mechanism beyond a null check.
- `SWS_Os_00052` and `SWS_Os_00069`, section 7.7.3.4: a returning task entry is
  terminated; if an error hook is configured, report `E_OS_MISSINGEND` before the
  task leaves the running state. This refines OSEK's original undefined case.
- `SWS_Os_00612`, section 7.9.15: termination and chaining with an occupied spinlock
  return `E_OS_SPINLOCK` in extended status. `SWS_Os_00624`, section 7.9.20 applies
  that restriction to scheduling. A profile without resource/spinlock services
  must disclose that omission rather than synthesize ownership checks.
- Section 7.9.8: identity read from an ISR concerns the interrupted task on the
  calling core. `SWS_Os_00598`, section 7.9.10 requires synchronous cross-core
  activation: acceptance or failure is known before the call returns.
- `ECUC_Os_00074`, section 10.2.32 specifies a positive activation maximum;
  `ECUC_Os_00075` specifies relative task priority with larger values higher;
  `ECUC_Os_00076` distinguishes `FULL` and `NON` scheduling.
- `SWS_Os_91025`, section 7.15 leaves AUTOSAR-specific error encodings to the
  implementation. `StatusType` and `E_OK` remain supplied by Standard Types.

Primary sources: [R26-11 OS specification](https://www.autosar.org/fileadmin/standards/R26-11/CP/AUTOSAR_CP_SWS_OS.pdf),
[R26-11 OS requirements](https://www.autosar.org/fileadmin/standards/R26-11/CP/AUTOSAR_CP_RS_OS.pdf).
The cached PDFs in `_build/standards/R26-11` were used for exact requirement text;
the public AUTOSAR PDF could not be fetched by the browser during this research.

## ESP-IDF 6.0.1 mechanisms

Native workers can be allocated with `xTaskCreatePinnedToCore`; ESP-IDF uses byte
stack sizes. Notifications can release workers while an independent, synchronized
activation counter preserves the configured admission limit. FreeRTOS itself
provides priority preemption; equal-priority time slicing and SMP affinity have
different ordering behavior from OSEK ready queues. Restricting this first profile
to explicitly pinned, fully preemptive basic tasks with distinct priorities on
each core avoids claiming an unsupported equal-priority scheduling policy.

Source: [ESP-IDF 6.0.1 FreeRTOS guide](https://docs.espressif.com/projects/esp-idf/en/v6.0.1/esp32s3/api-reference/system/freertos_idf.html).

The pinned Xtensa port's `xPortCanYield()` reads the processor interrupt level and
is false when that level is nonzero. It can detect masked task context before a
service acquires its own critical section. `xPortInIsrContext()` distinguishes
native interrupt context. These are port-specific checks, not implementations of
AUTOSAR interrupt suspension services.

Source: [ESP-IDF v6.0.1 Xtensa portmacro.h](https://github.com/espressif/esp-idf/blob/v6.0.1/components/freertos/FreeRTOS-Kernel/portable/xtensa/include/freertos/portmacro.h).

## Recommended implementation profile

Keep the configured activation and the persistent native worker as separate
objects. Configuration types and validation belong to EbOs; task IDs, entries,
stacks, priorities, affinity, limits and composition belong under `Conf/EbOs/Os`.
Gate workers until the entire configuration is validated and allocated, since a
new worker can execute before its creation function returns. Failure must not
expose a partially active task registry.

Successful termination/chaining can return internally to a worker activation
frame through C `setjmp`/`longjmp`, while never returning to application code. The
saved environment must belong to the same worker and remain live. Values modified
across the jump must be kept in runtime storage or be `volatile` where C requires
it. Configure C entries only: do not cross C++ destructor frames or rely on cleanup
of automatic resources. Native worker deletion/recreation on every activation
would instead introduce allocation failures and lifetime races into core task
services. Neither choice supplies AUTOSAR protection, timing guarantees or hooks
at every native preemption.

The C language constraints are documented in [WG14 N1570, sections 7.13.1.1 and
7.13.2.1](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf). The choice of
this internal mechanism is a NeoTech design decision, not an AUTOSAR requirement.

Acceptance checks should cover: queued limit accounting; self-chain at the limit;
failed chain preserving the caller; nested successful termination never returning;
entry restart; missing-end reporting while identity is still running; invalid
IDs/output pointers; invalid contexts and masked interrupts without mutation;
allocation rollback; ready/running/suspended snapshots; and concurrency on the
actual dual-core target. Host tests establish admission and lifecycle logic,
not native scheduling or on-device lifetime correctness.
