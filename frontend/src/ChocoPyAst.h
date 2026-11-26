#pragma once

#include "../../parser/src/LR1.h"

namespace chocopy {

LR1::AstValue buildNode(const Grammar &grammar, const Production &rule,
                        std::vector<LR1::AstValue> values);
void printAst(const ast::Node &node, std::ostream &output, int depth = 0);

} // namespace chocopy
