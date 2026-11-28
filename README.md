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

The selected reference is **AUTOSAR Classic R26-11**. `Os.h` and `Os_Types.h`
define a draft contract for nine standard task/lifecycle services. They are
**specified, not implemented**: there are no success stubs or aliases to the
working `EbOs_*` adapter. ESP-IDF 6.0.1 still owns the scheduler, drivers and
native tasks. HTTP and Wi-Fi execution are unchanged.

The NeoTech file templates and Doxygen `TRACE[...]` comments identify the source
of each contract. AUTOSAR `SRS_Os_*` / `SWS_Os_*` traces provide requirements
context; separate `NEO_EBOS_*` IDs identify implemented native-adapter extensions.
Neither the declarations nor matching comment style establish OS conformance.

## Documentation

Read the [published EbOs specification](https://neotech-vietnam.github.io/EbOs/specification.html)
or build the [reStructuredText documentation](docs/index.rst) from this repository.
The [requirements and service catalog](docs/requirements.rst) explains RS versus
SWS, inherited OSEK interfaces, implementation status and verification evidence.

The module documentation describes its source revision. A product using
`ebos/1.0.0@neotech/prototype` still consumes the older mutex/delay snapshot until
its package selection is updated; publishing documentation does not update it.
