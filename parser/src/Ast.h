#pragma once

#include "../../common/Diagnostic.h"
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace ast {

using Location = chocopy::SourceLocation;

struct Node {
    std::string kind;
    Location location;
    std::string text;
    std::optional<std::string> type;
    std::vector<std::unique_ptr<Node>> children;
};

using NodePtr = std::unique_ptr<Node>;

}
