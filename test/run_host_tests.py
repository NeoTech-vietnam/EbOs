#!/usr/bin/env python3
"""Compile the real EbOs backend against deterministic fake ESP-IDF facilities."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--contracts", type=Path, default=ROOT.parent / "NeoContracts")
    parser.add_argument("--sanitize", action="store_true")
    args = parser.parse_args()
    if not (args.contracts / "Neo_Types.h").is_file():
        parser.error("Supply --contracts <NeoContracts checkout>")
    output = ROOT / "test/.build"
    output.mkdir(exist_ok=True)
    cc = shutil.which(os.environ.get("CC", "cc"))
    if not cc:
        parser.error("Set CC to a native C11 compiler")
    environment = dict(os.environ, PATH=str(Path(cc).parent) + os.pathsep + os.environ.get("PATH", ""))
    flags = ["-std=c11", "-Wall", "-Wextra", "-Werror", "-pedantic", "-g"]
    if args.sanitize:
        flags += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-no-pie"]
    subprocess.run([cc, *flags, "-I" + str(ROOT / "test/fakes"), "-I" + str(ROOT),
                    "-fsyntax-only", str(ROOT / "test/test_os_contracts.c")],
                   env=environment, check=True)
    print("PASS R26-11 declaration signatures and profile types", flush=True)
    subprocess.run([sys.executable, str(ROOT / "test/test_traceability.py")], check=True)
    for cores in (1, 2):
        binary = output / (f"ebos-{cores}" + (".exe" if os.name == "nt" else ""))
        subprocess.run([cc, *flags, f"-DconfigNUMBER_OF_CORES={cores}",
                        "-I" + str(ROOT / "test/fakes"), "-I" + str(args.contracts.resolve()),
                        "-I" + str(ROOT), str(ROOT / "EbOs_EspIdf.c"),
                        str(ROOT / "test/test_ebos.c"), str(ROOT / "test/fixtures/second_project.c"),
                        "-o", str(binary)], env=environment, check=True)
        subprocess.run([str(binary)], env=environment, check=True)


if __name__ == "__main__":
    main()
