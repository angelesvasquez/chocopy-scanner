#!/usr/bin/env python3

import argparse
import os
from pathlib import Path
import subprocess


def main() -> int:
    parser = argparse.ArgumentParser(description="Compilador ChocoPy")
    parser.add_argument("programa", type=Path, metavar="archivo_entrada.py", help="archivo fuente ChocoPy")
    parser.add_argument("--trace", action="store_true", help="muestra la traza LR(1)")
    export = parser.add_mutually_exclusive_group()
    export.add_argument("--ast-json", type=Path, metavar="RUTA", help="exporta el AST a un archivo JSON")
    export.add_argument(
        "--report-json", type=Path, metavar="RUTA",
        help="cambia el destino del informe (frontend/visualizer/report.json por defecto)",
    )
    args = parser.parse_args()

    root = Path(__file__).resolve().parent
    executable_name = "chocopy_core.exe" if os.name == "nt" else "chocopy_core"
    executable = root / "build" / "bin" / executable_name
    if not executable.is_file():
        parser.exit(2, "Compila primero con cmake -S . -B build y cmake --build build.\n")

    command = [str(executable), str(args.programa)]
    if args.trace:
        command.append("--trace")
    if args.ast_json is not None:
        command.extend(["--ast-json", str(args.ast_json)])
    else:
        report = args.report_json or root / "frontend" / "visualizer" / "report.json"
        command.extend(["--report-json", str(report)])
    try:
        return subprocess.run(command, check=False).returncode
    except OSError as error:
        parser.exit(2, f"No se pudo iniciar el compilador C++: {error}\n")


if __name__ == "__main__":
    raise SystemExit(main())
