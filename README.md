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

This is a documented adaptation profile against AUTOSAR Classic R25-11 concepts,
not a conformant AUTOSAR OS. Basic-task activation, events, alarms, schedule
tables and protection are later compatibility stages. ESP-IDF 6.0.1 still owns
the scheduler, drivers and native tasks. HTTP and Wi-Fi execution are unchanged
by this pilot.

## Documentation

Read the [published EbOs specification](https://neotech-vietnam.github.io/EbOs/specification.html)
or build the [reStructuredText documentation](docs/index.rst) from this repository.

The module documentation describes its source revision. A product using
`ebos/1.0.0@neotech/prototype` still consumes the older mutex/delay snapshot until
its package selection is updated; publishing documentation does not update it.
