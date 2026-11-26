#!/usr/bin/env python3

import argparse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import subprocess
from tempfile import TemporaryDirectory
from urllib.parse import parse_qs, urlsplit
import webbrowser


def serve_visualizer(executable: Path, visualizer: Path) -> None:
    class VisualizerHandler(BaseHTTPRequestHandler):
        def send_content(self, status, content_type, content):
            self.send_response(status)
            self.send_header("Content-Type", content_type)
            self.send_header("Content-Length", str(len(content)))
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(content)

        def send_json(self, status, data):
            self.send_content(status, "application/json", json.dumps(data).encode("utf-8"))

        def do_GET(self):
            path = urlsplit(self.path).path
            if path in ("/", "/index.html"):
                self.send_content(200, "text/html; charset=utf-8", (visualizer / "index.html").read_bytes())
            elif path == "/api/compile":
                self.send_content(204, "text/plain", b"")
            else:
                self.send_error(404)

        def do_POST(self):
            request = urlsplit(self.path)
            if request.path != "/api/compile":
                self.send_error(404)
                return
            if self.headers.get("Host") != f"127.0.0.1:{self.server.server_port}":
                self.send_json(403, {"error": "Abre el visualizador desde la URL del lanzador."})
                return
            if self.headers.get_content_type() != "application/octet-stream":
                self.send_json(415, {"error": "Selecciona un archivo fuente ChocoPy."})
                return
            try:
                length = int(self.headers.get("Content-Length", "-1"))
            except ValueError:
                length = -1
            if not 0 <= length <= 1024 * 1024:
                self.send_json(400, {"error": "El archivo debe pesar como máximo 1 MiB."})
                return

            name = parse_qs(request.query).get("name", ["programa.py"])[0]
            try:
                self.connection.settimeout(15)
                content = self.rfile.read(length)
                if len(content) != length:
                    self.send_json(400, {"error": "La carga del archivo quedó incompleta."})
                    return
                with TemporaryDirectory(prefix="chocopy-") as directory:
                    source = Path(directory) / "programa.py"
                    report = Path(directory) / "report.json"
                    source.write_bytes(content)
                    result = subprocess.run(
                        [str(executable), source.name, "--report-json", report.name],
                        cwd=directory, stdout=subprocess.DEVNULL, check=False, timeout=30,
                    )
                    if result.returncode not in (0, 1):
                        self.send_json(500, {"error": "El compilador no pudo completar el análisis."})
                        return
                    data = json.loads(report.read_text(encoding="utf-8"))
            except TimeoutError:
                self.send_json(408, {"error": "La carga del archivo tardó demasiado."})
                return
            except subprocess.TimeoutExpired:
                self.send_json(504, {"error": "El análisis superó el tiempo límite."})
                return
            except (OSError, ValueError):
                self.send_json(500, {"error": "No se pudo generar el informe del compilador."})
                return
            data["source"] = name
            self.send_json(200, data)

    with ThreadingHTTPServer(("127.0.0.1", 0), VisualizerHandler) as server:
        url = f"http://127.0.0.1:{server.server_port}/"
        print(f"Visualizador ChocoPy: {url} (Ctrl+C para cerrar)", flush=True)
        try:
            webbrowser.open(url)
        except webbrowser.Error:
            pass
        try:
            server.serve_forever()
        except KeyboardInterrupt:
            pass


def main() -> int:
    parser = argparse.ArgumentParser(description="Compilador ChocoPy")
    parser.add_argument("programa", nargs="?", type=Path, metavar="archivo_entrada.py",
                        help="archivo fuente ChocoPy. Si se omite, abre el visualizador")
    parser.add_argument("--trace", action="store_true", help="muestra la traza LR(1)")
    export = parser.add_mutually_exclusive_group()
    export.add_argument("--ast-json", type=Path, metavar="RUTA", help="exporta el AST a un archivo JSON")
    export.add_argument(
        "--report-json", type=Path, metavar="RUTA",
        help="cambia el destino del informe (frontend/visualizer/report.json por defecto)",
    )
    args = parser.parse_args()
    if args.programa is None and (args.trace or args.ast_json or args.report_json):
        parser.error("--trace, --ast-json y --report-json requieren un archivo de entrada")

    root = Path(__file__).resolve().parent
    executable_name = "chocopy_core.exe" if os.name == "nt" else "chocopy_core"
    executable = root / "build" / "bin" / executable_name
    if args.programa is None:
        try:
            if not executable.is_file():
                subprocess.run(["cmake", "-S", str(root), "-B", str(root / "build")], check=True)
                subprocess.run(["cmake", "--build", str(root / "build")], check=True)
            serve_visualizer(executable, root / "frontend" / "visualizer")
        except (OSError, subprocess.CalledProcessError) as error:
            parser.exit(2, f"No se pudo iniciar el visualizador ChocoPy: {error}\n")
        return 0
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
