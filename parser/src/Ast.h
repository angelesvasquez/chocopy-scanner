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

template <typename NodeType>
class SequenceItems {
public:
    explicit SequenceItems(NodeType& root) : root_(&root) {}

    class Iterator {
    public:
        explicit Iterator(NodeType* node) : node_(node) {}
        NodeType& operator*() const {
            return node_->kind == "Seq" ? *node_->children.front() : *node_;
        }
        Iterator& operator++() {
            node_ = node_->kind == "Seq" ? node_->children.back().get() : nullptr;
            return *this;
        }
        bool operator!=(const Iterator& other) const { return node_ != other.node_; }

    private:
        NodeType* node_;
    };

    bool empty() const { return root_->kind == "Seq" && root_->children.empty(); }
    Iterator begin() const { return Iterator(empty() ? nullptr : root_); }
    Iterator end() const { return Iterator(nullptr); }

private:
    NodeType* root_;
};

}
