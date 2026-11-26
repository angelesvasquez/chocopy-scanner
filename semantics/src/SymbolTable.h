#pragma once

#include "../../parser/src/Ast.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace chocopy {

enum class ScopeKind { Module, Class, Function };
enum class SymbolKind {
  Variable,
  Parameter,
  Function,
  Class,
  Global,
  Nonlocal
};

class Scope;

struct Symbol {
  SymbolKind kind;
  const Scope *scope;
  const ast::Node *declaration;
  const Symbol *target = nullptr;

  const Symbol *definition() const;
};

class Scope {
public:
  Scope(ScopeKind kind, const Scope *parent) : kind_(kind), parent_(parent) {}

  ScopeKind kind() const { return kind_; }
  const Scope *parent() const { return parent_; }
  const Symbol *local(const std::string &name) const;
  const Symbol *lookup(const std::string &name) const;
  const std::unordered_map<std::string, Symbol> &entries() const {
    return entries_;
  }

private:
  friend class SymbolTable;
  Symbol *insert(const std::string &name, SymbolKind kind,
                 const ast::Node *declaration = nullptr);

  ScopeKind kind_;
  const Scope *parent_;
  std::unordered_map<std::string, Symbol> entries_;
};

class SymbolTable {
public:
  explicit SymbolTable(const ast::Node &program);

  const Scope &module() const { return *module_; }
  const Scope *scopeFor(const ast::Node &node) const;
  const Symbol *bindingFor(const ast::Node &node) const;
  const std::vector<Diagnostic> &diagnostics() const { return diagnostics_; }

private:
  Scope &createScope(const ast::Node &owner, ScopeKind kind,
                     const Scope *parent);
  Symbol *declare(const ast::Node &node, Scope &scope, SymbolKind kind);
  void registerDeclarations(const ast::Node &list, Scope &scope);
  void bindDirective(Symbol &symbol);
  void visit(const ast::Node &node, Scope &scope);
  void visitClass(const ast::Node &node, Scope &scope);
  void visitFunction(const ast::Node &node, Scope &scope);
  void visitTarget(const ast::Node &node, Scope &scope);
  void resolveName(const ast::Node &node, const Scope &scope,
                   bool write = false);
  void report(const ast::Node &node, std::string message);

  std::unordered_map<const ast::Node *, std::unique_ptr<Scope>> scopes_;
  std::unordered_map<const ast::Node *, const Symbol *> bindings_;
  std::vector<Diagnostic> diagnostics_;
  Scope *module_;
};

} // namespace chocopy
