"""Check OS profile evidence, source traces and unimplemented-service boundaries."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
TRACE = re.compile(r"TRACE\[([^\]]+)\]")
HEADER_SECTIONS = ("Includes", "Defines / Macros", "Types", "Extern declarations", "Function prototypes")
SOURCE_SECTIONS = ("Includes", "Defines / Macros", "Local types", "Static variables",
                   "Static function prototypes", "Local functions", "Public functions")
INTEGRATION = None
SOURCE_CACHE = None
PRODUCTION_GROUPS = ("Os", "OsShell", "Os_EspIdf")
STANDARD_SERVICES = {"ActivateTask", "TerminateTask", "ChainTask", "Schedule", "GetTaskID",
                     "GetTaskState", "StartOS", "ShutdownOS", "GetActiveApplicationMode"}
PROFILE_SERVICES = {"ActivateTask", "TerminateTask", "ChainTask", "Schedule", "GetTaskID",
                    "GetTaskState"}
PROFILE_BACKEND = "Os_EspIdf/Os_EspIdf.c"
PROFILE_EVIDENCE = ["test/test_os_tasks.c"]
CONTEXT_STATUSES = {"context_only_not_satisfied_by_declarations", "partial_profile_evidence"}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def read_catalog(root):
    return json.loads((root / "requirements/traceability.json").read_text(encoding="utf-8"))


def source_path(root, relative):
    path = Path(relative)
    require(not path.is_absolute() and ".." not in path.parts, "Source path escapes its repository")
    return root / path


def declaration_comment(text, name):
    match = re.search(r"^[ \t]*\w+[ \t]+" + re.escape(name) + r"\s*\([^;{}]*\)\s*;", text, re.M)
    require(match is not None, f"Missing declaration: {name}")
    prefix = text[:match.start()].rstrip()
    start = prefix.rfind("/**")
    require(start >= 0 and prefix.endswith("*/"), f"Missing adjacent Doxygen contract: {name}")
    comment = prefix[start:]
    require(comment.count("*/") == 1, f"Non-adjacent Doxygen contract: {name}")
    require("@brief" in comment and "@details" in comment and "@return" in comment,
            f"Incomplete Doxygen contract: {name}")
    return comment, match.group()


def production_sources(root):
    for group in PRODUCTION_GROUPS:
        for path in (root / group).rglob("*.[ch]"):
            if not {"test", "_build", "docs"}.intersection(path.relative_to(root / group).parts):
                yield path


def verify_sources(catalog, folder):
    # Optional local evidence check: normal CI stays offline and dependency-free.
    from pypdf import PdfReader
    readers = {}
    for key, document in catalog["documents"].items():
        if "sha256" not in document:
            continue
        path = folder / document["cache_file"]
        require(hashlib.sha256(path.read_bytes()).hexdigest() == document["sha256"],
                f"Changed standard PDF: {key}")
        reader = PdfReader(path)
        require(len(reader.pages) == document["pages"], f"Wrong page count: {key}")
        require(catalog["release"] in reader.pages[0].extract_text(), f"Wrong document release: {key}")
        readers[key] = reader
    for identifier, requirement in catalog["requirements"].items():
        if requirement["origin"] != "AUTOSAR":
            continue
        page = readers[requirement["document"]].pages[requirement["page"] - 1].extract_text()
        require("[" + identifier + "]" in page, f"Requirement absent from recorded PDF page: {identifier}")


def validate(root, integration=None, source_cache=None):
    catalog = read_catalog(root)
    require(not list(root.glob("*.[ch]")), "Production C/header files must be classified below EbOs")
    require(catalog["release"] == "R26-11", "Unexpected standard release")
    require(catalog["standard_runtime_implemented"] is True, "Incorrect selected runtime profile")
    require(set(catalog["standard_services"]) == STANDARD_SERVICES, "Incorrect core service coverage")
    known = catalog["requirements"]
    for identifier, requirement in known.items():
        if requirement["origin"] == "AUTOSAR":
            require(requirement["status"] in CONTEXT_STATUSES,
                    f"Incorrect requirement satisfaction claim: {identifier}")
            document = catalog["documents"][requirement["document"]]
            require(document["release"] == catalog["release"] and
                    1 <= requirement["page"] <= document["pages"],
                    f"Incorrect requirement source: {identifier}")
    types = (root / "Os/Os_Types.h").read_text(encoding="utf-8")
    require(not re.search(r"\btypedef\b[^;]*\bStatusType\s*;", types),
            "StatusType must remain owned by CUCEL")
    require(not re.search(r"^\s*#\s*define\s+E_OK\b", types, re.M),
            "E_OK must remain owned by CUCEL")
    seen = set()
    for record in catalog["source_files"]:
        owner = root if record["scope"] == "module" else integration
        if owner is None:
            continue
        path = source_path(owner, record["path"])
        text = path.read_text(encoding="utf-8")
        require(text.startswith("/*<NeoTechHead>") and "</NeoTechHead>*/" in text,
                f"Missing NeoTech header: {path.name}")
        require(f"$Filename__:{path.name}$" in text, f"Incorrect file metadata: {path.name}")
        sections = HEADER_SECTIONS if path.suffix == ".h" else SOURCE_SECTIONS
        positions = [text.find(" * " + name + "\n") for name in sections]
        require(all(position >= 0 for position in positions) and positions == sorted(positions),
                f"Missing or reordered template sections: {path.name}")
        ids = set(TRACE.findall(text))
        require(not ids - set(known), f"Unknown TRACE identifiers: {sorted(ids - set(known))}")
        if path.name in ("EbOs.h", "EbOs_EspIdf.c"):
            require(all(known[identifier]["origin"] == "NeoTech" for identifier in ids),
                    "Native extensions must not claim standard-service traces")
        seen.update(ids)
    required = set(known)
    if integration is None:
        required = {identifier for identifier in required if not identifier.startswith("NEO_EBOS_CFG_")}
    require(required <= seen, f"Unreferenced requirements: {sorted(required - seen)}")
    for name, service in catalog["standard_services"].items():
        implemented = name in PROFILE_SERVICES
        if implemented:
            require(service["status"] == "implemented_profile" and
                    service["runtime_verification"] == PROFILE_EVIDENCE and
                    service["implementation"] == PROFILE_BACKEND,
                    f"Incorrect implementation claim: {name}")
            require(all(source_path(root, item).is_file() for item in PROFILE_EVIDENCE),
                    f"Missing runtime evidence: {name}")
        else:
            require(service["status"] == "specified_not_implemented" and
                    service["runtime_verification"] is None and
                    service.get("implementation") is None,
                    f"Incorrect implementation claim: {name}")
        text = (root / service["header"]).read_text(encoding="utf-8")
        comment, signature = declaration_comment(text, name)
        require(set(service["traces"]) <= set(TRACE.findall(comment)), f"Missing service trace: {name}")
        require(re.sub(r"\s+", "", signature) == re.sub(r"\s+", "", service["signature"]),
                f"Changed standard signature: {name}")
        if implemented:
            require("profile" in comment.lower(), f"Missing implementation profile restriction: {name}")
        else:
            require("not implemented" in comment.lower(), f"Missing declaration-only notice: {name}")
        definitions = []
        for source in production_sources(root):
            body = re.sub(r"/\*.*?\*/|//[^\n]*", "", source.read_text(encoding="utf-8"), flags=re.S)
            require(not re.search(r"^\s*#\s*define\s+" + name + r"\b", body, re.M),
                    f"Standard service alias: {name}")
            pattern = (r"^[ \t]*(?:(?:static|inline|extern)[ \t]+)*"
                       r"(?:StatusType|void|AppModeType|Neo_StatusType|int)[ \t]+" +
                       re.escape(name) + r"\s*\([^;{}]*\)\s*\{")
            for match in re.finditer(pattern, body, re.M):
                require(implemented and source.relative_to(root).as_posix() == PROFILE_BACKEND,
                        f"Unexpected standard service implementation: {name}")
                definition = match.group().rstrip().removesuffix("{").rstrip() + ";"
                require(re.sub(r"\s+", "", definition) == re.sub(r"\s+", "", service["signature"]),
                        f"Changed implementation signature: {name}")
                definitions.append(source)
        require(len(definitions) == (1 if implemented else 0),
                f"Incorrect standard service body count: {name}")
    for name, service in catalog["extensions"].items():
        text = (root / service["header"]).read_text(encoding="utf-8")
        comment, _ = declaration_comment(text, name)
        require(service["trace"] in TRACE.findall(comment), f"Missing extension trace: {name}")
    if source_cache is not None:
        verify_sources(catalog, source_cache)
    return catalog


class TraceabilityTests(unittest.TestCase):
    def fixture(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        root = Path(temporary.name)
        (root / "requirements").mkdir()
        shutil.copy2(ROOT / "requirements/traceability.json", root / "requirements/traceability.json")
        for record in read_catalog(ROOT)["source_files"]:
            if record["scope"] == "module":
                target = root / record["path"]
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(ROOT / record["path"], target)
        for relative in PROFILE_EVIDENCE:
            target = root / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(ROOT / relative, target)
        return root

    def test_actual_profile(self):
        validate(ROOT, INTEGRATION, SOURCE_CACHE)

    def test_unknown_trace_is_rejected(self):
        root = self.fixture()
        path = root / "OsShell/EbOs.h"
        path.write_text(path.read_text() + "\n/* TRACE[SWS_Os_99999] */\n")
        with self.assertRaisesRegex(ValueError, "Unknown TRACE"):
            validate(root)

    def test_implementation_claim_is_rejected(self):
        root = self.fixture()
        catalog = read_catalog(root)
        catalog["standard_services"]["ActivateTask"]["status"] = "implemented"
        (root / "requirements/traceability.json").write_text(json.dumps(catalog))
        with self.assertRaisesRegex(ValueError, "Incorrect implementation claim"):
            validate(root)

    def test_lifecycle_implementation_claim_is_rejected(self):
        root = self.fixture()
        catalog = read_catalog(root)
        catalog["standard_services"]["StartOS"]["status"] = "implemented_profile"
        (root / "requirements/traceability.json").write_text(json.dumps(catalog))
        with self.assertRaisesRegex(ValueError, "Incorrect implementation claim"):
            validate(root)

    def test_missing_runtime_evidence_is_rejected(self):
        root = self.fixture()
        (root / PROFILE_EVIDENCE[0]).unlink()
        with self.assertRaisesRegex(ValueError, "Missing runtime evidence"):
            validate(root)

    def test_lifecycle_body_is_rejected(self):
        root = self.fixture()
        path = root / PROFILE_BACKEND
        path.write_text(path.read_text() + "\nvoid StartOS(AppModeType Mode) {}\n")
        with self.assertRaisesRegex(ValueError, "Unexpected standard service implementation"):
            validate(root)

    def test_duplicate_profile_body_is_rejected(self):
        root = self.fixture()
        path = root / PROFILE_BACKEND
        path.write_text(path.read_text() + "\nStatusType ActivateTask(TaskType TaskID) { return E_OK; }\n")
        with self.assertRaisesRegex(ValueError, "Incorrect standard service body count"):
            validate(root)

    def test_profile_body_outside_backend_is_rejected(self):
        root = self.fixture()
        path = root / "Os_EspIdf/EbOs_EspIdf.c"
        path.write_text(path.read_text() + "\nStatusType ActivateTask(TaskType TaskID) { return E_OK; }\n")
        with self.assertRaisesRegex(ValueError, "Unexpected standard service implementation"):
            validate(root)

    def test_native_alias_is_rejected(self):
        root = self.fixture()
        path = root / "Os/Os.h"
        path.write_text(path.read_text() + "\n#define ActivateTask EbOs_TaskCreate\n")
        with self.assertRaisesRegex(ValueError, "Standard service alias"):
            validate(root)

    def test_changed_signature_is_rejected(self):
        root = self.fixture()
        path = root / "Os/Os.h"
        path.write_text(path.read_text().replace("StatusType ActivateTask(", "void ActivateTask("))
        with self.assertRaisesRegex(ValueError, "Changed standard signature"):
            validate(root)

    def test_status_type_redefinition_is_rejected(self):
        root = self.fixture()
        path = root / "Os/Os_Types.h"
        path.write_text(path.read_text() + "\ntypedef unsigned char StatusType;\n")
        with self.assertRaisesRegex(ValueError, "StatusType must remain owned"):
            validate(root)

    def test_requirement_satisfaction_claim_is_rejected(self):
        root = self.fixture()
        catalog = read_catalog(root)
        catalog["requirements"]["SWS_Os_00001"]["status"] = "satisfied"
        (root / "requirements/traceability.json").write_text(json.dumps(catalog))
        with self.assertRaisesRegex(ValueError, "Incorrect requirement satisfaction claim"):
            validate(root)

    def test_native_alias_in_nested_backend_is_rejected(self):
        root = self.fixture()
        path = root / "Os_EspIdf/EbOs_EspIdf.c"
        path.write_text(path.read_text() + "\n#define ActivateTask EbOs_TaskCreate\n")
        with self.assertRaisesRegex(ValueError, "Standard service alias"):
            validate(root)

    def test_flat_production_header_is_rejected(self):
        root = self.fixture()
        (root / "EbOs.h").write_text('#include "OsShell/EbOs.h"\n')
        with self.assertRaisesRegex(ValueError, "must be classified"):
            validate(root)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--integration", type=Path, help="Also check project-owned Conf/EbOs")
    parser.add_argument("--source-cache", type=Path, help="Verify cached published PDFs using pypdf")
    args = parser.parse_args()
    INTEGRATION, SOURCE_CACHE = args.integration, args.source_cache
    unittest.main(argv=[__file__])
