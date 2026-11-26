#include "TypeChecker.h"
#include <algorithm>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace chocopy {
namespace {

using Type = std::string;
constexpr auto errorType = "<error>";
constexpr auto noneType = "<None>";
constexpr auto emptyType = "<Empty>";

bool isList(std::string_view type) {
  return !type.empty() && type.front() == '[';
}

Type elementType(const Type &list) { return list.substr(1, list.size() - 2); }

Type listOf(const Type &type) {
  return type == errorType ? type : '[' + type + ']';
}

bool isPrimitive(std::string_view type) {
  return type == "int" || type == "str" || type == "bool";
}

enum class Operand { Int, Bool, Str, List, Reference };

bool matches(Operand operand, std::string_view type) {
  switch (operand) {
  case Operand::Int:
    return type == "int";
  case Operand::Bool:
    return type == "bool";
  case Operand::Str:
    return type == "str";
  case Operand::List:
    return isList(type);
  case Operand::Reference:
    return !isPrimitive(type);
  }
  return false;
}

struct BinaryRule {
  std::string_view op;
  Operand left;
  Operand right;
  std::string_view result;
};

constexpr BinaryRule binaryRules[] = {
    {"+", Operand::Int, Operand::Int, "int"},
    {"+", Operand::Str, Operand::Str, "str"},
    // The result element type is the join of both input element types.
    {"+", Operand::List, Operand::List, ""},
    {"-", Operand::Int, Operand::Int, "int"},
    {"*", Operand::Int, Operand::Int, "int"},
    {"//", Operand::Int, Operand::Int, "int"},
    {"%", Operand::Int, Operand::Int, "int"},
    {"<", Operand::Int, Operand::Int, "bool"},
    {"<=", Operand::Int, Operand::Int, "bool"},
    {">", Operand::Int, Operand::Int, "bool"},
    {">=", Operand::Int, Operand::Int, "bool"},
    {"==", Operand::Int, Operand::Int, "bool"},
    {"==", Operand::Bool, Operand::Bool, "bool"},
    {"==", Operand::Str, Operand::Str, "bool"},
    {"!=", Operand::Int, Operand::Int, "bool"},
    {"!=", Operand::Bool, Operand::Bool, "bool"},
    {"!=", Operand::Str, Operand::Str, "bool"},
    {"is", Operand::Reference, Operand::Reference, "bool"},
    {"and", Operand::Bool, Operand::Bool, "bool"},
    {"or", Operand::Bool, Operand::Bool, "bool"},
};

struct UnaryRule {
  std::string_view op;
  Operand operand;
  const char *result;
};
constexpr UnaryRule unaryRules[] = {{"-", Operand::Int, "int"},
                                    {"not", Operand::Bool, "bool"}};

struct Signature {
  std::vector<Type> parameters;
  Type result;
};

Signature signatureOf(const ast::Node &function, bool bound = false) {
  Signature signature{{}, function.type.value()};
  const auto parameters = ast::SequenceItems(*function.children.front());
  auto parameter = parameters.begin();
  if (bound && parameter != parameters.end())
    ++parameter;
  for (; parameter != parameters.end(); ++parameter) {
    signature.parameters.push_back((*parameter).type.value());
  }
  return signature;
}

class TypeChecker {
public:
  explicit TypeChecker(const SymbolTable &symbols) : symbols_(symbols) {}

  std::vector<Diagnostic> run(ast::Node &program);

private:
  void collectAnnotations(ast::Node &node);
  Type annotation(ast::Node &node);
  void collectHierarchy(ast::Node &declarations);
  void checkClass(ast::Node &node);
  const Symbol *findMember(const Type &receiver,
                           const std::string &member) const;
  void reportMissingMember(const ast::Node &member, const Type &receiver);
  Type parentOf(const Type &type) const;
  bool compatible(const Type &from, const Type &to) const;
  Type join(const Type &left, const Type &right) const;
  void report(const ast::Node &node, std::string message);
  void requireCompatible(const ast::Node &node, const Type &from,
                         const Type &to, std::string context);
  void checkCondition(ast::Node &node);
  bool checkIf(ast::Node &node, const Type *returnType);
  bool checkStatement(ast::Node &node, const Type *returnType);
  Type infer(ast::Node &node);
  Type inferExpression(ast::Node &node);
  Type inferIndex(ast::Node &node, bool write = false);
  Type inferBinary(ast::Node &node);
  Type inferCall(ast::Node &node);

