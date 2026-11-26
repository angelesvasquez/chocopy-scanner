#include "Ast.h"
#include "../../parser/src/LR1.h"
#include "../../semantics/src/SymbolTable.h"
#include <algorithm>
#include <fstream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace chocopy {
namespace {

using ast::Location;
using ast::NodePtr;

template <typename... Children>
NodePtr node(std::string kind, Location location, std::string text,
             Children... children) {
  auto result = std::make_unique<ast::Node>();
  result->kind = std::move(kind);
  result->location = location;
  result->text = std::move(text);
  auto addChild = [&result](NodePtr child) {
    if (child) {
      result->children.push_back(std::move(child));
    }
  };
  if constexpr (sizeof...(children) > 0) {
    (addChild(std::move(children)), ...);
  }
  return result;
}

NodePtr buildSequence(LR1::AstSequence sequence) {
  if (sequence.items.empty())
    return node("Seq", {}, sequence.role);

  auto tail = std::move(sequence.items.back());
  for (std::size_t i = sequence.items.size() - 1; i > 0; --i) {
    auto &head = sequence.items[i - 1];
    const auto start = head->location;
    tail = node("Seq", start, sequence.role, std::move(head), std::move(tail));
  }
  return tail;
}

LR1::AstValue buildNode(const Grammar &grammar, const Production &rule,
                        std::vector<LR1::AstValue> values) {
  const auto &lhs = grammar.symbolTable[rule.left].name;
  const auto rhs = [&](std::size_t i) -> const std::string & {
    return grammar.symbolTable[rule.right.at(i)].name;
  };
  const auto token = [&](std::size_t i) -> const TokenData & {
    return std::get<TokenData>(values.at(i));
  };
  const auto take = [&](std::size_t i) {
    auto &value = values.at(i);
    if (auto *sequence = std::get_if<LR1::AstSequence>(&value)) {
      return buildSequence(std::move(*sequence));
    }
    return std::get<NodePtr>(std::move(value));
  };
  const auto at = [&](std::size_t i) -> Location {
    if (const auto *terminal = std::get_if<TokenData>(&values.at(i))) {
      return {terminal->row, terminal->col};
    }
    const auto &child = std::get<NodePtr>(values.at(i));
    return child ? child->location : Location{};
  };
  const auto sequence = [&](const char *role, std::size_t next) {
    auto items = !values.empty() && rhs(0) == lhs
                     ? std::get<LR1::AstSequence>(std::move(values[0]))
                     : LR1::AstSequence{role, {}};
    if (!values.empty())
      items.items.push_back(take(next));
    return items;
  };
  const auto id = [&](std::size_t i) {
    return node("Id", at(i), token(i).value);
  };
  const auto size = values.size();

  if (lhs == "program") {
    auto declarations = take(0);
    auto statements = size == 2 ? take(1) : node("Seq", {}, "statements");
    Location start = declarations->location.line == 0 ? statements->location
                                                      : declarations->location;
    if (start.line == 0) {
      start = {1, 1};
    }
    return node("Program", start, "", std::move(declarations),
                std::move(statements));
  }
  if (lhs == "decl_list" || lhs == "func_decl_list")
    return sequence("declarations", 1);
  if (lhs == "stmt_plus")
    return sequence("statements", size - 1);
  if (lhs == "class_item_list")
    return sequence("members", size - 1);
  if (lhs == "params")
    return sequence("parameters", size - 1);
  if (lhs == "expr_list")
    return sequence("expressions", size - 1);
  if (lhs == "assign_list")
    return sequence("targets", size - 2);

  if (lhs == "params_opt" && size == 0)
    return node("Seq", {}, "parameters");
  if (lhs == "expr_list_opt" && size == 0)
    return node("Seq", {}, "expressions");
  if ((lhs == "expr_opt" || lhs == "else_opt" || lhs == "ret_opt") && size == 0)
    return NodePtr{};
  if (lhs == "ret_opt")
    return take(1);
  if (lhs == "else_opt")
    return node("Else", at(0), "", take(2));
  if (lhs == "elif_list") {
    if (size == 0)
      return LR1::AstSequence{"elif", {}};
    auto branches = std::get<LR1::AstSequence>(std::move(values[0]));
    branches.items.push_back(node("Elif", at(1), "", take(2), take(4)));
    return branches;
  }

  if (lhs == "class_def")
    return node("ClassDef", at(0), token(1).value, id(3), take(8));
  if (lhs == "class_body" && size == 2)
    return node("Pass", at(0), "");
  if (lhs == "func_def")
    return node("FuncDef", at(0), token(1).value, take(3), take(5), take(9));
  if (lhs == "func_body") {
    auto declarations = take(0);
    auto statements = take(1);
    const auto start = declarations->location.line == 0
                           ? statements->location
                           : declarations->location;
    return node("Body", start, "", std::move(declarations),
                std::move(statements));
  }
  if (lhs == "block")
    return take(2);
  if (lhs == "typed_var")
    return node("Param", at(0), token(0).value, take(2));
  if (lhs == "var_def") {
    auto variable = take(0);
    variable->kind = "VarDef";
    variable->children.push_back(take(2));
    return variable;
  }
  if (lhs == "type") {
    if (size == 3)
      return node("ListType", at(0), "", take(1));
    std::string name = token(0).value;
    if (rhs(0) == "IDSTRING")
      name = name.substr(1, name.size() - 2);
    return node("NamedType", at(0), std::move(name));
  }
  if (lhs == "global_decl")
    return node("GlobalDecl", at(0), token(1).value);
  if (lhs == "nonlocal_decl")
    return node("NonlocalDecl", at(0), token(1).value);

  if (lhs == "stmt") {
    if (rhs(0) == "if")
      return node("If", at(0), "", take(1), take(3), take(4), take(5));
    if (rhs(0) == "while")
      return node("While", at(0), "", take(1), take(3));
    if (rhs(0) == "for")
      return node("For", at(0), "", id(1), take(3), take(5));
    return take(0);
  }
  if (lhs == "simple_stmt") {
    if (rhs(0) == "pass")
      return node("Pass", at(0), "");
    if (rhs(0) == "return")
      return node("Return", at(0), "", take(1));
    auto first = take(0);
    const auto start = first->location;
    if (size == 2)
      return node("Assign", start, "", std::move(first), take(1));
    return node("ExprStmt", start, "", std::move(first));
  }
  if (lhs == "literal") {
    const auto &symbol = rhs(0);
    const auto location = at(0);
    const auto &lexeme = token(0).value;
    if (symbol == "INTEGER")
      return node("IntLit", location, lexeme);
    if (symbol == "STRING" || symbol == "IDSTRING")
      return node("StrLit", location, lexeme);
    if (symbol == "None")
      return node("NoneLit", location, lexeme);
    return node("BoolLit", location, lexeme);
  }

  if (lhs == "expr" && size == 5) {
    const auto start = at(0);
    return node("IfExpr", start, "", take(2), take(0), take(4));
  }
  if ((lhs == "or_expr" || lhs == "and_expr" || lhs == "arith" ||
       lhs == "term") &&
      size == 3) {
    return node("BinOp", at(1), token(1).value, take(0), take(2));
  }
  if ((lhs == "not_expr" || lhs == "unary") && size == 2) {
    return node("UnaryOp", at(0), token(0).value, take(1));
  }
  if (lhs == "cmp_op")
    return node("Operator", at(0), token(0).value);
  if (lhs == "cmp_expr" && size == 3) {
    auto op = take(1);
    return node("BinOp", op->location, std::move(op->text), take(0), take(2));
  }
  if (lhs == "member_expr") {
    const auto start = at(0);
    return node("Member", start, token(2).value, take(0));
  }
  if (lhs == "index_expr") {
    const auto start = at(0);
    return node("Index", start, "", take(0), take(2));
  }
  if (lhs == "primary") {
    if (rhs(0) == "[")
      return node("ListExpr", at(0), "", take(1));
    if (rhs(0) == "(")
      return take(1);
    if (size == 4) {
      const auto start = at(0);
      auto callee = rhs(0) == "ID" ? id(0) : take(0);
      return node("Call", start, "", std::move(callee), take(2));
    }
  }
  if ((lhs == "primary" || lhs == "target") && rhs(0) == "ID")
    return id(0);

  if (size == 1 && !std::holds_alternative<TokenData>(values[0]))
    return std::move(values[0]);
  throw std::logic_error("Produccion sin accion para el AST: " + lhs);
}

namespace fs = std::filesystem;

std::size_t utf8Length(std::string_view text, std::size_t start) {
  const unsigned char first = text[start];
  if (first < 0xC2 || first > 0xF4)
    return 0;
  const std::size_t length = first < 0xE0 ? 2 : first < 0xF0 ? 3 : 4;
  if (text.size() - start < length)
    return 0;

  const unsigned char second = text[start + 1];
  if ((first == 0xE0 && second < 0xA0) || (first == 0xED && second > 0x9F) ||
      (first == 0xF0 && second < 0x90) || (first == 0xF4 && second > 0x8F))
    return 0;
  for (std::size_t i = start + 1; i < start + length; ++i) {
    if ((static_cast<unsigned char>(text[i]) & 0xC0) != 0x80)
      return 0;
  }
  return length;
}

void writeString(std::ostream &out, std::string_view text) {
  constexpr char hex[] = "0123456789abcdef";
  out << '"';
  for (std::size_t i = 0; i < text.size(); ++i) {
    const unsigned char ch = text[i];
    if (ch == '"' || ch == '\\') {
      out << '\\' << static_cast<char>(ch);
    } else if (ch < 0x20) {
      out << "\\u00" << hex[ch >> 4] << hex[ch & 0x0F];
    } else if (ch < 0x80) {
      out << static_cast<char>(ch);
    } else if (const auto length = utf8Length(text, i)) {
      out << text.substr(i, length);
      i += length - 1;
    } else {
      out << "\\ufffd";
    }
  }
  out << '"';
}

void writeNode(std::ostream &out, const ast::Node &node) {
  out << "{\"kind\":";
  writeString(out, node.kind);
  out << ",\"text\":";
  writeString(out, node.text);
  out << ",\"line\":" << node.location.line << ",\"col\":" << node.location.col
      << ",\"type\":";
  if (node.type) {
    writeString(out, *node.type);
  } else {
    out << "null";
  }
  out << ",\"children\":[";
  const char *separator = "";
  for (const auto &child : node.children) {
    out << separator;
    writeNode(out, *child);
    separator = ",";
  }
  out << "]}";
}

template <typename Range, typename Writer>
void writeArray(std::ostream &out, const Range &items, Writer write) {
  out << '[';
  const char *separator = "";
  for (const auto &item : items) {
    out << separator;
    write(out, item);
    separator = ",";
  }
  out << ']';
}

void writeDiagnostics(std::ostream &out,
                      const std::vector<Diagnostic> &diagnostics) {
  writeArray(out, diagnostics, [](auto &stream, const Diagnostic &diagnostic) {
    stream << "{\"line\":" << diagnostic.location.line
           << ",\"col\":" << diagnostic.location.col << ",\"message\":";
    writeString(stream, diagnostic.message);
    stream << '}';
  });
}

const char *scopeKind(ScopeKind kind) {
  switch (kind) {
  case ScopeKind::Module:
    return "modulo";
  case ScopeKind::Class:
    return "clase";
  case ScopeKind::Function:
    return "funcion";
  }
  throw std::logic_error("Ambito desconocido.");
}

const char *symbolKind(SymbolKind kind) {
  switch (kind) {
  case SymbolKind::Variable:
    return "variable";
  case SymbolKind::Parameter:
    return "parametro";
  case SymbolKind::Function:
    return "funcion";
  case SymbolKind::Class:
    return "clase";
  case SymbolKind::Global:
    return "global";
  case SymbolKind::Nonlocal:
    return "nonlocal";
  }
  throw std::logic_error("Simbolo desconocido.");
}

void collectScopes(const ast::Node &node, const SymbolTable &symbols,
                   std::vector<const ast::Node *> &owners) {
  if (symbols.scopeFor(node))
    owners.push_back(&node);
  for (const auto &child : node.children)
    collectScopes(*child, symbols, owners);
}

void writeScopes(std::ostream &out, const ast::Node &ast,
                 const SymbolTable &symbols) {
  std::vector<const ast::Node *> owners;
  collectScopes(ast, symbols, owners);
  std::unordered_map<const Scope *, std::size_t> ids;
  for (std::size_t i = 0; i < owners.size(); ++i)
    ids.emplace(symbols.scopeFor(*owners[i]), i);

  writeArray(out, owners, [&](auto &stream, const ast::Node *owner) {
    const auto *scope = symbols.scopeFor(*owner);
    stream << "{\"id\":" << ids.at(scope) << ",\"name\":";
    writeString(stream, owner->text);
    stream << ",\"kind\":";
    writeString(stream, scopeKind(scope->kind()));
    stream << ",\"line\":" << owner->location.line
           << ",\"col\":" << owner->location.col << ",\"parent\":";
    if (scope->parent())
      stream << ids.at(scope->parent());
    else
      stream << "null";
    stream << ",\"entries\":";
    std::vector<std::string> names;
    for (const auto &entry : scope->entries())
      names.push_back(entry.first);
    std::sort(names.begin(), names.end());
    writeArray(stream, names, [&](auto &entry, const std::string &name) {
      const auto &symbol = scope->entries().at(name);
      const auto *definition = symbol.definition();
      entry << "{\"name\":";
      writeString(entry, name);
      entry << ",\"kind\":";
      writeString(entry, symbolKind(symbol.kind));
      entry << ",\"line\":"
            << (symbol.declaration ? symbol.declaration->location.line : 0)
            << ",\"col\":"
            << (symbol.declaration ? symbol.declaration->location.col : 0)
            << ",\"type\":";
      if (definition && definition->declaration &&
          definition->declaration->type) {
        writeString(entry, *definition->declaration->type);
      } else {
        entry << "null";
      }
      entry << ",\"target\":";
      if (symbol.target)
        entry << ids.at(symbol.target->scope);
      else
        entry << "null";
      entry << '}';
    });
    stream << '}';
  });
}

class ReportStaging {
public:
  ReportStaging(const fs::path &parent, bool withCompanion) {
    if (withCompanion) {
      directory = parent / ".chocopy-ast-export";
      if (!fs::create_directory(directory)) {
        throw std::runtime_error(
            "El directorio temporal de exportacion del AST ya existe.");
      }
      return;
    }
    std::random_device random;
    for (int attempt = 0; attempt < 8; ++attempt) {
      auto candidate = parent / (".chocopy-ast-" + std::to_string(random()));
      if (fs::create_directory(candidate)) {
        directory = std::move(candidate);
        return;
      }
    }
    throw std::runtime_error("No se pudo preparar la escritura del AST JSON.");
  }

