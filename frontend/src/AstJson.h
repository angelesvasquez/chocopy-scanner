#pragma once

#include "../../parser/src/Ast.h"
#include <filesystem>

class LR1;

namespace chocopy {

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
