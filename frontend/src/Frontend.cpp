#include "Frontend.h"

Grammar Frontend::loadGrammar(string parser_file) {
    Grammar grammar;
    grammar.loadFile(parser_file);
    return grammar;
}

Frontend::Frontend(string parser_file, string scanner_file, bool verboseScanner): scanner(scanner_file, verboseScanner), parser(loadGrammar(parser_file)) {}

bool Frontend::run() {
    if (!parser.build(false)) {
        return false;
    }

    parser.setPanicSet({"stmt", "func_decl", "class_item", "decl_item"});
    bool ok = parser.parse(&scanner);
    parser.printTrace();

    cout << "\n" << (ok ? "Analisis sintactico: ACEPTADO" : "Analisis sintactico: RECHAZADO") << " (errores lexicos: " << scanner.errors() << ")" << endl;
    return ok;
}