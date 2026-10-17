#include <iostream>
#include <string>
#include "scanner.h"

using namespace std;

int main(int argc, char** argv)
{
    string filename = argc > 1 ? argv[1] : "input.txt";
    Scanner scanner(filename);

    while (scanner.gettoken().token != "EOF") {
    }

    return scanner.errors() > 0 ? 1 : 0;
}