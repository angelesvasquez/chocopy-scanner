#pragma once

#include "SymbolTable.h"

namespace chocopy {

std::vector<Diagnostic> checkTypes(ast::Node& program, const SymbolTable& symbols);

}