  const SymbolTable &symbols_;
  std::unordered_map<Type, Type> superclasses_{
      {"int", "object"}, {"bool", "object"}, {"str", "object"}};
  std::vector<Diagnostic> diagnostics_;
};

std::vector<Diagnostic> TypeChecker::run(ast::Node &program) {
  collectHierarchy(*program.children.front());
  collectAnnotations(program);
  checkStatement(program, nullptr);
  return std::move(diagnostics_);
}

void TypeChecker::report(const ast::Node &node, std::string message) {
  diagnostics_.push_back({node.location, std::move(message)});
}

Type TypeChecker::annotation(ast::Node &node) {
  if (node.kind == "ListType") {
    node.type = listOf(annotation(*node.children.front()));
  } else if (symbols_.bindingFor(node)) {
    node.type = node.text;
  } else {
    report(node, "Tipo '" + node.text + "' no declarado.");
    node.type = errorType;
  }
  return *node.type;
}

void TypeChecker::collectAnnotations(ast::Node &node) {
  if (node.kind == "VarDef" || node.kind == "Param") {
    node.type = annotation(*node.children.front());
    return;
  }
  if (node.kind == "FuncDef") {
    node.type =
        node.children.size() == 3 ? annotation(*node.children[1]) : noneType;
  }
  if (node.kind == "Seq") {
    for (auto &item : ast::SequenceItems(node))
      collectAnnotations(item);
  } else {
    for (const auto &child : node.children)
      collectAnnotations(*child);
  }
}

void TypeChecker::collectHierarchy(ast::Node &declarations) {
  for (auto &child : ast::SequenceItems(declarations)) {
    if (child.kind != "ClassDef" || !symbols_.bindingFor(child))
      continue;
    auto &base = *child.children.front();
    const auto *symbol = symbols_.bindingFor(base);
    if ((base.text == "object" ||
         (!isPrimitive(base.text) && superclasses_.count(base.text))) &&
        symbol && symbol->kind == SymbolKind::Class) {
      superclasses_.emplace(child.text, base.text);
      base.type = base.text;
    } else {
      if (symbol)
        report(base, "Superclase invalida '" + base.text +
                         "': debe ser object o una clase anterior.");
      superclasses_.emplace(child.text, "object");
      base.type = errorType;
    }
    child.type = child.text;
  }
}

Type TypeChecker::parentOf(const Type &type) const {
  if (type == "object")
    return {};
  const auto it = superclasses_.find(type);
  return it == superclasses_.end() ? "object" : it->second;
}

bool TypeChecker::compatible(const Type &from, const Type &to) const {
  if (from == errorType || to == errorType || from == to)
    return true;
  if (from == noneType)
    return !isPrimitive(to);
  if (isList(to)) {
    return from == emptyType ||
           (from == "[<None>]" && compatible(noneType, elementType(to)));
  }
  for (auto parent = parentOf(from); !parent.empty();
       parent = parentOf(parent)) {
    if (parent == to)
      return true;
  }
  return false;
}

Type TypeChecker::join(const Type &left, const Type &right) const {
  if (left == errorType || right == errorType)
    return errorType;
  if (compatible(left, right))
    return right;
  if (compatible(right, left))
    return left;
  for (auto parent = parentOf(left); !parent.empty();
       parent = parentOf(parent)) {
    if (compatible(right, parent))
      return parent;
  }
  return "object";
}

void TypeChecker::requireCompatible(const ast::Node &node, const Type &from,
                                    const Type &to, std::string context) {
  if (!compatible(from, to)) {
    report(node, std::move(context) + ": se esperaba '" + to +
                     "', se obtuvo '" + from + "'.");
  }
}

const Symbol *TypeChecker::findMember(const Type &receiver,
                                      const std::string &member) const {
  for (auto current = receiver; !current.empty(); current = parentOf(current)) {
    const auto *cls = symbols_.module().local(current);
    if (!cls || cls->kind != SymbolKind::Class || !cls->declaration)
      return nullptr;
    const auto *scope = symbols_.scopeFor(*cls->declaration);
    if (const auto *found = scope->local(member))
      return found;
  }
  return nullptr;
}

void TypeChecker::reportMissingMember(const ast::Node &member,
                                      const Type &receiver) {
  report(member, "El tipo '" + receiver + "' no tiene el miembro '" +
                     member.text + "'.");
}

void TypeChecker::checkClass(ast::Node &node) {
  const auto base = symbols_.bindingFor(node) ? parentOf(node.text) : Type{};
  for (auto &member : ast::SequenceItems(*node.children.at(1))) {
    if (member.kind == "Pass")
      continue;
    if (member.kind == "FuncDef") {
      const auto method = signatureOf(member);
      if (method.parameters.empty() ||
          (method.parameters.front() != errorType &&
           method.parameters.front() != node.text)) {
        report(member, "El primer parametro del metodo '" + member.text +
                           "' debe ser de tipo '" + node.text + "'.");
      }
      if (member.text == "__init__" &&
          (member.children.size() == 3 || method.parameters.size() > 1)) {
        report(member, "'__init__' solo recibe el receptor y no declara un "
                       "tipo de retorno.");
      }
    }
    if (member.text == "__init__" && member.kind != "FuncDef") {
      report(member, "'__init__' debe ser un metodo.");
    } else if (const auto *inherited = findMember(base, member.text)) {
      if (member.kind != "FuncDef" || inherited->kind != SymbolKind::Function) {
        report(member,
               "No se puede redefinir el atributo o miembro heredado '" +
                   member.text + "'.");
      } else {
        const auto method = signatureOf(member, true);
        const auto parentMethod = signatureOf(*inherited->declaration, true);
        const auto sameType = [](const Type &a, const Type &b) {
          return a == b || a == errorType || b == errorType;
        };
        if (!sameType(method.result, parentMethod.result) ||
            !std::equal(method.parameters.begin(), method.parameters.end(),
                        parentMethod.parameters.begin(),
                        parentMethod.parameters.end(), sameType)) {
          report(member, "La firma del metodo '" + member.text +
                             "' difiere de la heredada.");
        }
      }
    }
    checkStatement(member, nullptr);
  }
}

void TypeChecker::checkCondition(ast::Node &node) {
  requireCompatible(node, infer(node), "bool", "Condicion invalida");
}

bool TypeChecker::checkIf(ast::Node &node, const Type *returnType) {
  bool nextBranchReachable = true;
  bool allBranchesReturn = true;
  const auto checkBranch = [&](ast::Node &condition, ast::Node &body) {
    checkCondition(condition);
    const bool returns = checkStatement(body, returnType);
    if (!nextBranchReachable ||
        (condition.kind == "BoolLit" && condition.text == "False"))
      return;
    allBranchesReturn = allBranchesReturn && returns;
    nextBranchReachable = condition.kind != "BoolLit";
  };

  checkBranch(*node.children[0], *node.children[1]);
  for (auto &branch : ast::SequenceItems(*node.children[2])) {
    checkBranch(*branch.children[0], *branch.children[1]);
  }
  if (node.children.size() == 4) {
    const bool returns = checkStatement(*node.children[3], returnType);
    if (nextBranchReachable)
      allBranchesReturn = allBranchesReturn && returns;
    nextBranchReachable = false;
  }
  return allBranchesReturn && !nextBranchReachable;
}

bool TypeChecker::checkStatement(ast::Node &node, const Type *returnType) {
  const auto &kind = node.kind;
  if (kind == "FuncDef") {
    const auto &expected = node.type.value();
    const bool returns = checkStatement(*node.children.back(), &expected);
    if (!returns && isPrimitive(expected)) {
      report(node, "La funcion '" + node.text + "' debe devolver '" + expected +
                       "' en todos los caminos.");
    }
  } else if (kind == "ClassDef") {
    checkClass(node);
  } else if (kind == "VarDef") {
    const auto actual = infer(*node.children.at(1));
    requireCompatible(node, actual, node.type.value(),
                      "Inicializacion de '" + node.text + "'");
  } else if (kind == "Assign") {
    const auto from = infer(*node.children.at(1));
    auto &targets = *node.children.front();
    if (targets.kind == "Seq" && from == "[<None>]") {
      report(node,
             "Una lista de tipo '[<None>]' no admite asignacion multiple.");
    }
    for (auto &target : ast::SequenceItems(targets)) {
      if (target.kind == "Index")
        target.type = inferIndex(target, true);
      else
        infer(target);
      requireCompatible(target, from, target.type.value(),
                        "Asignacion incompatible");
    }
  } else if (kind == "For") {
    auto &variable = *node.children[0];
    auto &iterable = *node.children[1];
    const auto target = infer(variable);
    const auto source = infer(iterable);
    if (source == "str" || isList(source)) {
      requireCompatible(variable,
                        source == "str" ? source : elementType(source), target,
                        "Variable de iteracion incompatible");
    } else if (source != errorType) {
      report(iterable,
             "Solo se puede iterar sobre una lista o str; se obtuvo '" +
                 source + "'.");
    }
    checkStatement(*node.children[2], returnType);
  } else if (kind == "While") {
    auto &condition = *node.children[0];
    checkCondition(condition);
    const bool returns = checkStatement(*node.children[1], returnType);
    return returns && condition.kind == "BoolLit" && condition.text == "True";
  } else if (kind == "If") {
    return checkIf(node, returnType);
  } else if (kind == "Return") {
    const auto type =
        node.children.empty() ? Type(noneType) : infer(*node.children.front());
    if (returnType)
      requireCompatible(node, type, *returnType, "Retorno incompatible");
    else
      report(node, "'return' fuera de una funcion.");
    return true;
  } else if (kind == "ExprStmt") {
    infer(*node.children.front());
  } else if (kind == "Seq") {
    bool returns = false;
    for (auto &item : ast::SequenceItems(node))
      returns = checkStatement(item, returnType) || returns;
    return returns;
  } else {
    bool returns = false;
    for (const auto &child : node.children)
      returns = checkStatement(*child, returnType) || returns;
    return returns;
  }
  return false;
}

Type TypeChecker::infer(ast::Node &node) {
  node.type = inferExpression(node);
  return *node.type;
}

Type TypeChecker::inferBinary(ast::Node &node) {
  const auto left = infer(*node.children[0]);
  const auto right = infer(*node.children[1]);
  if (left == errorType || right == errorType)
    return errorType;
  for (const auto &rule : binaryRules) {
    if (rule.op == node.text && matches(rule.left, left) &&
        matches(rule.right, right)) {
      return rule.result.empty()
                 ? listOf(join(elementType(left), elementType(right)))
                 : Type(rule.result);
    }
  }
  report(node, "Operador '" + node.text + "' incompatible con '" + left +
                   "' y '" + right + "'.");
  return errorType;
}

Type TypeChecker::inferIndex(ast::Node &node, bool write) {
  const auto source = infer(*node.children[0]);
  const auto position = infer(*node.children[1]);
  requireCompatible(*node.children[1], position, "int", "Indice invalido");
  if (source == errorType)
    return errorType;
  if (source == "str" && write) {
    report(node, "No se puede modificar un elemento de str.");
  } else if (source == "str" || isList(source)) {
    if (position != "int")
      return errorType;
    return source == "str" ? source : elementType(source);
  } else {
    report(node, "Solo se puede indexar una lista o str; se obtuvo '" + source +
                     "'.");
  }
  return errorType;
}

Type TypeChecker::inferCall(ast::Node &node) {
  auto &callee = *node.children.front();
  const auto arguments = ast::SequenceItems(*node.children.at(1));
  std::vector<Type> actual;
  for (auto &argument : arguments)
    actual.push_back(infer(argument));

  const bool bound = callee.kind == "Member";
  const Symbol *symbol = nullptr;
  bool defaultInitializer = false;
  if (bound) {
    const auto receiver = infer(*callee.children.front());
    if (receiver == errorType)
      return errorType;
    symbol = findMember(receiver, callee.text);
    const auto *cls = symbols_.module().local(receiver);
    defaultInitializer = !symbol && cls && cls->kind == SymbolKind::Class &&
                         callee.text == "__init__";
    if (!symbol && !defaultInitializer) {
      reportMissingMember(callee, receiver);
      return errorType;
    }
  } else {
    symbol = symbols_.bindingFor(callee);
    if (!symbol)
      return errorType;
  }

  Signature signature;
  if (defaultInitializer) {
    signature = {{}, noneType};
  } else if (symbol->kind == SymbolKind::Class) {
    signature = {{}, callee.text};
  } else if (symbol->kind == SymbolKind::Function) {
    if (symbol->declaration)
      signature = signatureOf(*symbol->declaration, bound);
    else if (callee.text == "input")
      signature = {{}, "str"};
    else if (callee.text == "len")
      signature = {{"object"}, "int"};
    else
      signature = {{"object"}, noneType};
  } else {
    report(callee, "'" + callee.text + "' no es invocable.");
    return errorType;
  }

  bool valid = actual.size() == signature.parameters.size();
  if (!valid) {
    report(node, "La llamada a '" + callee.text + "' espera " +
                     std::to_string(signature.parameters.size()) +
                     " argumento(s), recibio " + std::to_string(actual.size()) +
                     ".");
  }
  std::size_t i = 0;
  for (const auto &argument : arguments) {
    if (i == signature.parameters.size())
      break;
    requireCompatible(argument, actual[i], signature.parameters[i],
                      "Argumento " + std::to_string(i + 1) + " de '" +
                          callee.text + "'");
    valid = valid && actual[i] != errorType &&
            compatible(actual[i], signature.parameters[i]);
    ++i;
  }
  return valid ? signature.result : errorType;
}

Type TypeChecker::inferExpression(ast::Node &node) {
  const auto &kind = node.kind;
  if (kind == "IntLit")
    return "int";
  if (kind == "BoolLit")
    return "bool";
  if (kind == "StrLit")
    return "str";
  if (kind == "NoneLit")
    return noneType;
  if (kind == "BinOp")
    return inferBinary(node);
  if (kind == "Index")
    return inferIndex(node);
  if (kind == "Call")
    return inferCall(node);
  if (kind == "UnaryOp") {
    const auto operand = infer(*node.children.front());
    if (operand == errorType)
      return errorType;
    for (const auto &rule : unaryRules) {
      if (rule.op == node.text && matches(rule.operand, operand))
        return rule.result;
    }
    report(node,
           "Operador '" + node.text + "' incompatible con '" + operand + "'.");
    return errorType;
  }
  if (kind == "Id" || kind == "Member") {
    const Symbol *symbol = nullptr;
    if (kind == "Id") {
      symbol = symbols_.bindingFor(node);
    } else {
      const auto receiver = infer(*node.children.front());
      if (receiver == errorType)
        return errorType;
      symbol = findMember(receiver, node.text);
      if (!symbol)
        reportMissingMember(node, receiver);
    }
    if (!symbol)
      return errorType;
    if (symbol->kind == SymbolKind::Variable ||
        symbol->kind == SymbolKind::Parameter) {
      return symbol->declaration->type.value();
    }
    report(node, "'" + node.text +
                     "' solo se puede usar como destino de una llamada.");
    return errorType;
  }
  if (kind == "ListExpr") {
    const auto values = ast::SequenceItems(*node.children.front());
    if (values.empty())
      return emptyType;
    auto value = values.begin();
    auto element = infer(*value);
    for (++value; value != values.end(); ++value)
      element = join(element, infer(*value));
    return listOf(element);
  }
  if (kind == "IfExpr") {
    checkCondition(*node.children[0]);
    const auto left = infer(*node.children[1]);
    const auto right = infer(*node.children[2]);
    return node.children[0]->type == "bool" ? join(left, right)
                                            : Type(errorType);
  }
  throw std::logic_error("Expresion sin regla de tipos: " + kind);
}

}

std::vector<Diagnostic> checkTypes(ast::Node &program,
                                   const SymbolTable &symbols) {
  return TypeChecker(symbols).run(program);
}

}