  ~ReportStaging() {
    std::error_code error;
    fs::remove(file("ast.json"), error);
    fs::remove(file("ast-data.js"), error);
    fs::remove(file("previous-data.js"), error);
    fs::remove(directory, error);
  }

  fs::path file(const char *filename) const { return directory / filename; }

private:
  fs::path directory;
};

void checkDestination(const fs::path &source, const fs::path &destination) {
  const bool destinationExists = fs::exists(destination);
  const bool sameFile =
      destinationExists && fs::exists(source)
          ? fs::equivalent(source, destination)
          : fs::weakly_canonical(fs::absolute(source)) ==
                fs::weakly_canonical(fs::absolute(destination));
  if (sameFile) {
    throw std::runtime_error(
        "Un destino de la exportacion del AST coincide con el archivo fuente.");
  }
  if (destinationExists && !fs::is_regular_file(destination)) {
    throw std::runtime_error(
        "Un destino de la exportacion del AST no es un archivo.");
  }
}

void stageReport(const fs::path &file, std::string_view data,
                 const char *global = nullptr) {
  std::ofstream out(file, std::ios::binary);
  if (!out) {
    throw std::runtime_error(
        "No se pudo abrir un archivo de exportacion del AST.");
  }
  if (global)
    out << "globalThis." << global << " = ";
  out << data;
  if (global)
    out << ";\n";
  out.close();
  if (!out) {
    throw std::runtime_error(
        "No se pudo guardar un archivo de exportacion del AST.");
  }
}

void publish(const std::string &data, const fs::path &sourceFile,
             const fs::path &outputFile, const char *canonicalName,
             const char *companionName, const char *global) {
  const bool withCompanion = outputFile.filename() == canonicalName;
  const auto companion = outputFile.parent_path() / companionName;
  const ReportStaging staging(outputFile.parent_path(), withCompanion);
  checkDestination(sourceFile, outputFile);
  if (withCompanion)
    checkDestination(sourceFile, companion);

  const auto stagedJson = staging.file("ast.json");
  stageReport(stagedJson, data);
  if (!withCompanion) {
    fs::rename(stagedJson, outputFile);
    return;
  }

  const auto stagedScript = staging.file("ast-data.js");
  const auto previous = staging.file("previous-data.js");
  stageReport(stagedScript, data, global);
  const bool hadCompanion = fs::exists(companion);
  if (hadCompanion)
    fs::copy_file(companion, previous);
  fs::rename(stagedScript, companion);
  try {
    fs::rename(stagedJson, outputFile);
  } catch (...) {
    if (hadCompanion) {
      fs::rename(previous, companion);
    } else {
      fs::remove(companion);
    }
    throw;
  }
}

void writeSource(std::ostream &out, const fs::path &sourceFile) {
  out << "{\n  \"source\": ";
  const auto filename = sourceFile.filename().u8string();
  writeString(out, std::string(filename.begin(), filename.end()));
}

}

