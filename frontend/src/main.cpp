#include "Frontend.h"
#include <exception>
#include <iostream>
#include <string>

int main(int argc, char **argv) {
  const char *usage = "Uso: python choco.py archivo_entrada.py [--trace] "
                      "[--report-json RUTA | --ast-json RUTA]\n";
  if (argc < 2) {
    std::cerr << usage;
    return 2;
  }

  try {
    const std::filesystem::path sourceFile(argv[1]);
    bool trace = false;
    std::filesystem::path astFile;
    std::filesystem::path reportFile;
    for (int i = 2; i < argc; ++i) {
      const std::string option(argv[i]);
      if (option == "--trace" && !trace) {
        trace = true;
      } else if (astFile.empty() && reportFile.empty() && i + 1 < argc &&
                 (option == "--ast-json" || option == "--report-json")) {
        auto &destination = option == "--ast-json" ? astFile : reportFile;
        destination = argv[++i];
      } else {
        std::cerr << usage;
        return 2;
      }
    }
    Frontend frontEnd(sourceFile);
    return frontEnd.run(trace, astFile, reportFile) ? 0 : 1;
  } catch (const std::exception &error) {
    std::cerr << "Error del compilador: " << error.what() << '\n';
    return 2;
  }
}
