#pragma once

#include <string>

namespace chocopy {

struct SourceLocation {
    int line = 0;
    int col = 0;
};

struct Diagnostic {
    SourceLocation location;
    std::string message;
};

}