Grammar loadChocoPyGrammar() {
  constexpr char grammarText[] = R"ChocoPyGrammar(@CHOCOPY_GRAMMAR@)ChocoPyGrammar";
  std::istringstream input(grammarText);
  Grammar grammar;
  grammar.load(input);
  if (grammar.productions.empty()) {
    throw std::runtime_error("La gramatica ChocoPy esta vacia.");
  }
  return grammar;
}

ast::NodePtr parseAst(LR1 &parser, Scanner &scanner, bool trace) {
  return parser.parse(&scanner, buildNode, trace);
}

void printAst(const ast::Node &node, std::ostream &output, int depth) {
  output << std::string(depth * 2, ' ') << node.kind;
  if (!node.text.empty())
    output << ' ' << node.text;
  if (node.location.line > 0)
    output << " [" << node.location.line << ':' << node.location.col << ']';
  if (node.type)
    output << " : " << *node.type;
  output << '\n';

  for (const auto &child : node.children) {
    printAst(*child, output, depth + 1);
  }
}

void writeAstJson(const ast::Node &root, const fs::path &sourceFile,
                  const fs::path &outputFile) {
  std::ostringstream out;
  writeSource(out, sourceFile);
  out << ",\n  \"ast\": ";
  writeNode(out, root);
  out << "\n}\n";
  publish(out.str(), sourceFile, outputFile, "ast.json", "ast-data.js",
          "chocopyAst");
}

