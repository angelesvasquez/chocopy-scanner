#include "SymbolTable.h"
#include <utility>

namespace chocopy {
namespace {

bool isVariable(const Symbol* symbol) {
    return symbol && (symbol->kind == SymbolKind::Variable || symbol->kind == SymbolKind::Parameter);
}

bool isDirective(SymbolKind kind) {
    return kind == SymbolKind::Global || kind == SymbolKind::Nonlocal;
}

}

const Symbol* Symbol::definition() const {
    return isDirective(kind) ? target : this;
}

const Symbol* Scope::local(const std::string& name) const {
    const auto it = entries_.find(name);
    return it == entries_.end() ? nullptr : &it->second;
}

const Symbol* Scope::lookup(const std::string& name) const {
    for (auto scope = this; scope; scope = scope->parent()) {
        if (const auto* symbol = scope->local(name)) return symbol;
    }
    return nullptr;
}

Symbol* Scope::insert(const std::string& name, SymbolKind kind, const ast::Node* declaration) {
    const auto [it, inserted] = entries_.try_emplace(name, Symbol{kind, this, declaration});
    return inserted ? &it->second : nullptr;
}

SymbolTable::SymbolTable(const ast::Node& program)
    : module_(&createScope(program, ScopeKind::Module, nullptr)) {
    for (const auto* name : {"object", "int", "bool", "str"}) {
        module_->insert(name, SymbolKind::Class);
    }
    for (const auto* name : {"print", "input", "len"}) {
        module_->insert(name, SymbolKind::Function);
    }

    const auto& declarations = *program.children.at(0);
    registerDeclarations(declarations, *module_);
    visit(program, *module_);
}

const Scope* SymbolTable::scopeFor(const ast::Node& node) const {
    const auto it = scopes_.find(&node);
    return it == scopes_.end() ? nullptr : it->second.get();
}

const Symbol* SymbolTable::bindingFor(const ast::Node& node) const {
    const auto it = bindings_.find(&node);
    return it == bindings_.end() ? nullptr : it->second;
}

Scope& SymbolTable::createScope(const ast::Node& owner, ScopeKind kind, const Scope* parent) {
    const auto it = scopes_.emplace(&owner, std::make_unique<Scope>(kind, parent)).first;
    return *it->second;
}

void SymbolTable::report(const ast::Node& node, std::string message) {
    diagnostics_.push_back({node.location, std::move(message)});
}

Symbol* SymbolTable::declare(const ast::Node& node, Scope& scope, SymbolKind kind) {
    auto* symbol = scope.insert(node.text, kind, &node);
    if (!symbol) {
        report(node, "Nombre '" + node.text + "' redeclarado en el mismo ambito.");
        return nullptr;
    }
    const auto* global = module_->local(node.text);
    if (scope.kind() != ScopeKind::Module && !isDirective(kind) &&
        global && global->kind == SymbolKind::Class) {
        report(node, "No se puede ocultar el nombre de clase '" + node.text + "'.");
    }
    if (!isDirective(kind)) bindings_.emplace(&node, symbol);
    return symbol;
}

void SymbolTable::registerDeclarations(const ast::Node& list, Scope& scope) {
    std::vector<Symbol*> directives;
    for (const auto& declaration : list.children) {
        const auto& kind = declaration->kind;
        if (kind == "VarDef") {
            declare(*declaration, scope, SymbolKind::Variable);
        } else if (kind == "FuncDef") {
            declare(*declaration, scope, SymbolKind::Function);
        } else if (kind == "ClassDef") {
            declare(*declaration, scope, SymbolKind::Class);
        } else if (kind == "GlobalDecl" || kind == "NonlocalDecl") {
            const auto directive = kind == "GlobalDecl" ? SymbolKind::Global : SymbolKind::Nonlocal;
            if (auto* symbol = declare(*declaration, scope, directive)) directives.push_back(symbol);
        }
    }
    for (auto* directive : directives) bindDirective(*directive);
}

void SymbolTable::bindDirective(Symbol& symbol) {
    const auto& node = *symbol.declaration;
    if (symbol.kind == SymbolKind::Global) {
        const auto* target = module_->local(node.text);
        if (symbol.scope->kind() == ScopeKind::Function && isVariable(target)) {
            symbol.target = target;
        } else {
            report(node, "'global " + node.text + "' requiere una variable declarada en el ambito global.");
        }
    } else {
        const auto* parent = symbol.scope->parent();
        const auto* visible = parent ? parent->lookup(node.text) : nullptr;
        const auto* target = visible ? visible->definition() : nullptr;
        if (symbol.scope->kind() == ScopeKind::Function && isVariable(target) &&
            target->scope->kind() == ScopeKind::Function) {
            symbol.target = target;
        } else {
            report(node, "'nonlocal " + node.text + "' requiere una variable o parametro de una funcion envolvente.");
        }
    }
    if (symbol.target) bindings_.emplace(&node, symbol.target);
}

void SymbolTable::visitFunction(const ast::Node& node, Scope& scope) {
    // Class members are found through the receiver, outside the lexical chain of a method.
    const Scope* parent = scope.kind() == ScopeKind::Class ? scope.parent() : &scope;
    auto& function = createScope(node, ScopeKind::Function, parent);
    for (const auto& parameter : node.children.front()->children) {
        declare(*parameter, function, SymbolKind::Parameter);
    }
    const auto& body = *node.children.back();
    registerDeclarations(*body.children.at(0), function);
    for (const auto& child : node.children) visit(*child, function);
}

void SymbolTable::visitClass(const ast::Node& node, Scope& scope) {
    auto& members = createScope(node, ScopeKind::Class, &scope);
    resolveName(*node.children.at(0), scope);
    const auto& body = *node.children.at(1);
    registerDeclarations(body, members);
    visit(body, members);
}

void SymbolTable::resolveName(const ast::Node& node, const Scope& scope, bool write) {
    const auto* binding = scope.lookup(node.text);
    if (!binding) {
        report(node, "Nombre '" + node.text + "' no declarado.");
        return;
    }
    const auto* target = binding->definition();
    if (!target) return;  // The invalid directive already has a diagnostic.
    if (write && !isVariable(target)) {
        report(node, "'" + node.text + "' no es una variable asignable.");
    } else if (write && binding->scope != &scope) {
        const std::string directive = target->scope->kind() == ScopeKind::Module ? "global" : "nonlocal";
        report(node, "Asignar a '" + node.text + "' requiere declarar '" + directive + " " + node.text + "'.");
    } else {
        bindings_.emplace(&node, target);
    }
}

void SymbolTable::visitTarget(const ast::Node& node, Scope& scope) {
    if (node.kind == "Id") {
        resolveName(node, scope, true);
    } else {
        visit(node, scope);
    }
}

void SymbolTable::visit(const ast::Node& node, Scope& scope) {
    if (node.kind == "FuncDef") {
        visitFunction(node, scope);
    } else if (node.kind == "ClassDef") {
        visitClass(node, scope);
    } else if (node.kind == "Id") {
        resolveName(node, scope);
    } else if (node.kind == "NamedType") {
        const auto* type = module_->local(node.text);
        if (type && type->kind == SymbolKind::Class) bindings_.emplace(&node, type);
    } else if (node.kind == "Assign") {
        for (const auto& target : node.children.at(0)->children) visitTarget(*target, scope);
        visit(*node.children.at(1), scope);
    } else if (node.kind == "For") {
        visitTarget(*node.children.at(0), scope);
        visit(*node.children.at(1), scope);
        visit(*node.children.at(2), scope);
    } else {
        for (const auto& child : node.children) visit(*child, scope);
    }
}

}
