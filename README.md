# EbOs

Reusable ESP-IDF OS adaptation, maintained independently of each product.

EbOs owns public contracts, configuration types and validation, task/mutex
mechanisms, diagnostics and tests. Products supply typed C task configuration
and entry-function wiring under `Conf/EbOs/OsShell`; startup order belongs to their
composition. No camera or HTTP symbols belong in EbOs.

The camera-worker pilot adds opaque managed tasks with cooperative completion,
bounded join-and-delete and stack-minimum reporting. Existing mutex and delay
interfaces remain available. Native dynamic task allocation is retained;
typed C configuration does not require static RTOS memory.

The selected reference is **AUTOSAR Classic R26-11**. `Os/Os.h` and `Os/Os_Types.h`
define a draft contract for nine standard task/lifecycle services. They are
**specified, not implemented**: there are no success stubs or aliases to the
working `EbOs_*` adapter. ESP-IDF 6.0.1 still owns the scheduler, drivers and
native tasks. HTTP and Wi-Fi execution are unchanged.

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
│   └── EbOs.h                # Reusable native-task/mutex interface
├── Os_EspIdf/
│   └── EbOs_EspIdf.c         # ESP-IDF implementation
├── module.cmake              # Package source/include metadata
├── docs/
├── requirements/
└── test/

Conf/EbOs/                    # Owned by the integrating project
└── OsShell/
    ├── EbOs_Cfg.h            # Project task policy
    └── EbOs_Cfg.c            # Product entry/runnable wiring
```

This follows MMD's separation of standard OS integration, reusable shell
mechanisms and platform adaptation. Only groups with current content are created;
Bosch-specific package names and unused MMD features are not copied. Package
metadata, requirements, documentation and shared verification stay at the root.

`module.cmake` exports the classified include directories. Consumers continue to
write `#include "Os.h"` or `#include "EbOs.h"`; no forwarding headers are kept at
the package root. The folder move preserves interfaces, behavior and trace IDs.

## Documentation

Read the [published EbOs specification](https://neotech-vietnam.github.io/EbOs/specification.html)
or build the [reStructuredText documentation](docs/index.rst) from this repository.
The [requirements and service catalog](docs/requirements.rst) explains RS versus
SWS, inherited OSEK interfaces, implementation status and verification evidence.

The module documentation describes its source revision. A product using
`ebos/1.0.0@neotech/prototype` still consumes the older mutex/delay snapshot until
its package selection is updated; publishing documentation does not update it.
