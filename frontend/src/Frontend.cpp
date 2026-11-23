#include "Frontend.h"
#include "ChocoPyAst.h"
#include "ChocoPyGrammar.h"
#include "AstJson.h"
#include "../../semantics/src/TypeChecker.h"
#include <algorithm>
#include <iterator>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <tuple>

namespace {

void printGroup(const char* heading, const std::vector<chocopy::Diagnostic>& diagnostics) {
    if (diagnostics.empty()) return;
    std::cout << heading << '\n';
    for (const auto& diagnostic : diagnostics) {
        if (diagnostic.location.line > 0) {
            std::cout << "  Linea " << diagnostic.location.line << ", col " << diagnostic.location.col << ": ";
        } else {
            std::cout << "  ";
        }
        std::cout << diagnostic.message << '\n';
    }
}

void printDiagnostics(const std::vector<chocopy::Diagnostic>& lexical,
                      const std::vector<chocopy::Diagnostic>& syntax,
                      const std::vector<chocopy::Diagnostic>& semantic) {
    if (!lexical.empty() || !syntax.empty() || !semantic.empty()) {
        std::cout << "\n--- DIAGNOSTICOS ---\n";
        printGroup("Errores lexicos", lexical);
        printGroup("Errores sintacticos", syntax);
        printGroup("Errores semanticos", semantic);
    }

    std::cout << "Resumen: errores lexicos: " << lexical.size()
              << ", errores sintacticos: " << syntax.size()
              << ", errores semanticos: " << semantic.size() << '\n';
}

} // namespace

Grammar Frontend::loadGrammar() {
    Grammar grammar;
    std::istringstream input(chocopyGrammar);
    grammar.load(input);
    if (grammar.productions.empty()) {
        throw std::runtime_error("La gramatica ChocoPy esta vacia.");
    }
    return grammar;
}

Frontend::Frontend(const std::filesystem::path& sourceFile)
    : sourceFile(sourceFile), scanner(sourceFile, false), parser(loadGrammar()) {}

bool Frontend::run(bool trace, const std::filesystem::path& astFile, const std::filesystem::path& reportFile) {
    ast::NodePtr ast;
    const LR1* parsed = nullptr;
    if (scanner.errors() == 0) {
        if (!parser.build(false)) {
            throw std::runtime_error("No se pudieron construir las tablas LR(1) de ChocoPy.");
        }

        parser.setPanicSet({"stmt", "func_decl", "class_item", "decl_item"});
        ast = parser.parse(&scanner, chocopy::buildNode, trace || !reportFile.empty());
        // Syntax recovery can stop before it has requested every token.
        parser.finishScanning();
        parsed = &parser;
        if (trace) parser.printTrace();
    }

    std::vector<chocopy::Diagnostic> semantic;
    std::optional<chocopy::SymbolTable> symbols;
    if (ast) {
        symbols.emplace(*ast);
        semantic = symbols->diagnostics();
        auto types = chocopy::checkTypes(*ast, *symbols);
        semantic.insert(semantic.end(), std::make_move_iterator(types.begin()), std::make_move_iterator(types.end()));
        std::stable_sort(semantic.begin(), semantic.end(), [](const auto& a, const auto& b) {
            return std::tie(a.location.line, a.location.col) < std::tie(b.location.line, b.location.col);
        });
    }

    printDiagnostics(scanner.diagnostics(), parser.syntaxDiagnostics(), semantic);
    const bool accepted = ast && scanner.errors() == 0 && parser.syntaxDiagnostics().empty() && semantic.empty();
    std::cout << "Resultado: " << (accepted ? "ACEPTADO" : "RECHAZADO") << '\n';
    if (!reportFile.empty()) {
        const chocopy::CompilationReport report{
            accepted, parsed ? &scanner.sourceText() : nullptr,
            ast.get(), symbols ? &*symbols : nullptr, parsed, scanner.diagnostics(), semantic
        };
        chocopy::writeCompilationReport(report, sourceFile, reportFile);
        const auto path = reportFile.u8string();
        std::cout << "Informe guardado en " << std::string(path.begin(), path.end()) << '\n';
        return accepted;
    }
    if (!accepted) return false;

    if (astFile.empty()) {
        chocopy::printAst(*ast, std::cout);
    } else {
        chocopy::writeAstJson(*ast, sourceFile, astFile);
        const auto path = astFile.u8string();
        std::cout << "AST guardado en " << std::string(path.begin(), path.end()) << '\n';
    }
    return true;
}
