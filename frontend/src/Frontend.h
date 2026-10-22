#ifndef FRONTEND_H
#define FRONTEND_H

#include <string>

#include "../../scanner/src/scanner.h"
#include "../../parser/src/Grammar.h"
#include "../../parser/src/LR1.h"

using namespace std;

class Frontend {
    Scanner scanner;
    LR1 parser;

    Grammar loadGrammar(string parser_file);

public:
    Frontend(string parser_file, string scanner_file, bool verboseScanner = false);

    bool run();
};

#endif