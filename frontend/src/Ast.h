#pragma once

#include "../../common/Diagnostic.h"
#include <filesystem>
#include <iosfwd>
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

template <typename NodeType> class SequenceItems {
public:
  explicit SequenceItems(NodeType &root) : root_(&root) {}

  class Iterator {
  public:
    explicit Iterator(NodeType *node) : node_(node) {}
    NodeType &operator*() const {
      return node_->kind == "Seq" ? *node_->children.front() : *node_;
    }
    Iterator &operator++() {
      node_ = node_->kind == "Seq" ? node_->children.back().get() : nullptr;
      return *this;
    }
    bool operator!=(const Iterator &other) const {
      return node_ != other.node_;
    }

  private:
    NodeType *node_;
  };

  bool empty() const { return root_->kind == "Seq" && root_->children.empty(); }
  Iterator begin() const { return Iterator(empty() ? nullptr : root_); }
  Iterator end() const { return Iterator(nullptr); }

private:
  NodeType *root_;
};

} // namespace ast

class Grammar;
class LR1;
class Scanner;

namespace chocopy {

Grammar loadChocoPyGrammar();
ast::NodePtr parseAst(LR1 &parser, Scanner &scanner, bool trace);
void printAst(const ast::Node &node, std::ostream &output, int depth = 0);

class SymbolTable;

struct CompilationReport {
  bool accepted;
  const std::string *sourceText;
  const ast::Node *ast;
  const SymbolTable *symbols;
  const LR1 *parser;
  const std::vector<Diagnostic> &lexical;
  const std::vector<Diagnostic> &semantic;
};

void writeAstJson(const ast::Node &root,
                  const std::filesystem::path &sourceFile,
                  const std::filesystem::path &outputFile);
void writeCompilationReport(const CompilationReport &report,
                            const std::filesystem::path &sourceFile,
                            const std::filesystem::path &outputFile);

} // namespace chocopy