void writeCompilationReport(const CompilationReport &report,
                            const fs::path &sourceFile,
                            const fs::path &outputFile) {
  std::ostringstream out;
  writeSource(out, sourceFile);
  out << ",\n  \"sourceText\": ";
  if (report.sourceText)
    writeString(out, *report.sourceText);
  else
    out << "null";
  out << ",\n  \"accepted\": " << (report.accepted ? "true" : "false")
      << ",\n  \"diagnostics\": {\"lexical\":";
  writeDiagnostics(out, report.lexical);
  out << ",\"syntax\":";
  if (report.parser)
    writeDiagnostics(out, report.parser->syntaxDiagnostics());
  else
    out << "[]";
  out << ",\"semantic\":";
  writeDiagnostics(out, report.semantic);
  out << "},\n  \"tokens\": ";
  if (report.parser) {
    writeArray(out, report.parser->tokens(),
               [](auto &stream, const TokenData &token) {
                 stream << "{\"line\":" << token.row << ",\"col\":" << token.col
                        << ",\"symbol\":";
                 writeString(stream, token.symbol);
                 stream << ",\"text\":";
                 writeString(stream, token.value);
                 stream << '}';
               });
  } else {
    out << "[]";
  }
  out << ",\n  \"trace\": ";
  if (report.parser) {
    writeArray(out, report.parser->traceRows(),
               [](auto &stream, const TraceRow &row) {
                 stream << "{\"states\":";
                 writeString(stream, row.states);
                 stream << ",\"symbols\":";
                 writeString(stream, row.symbols);
                 stream << ",\"input\":";
                 writeString(stream, row.input);
                 stream << ",\"action\":";
                 writeString(stream, row.action);
                 stream << '}';
               });
  } else {
    out << "[]";
  }
  out << ",\n  \"automaton\": ";
  if (report.parser)
    report.parser->writeJSON(out);
  else
    out << "null";
  out << ",\n  \"scopes\": ";
  if (report.ast && report.symbols)
    writeScopes(out, *report.ast, *report.symbols);
  else
    out << "[]";
  out << ",\n  \"ast\": ";
  if (report.ast)
    writeNode(out, *report.ast);
  else
    out << "null";
  out << "\n}\n";
  publish(out.str(), sourceFile, outputFile, "report.json", "report-data.js",
          "chocopyReport");
}

}
