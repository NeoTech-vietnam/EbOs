# EbOs

Reusable ESP-IDF OS adaptation, maintained independently of each product.

EbOs owns public contracts, configuration types and validation, task/mutex
mechanisms, diagnostics and tests. Products supply typed C task configuration
and entry-function wiring under `Conf/EbOs`; startup order belongs to their
composition. No camera or HTTP symbols belong in EbOs.

The camera-worker pilot adds opaque managed tasks with cooperative completion,
bounded join-and-delete and stack-minimum reporting. Existing mutex and delay
interfaces remain available. Native dynamic task allocation is retained;
typed C configuration does not require static RTOS memory.

The selected reference is **AUTOSAR Classic R26-11**. The six task services in
`Os/Os.h` now have an ESP-IDF basic-task implementation: `ActivateTask`,
`TerminateTask`, `ChainTask`, `Schedule`, `GetTaskID` and `GetTaskState`.
`StartOS`, `ShutdownOS` and `GetActiveApplicationMode` remain specified, not
implemented. ESP-IDF 6.0.1 owns the scheduler, drivers and native tasks.
Existing camera, HTTP and Wi-Fi execution is unchanged.

The first task profile supports fully preemptive basic C tasks, distinct
priorities and one explicitly pinned core. Activation limits include running
and queued work. Successful termination/chaining never return to application
code. ISRs, events, resource/spinlock services, protection and kernel lifecycle
are outside this profile; this is not a claim of AUTOSAR OS conformance.

The NeoTech file templates and Doxygen `TRACE[...]` comments identify the source
of each contract. AUTOSAR `SRS_Os_*` / `SWS_Os_*` traces provide requirements
context; separate `NEO_EBOS_*` IDs identify implemented native-adapter extensions.
Neither the declarations nor matching comment style establish OS conformance.

## MMD-style source groups

The earlier flat source layout is now classified by responsibility:

```text
Before: EbOs.h, EbOs_EspIdf.c, Os.h, Os_Types.h at the package root

EbOs/
├── Os/
│   ├── Os.h                  # Standard service declarations
│   └── Os_Types.h            # Standard contract types/profile values
├── OsShell/
│   ├── EbOs.h                # Reusable native-task/mutex interface
│   └── EbOs_Os.h             # Basic-task profile types and init/deinit
├── Os_EspIdf/
│   ├── EbOs_EspIdf.c         # Native helpers
│   └── Os_EspIdf.c           # Configured task-service implementation
├── module.cmake              # Package source/include metadata
├── docs/
├── requirements/
└── test/

Conf/EbOs/                    # Owned by the integrating project
├── Os/
│   ├── Os_Cfg.h              # Configured basic-task IDs
│   └── Os_Cfg.c              # Project task table and entry wiring
└── OsShell/
    ├── EbOs_Cfg.h            # Existing native camera task policy
    └── EbOs_Cfg.c            # Existing native camera entry/runnable wiring
```

This follows MMD's separation of standard OS integration, reusable shell
mechanisms and platform adaptation. Only groups with current content are created;
Bosch-specific package names and unused MMD features are not copied. Package
metadata, requirements, documentation and shared verification stay at the root.

`module.cmake` exports the classified include directories. Consumers continue to
write `#include "Os.h"` or `#include "EbOs.h"`; no forwarding headers are kept at
the package root.

## Use from project composition

Keep names, stacks, priorities, activation limits and entry bindings in
`Conf/EbOs/Os`. EbOs accepts an immutable configuration; it includes no product
configuration headers. Initialization creates dormant workers and activates
nothing. The NeoCam example task runs only when project composition requests it:

```c
#include "EbOs_Os.h"
#include "Os_Cfg.h"

if (EbOs_OsInit(&Os_Config) == NEO_OK)
{
    StatusType status = ActivateTask(Os_Task_ProjectCheck);
    /* Handle status in project composition. */
    (void)status;
}
```

The composition owner serializes init/deinit with **all six task services** and
keeps configuration storage immutable and alive until successful deinit.
`EbOs_OsDeinit()` returns `NEO_E_BUSY` while work is pending or a worker has not
parked; retry after quiescence. It never force-deletes active work.

Task entries must not block and must release native locks before terminating.
Successful termination/chaining use internal C nonlocal exits: do not cross
variable-length arrays or C++ objects requiring destruction. The optional
`missing_end_hook` receives the task ID and `E_OS_MISSINGEND` as a NeoTech
diagnostic, not standard `ErrorHook`.

Run `python test/run_host_tests.py` for native-helper and task-profile regressions.
Host tests do not establish physical ESP32-S3 scheduling or lifetime safety;
on-device acceptance remains a separate gate.

## Documentation

Read the [published EbOs specification](https://neotech-vietnam.github.io/EbOs/specification.html)
or build the [reStructuredText documentation](docs/index.rst) from this repository.
The [requirements and service catalog](docs/requirements.rst) explains RS versus
SWS, inherited OSEK interfaces, implementation status and verification evidence.

The module documentation describes its source revision. A product using
`ebos/1.0.0@neotech/prototype` still consumes the older mutex/delay snapshot until
its package selection is updated; publishing documentation does not update it.
