# EbOs verification

The host regression compiles the real `Os_EspIdf/EbOs_EspIdf.c` against deterministic IDF
fakes. It covers one- and two-core configuration limits, failure cleanup,
immediate completion, completion before native suspension, join timeout/retry,
stack sampling, existing mutex/delay behavior and an unrelated screen-project
configuration. It uses the real `NeoContracts/Neo_Types.h`; the fake standard
types isolate this runtime test from a project's platform-header migration.

The runner obtains standard headers from `Os`, the native interface from
`OsShell`, and the backend from `Os_EspIdf`. Shared test fixtures remain under
`test`; the public include names and verification commands are unchanged.

From this repository, with a C11 compiler and sibling NeoContracts checkout:

```text
python test/run_host_tests.py
```

This command also compiles all nine standard-service signatures without linking
them and runs the offline trace checks. It does not execute any standard OS
service. The existing native-task adapter remains the runtime implementation.

To check project configuration comments as well, run:

```text
python test/test_traceability.py --integration <assembled-workspace>
```

The checker rejects unknown trace IDs, changed signatures, native aliases and
false implementation claims. The catalog records exact R26-11 document URLs,
SHA-256 digests and requirement pages. A reviewer with `pypdf` installed can
place the official PDFs at `_build/standards/R26-11/RS_OS.pdf` and `SWS_OS.pdf`
and verify the primary evidence:

```text
python test/test_traceability.py --source-cache _build/standards/R26-11
```

PDFs remain outside Git. Normal host tests and CI do not download standards or
require `pypdf`; they validate the reviewed, versioned catalog offline.

Use `--contracts <checkout>` when NeoContracts is elsewhere. On Linux,
`--sanitize` enables address/undefined-behavior sanitizers. Host fakes do not
prove cross-core context-switch or native task cleanup behavior.

## Dedicated ESP32-S3 acceptance app

`on_target` compiles the real backend without host fakes. It requires ESP-IDF
6.0.1 and an assembled integration workspace supplying compatible CUCEL and
NeoContracts headers. The default workspace is the sibling `neocam_csw`.
It exercises both pinned cores and unpinned tasks, 100 immediate returns,
10 cooperative timeout/retries, stack samples and retained internal heap.

From an activated ESP-IDF 6.0.1 terminal:

```text
cd test/on_target
idf.py -DIDF_TARGET=esp32s3 -DNEO_INTEGRATION_DIR=<assembled-workspace> build
```

Run the following only when the board is available for this test firmware:

```text
idf.py -p <serial-port> flash monitor
```

Flashing replaces camera firmware on that board. A successful build is not
hardware acceptance: record the monitor output containing
`PASS EBOS_DUAL_CORE_LIFECYCLE` and verify normal NeoCam operation separately.
