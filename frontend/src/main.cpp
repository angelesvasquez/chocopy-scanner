#include <iostream>
#include "Frontend.h"

using namespace std;

int main(int argc, char** argv) {
    string parser_file = argc > 1 ? argv[1] : "../../parser/src/input.txt";
    string scanner_file = argc > 2 ? argv[2] : "../../scanner/src/input.txt";

    Frontend frontEnd(parser_file, scanner_file);
    return frontEnd.run() ? 0 : 1;
}