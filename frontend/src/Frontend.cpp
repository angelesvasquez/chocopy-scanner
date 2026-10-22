#include "Frontend.h"

Grammar Frontend::loadGrammar(string parser_file) {
    Grammar grammar;
    grammar.loadFile(parser_file);
    return grammar;
}

Frontend::Frontend(string parser_file, string scanner_file, bool verboseScanner): scanner(scanner_file, verboseScanner), parser(loadGrammar(parser_file)) {}

bool Frontend::run() {
    if (!parser.build(false)) {
        cout << "La gramatica tiene conflictos; no se puede analizar." << endl;
        return false;
    }

    bool ok = parser.parse(&scanner);
    parser.printTrace();

    cout << "\n" << (ok ? "Analisis sintactico: ACEPTADO" : "Analisis sintactico: RECHAZADO") << " (errores lexicos: " << scanner.errors() << ")" << endl;
    return ok;
}