#include "AstJson.h"
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

namespace fs = std::filesystem;

std::size_t utf8Length(std::string_view text, std::size_t start) {
    const unsigned char first = text[start];
    if (first < 0xC2 || first > 0xF4) return 0;
    const std::size_t length = first < 0xE0 ? 2 : first < 0xF0 ? 3 : 4;
    if (text.size() - start < length) return 0;

    const unsigned char second = text[start + 1];
    if ((first == 0xE0 && second < 0xA0) || (first == 0xED && second > 0x9F) ||
        (first == 0xF0 && second < 0x90) || (first == 0xF4 && second > 0x8F)) return 0;
    for (std::size_t i = start + 1; i < start + length; ++i) {
        if ((static_cast<unsigned char>(text[i]) & 0xC0) != 0x80) return 0;
    }
    return length;
}

void writeString(std::ostream& out, std::string_view text) {
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

void writeNode(std::ostream& out, const ast::Node& node) {
    out << "{\"kind\":";
    writeString(out, node.kind);
    out << ",\"text\":";
    writeString(out, node.text);
    out << ",\"line\":" << node.location.line << ",\"col\":" << node.location.col << ",\"type\":";
    if (node.type) {
        writeString(out, *node.type);
    } else {
        out << "null";
    }
    out << ",\"children\":[";
    const char* separator = "";
    for (const auto& child : node.children) {
        out << separator;
        writeNode(out, *child);
        separator = ",";
    }
    out << "]}";
}

template <typename Range, typename Writer>
void writeArray(std::ostream& out, const Range& items, Writer write) {
    out << '[';
    const char* separator = "";
    for (const auto& item : items) {
        out << separator;
        write(out, item);
        separator = ",";
    }
    out << ']';
}

void writeDiagnostics(std::ostream& out, const std::vector<Diagnostic>& diagnostics) {
    writeArray(out, diagnostics, [](auto& stream, const Diagnostic& diagnostic) {
        stream << "{\"line\":" << diagnostic.location.line << ",\"col\":" << diagnostic.location.col << ",\"message\":";
        writeString(stream, diagnostic.message);
        stream << '}';
    });
}

const char* scopeKind(ScopeKind kind) {
    switch (kind) {
    case ScopeKind::Module: return "modulo";
    case ScopeKind::Class: return "clase";
    case ScopeKind::Function: return "funcion";
    }
    throw std::logic_error("Ambito desconocido.");
}

const char* symbolKind(SymbolKind kind) {
    switch (kind) {
    case SymbolKind::Variable: return "variable";
    case SymbolKind::Parameter: return "parametro";
    case SymbolKind::Function: return "funcion";
    case SymbolKind::Class: return "clase";
    case SymbolKind::Global: return "global";
    case SymbolKind::Nonlocal: return "nonlocal";
    }
    throw std::logic_error("Simbolo desconocido.");
}

void collectScopes(const ast::Node& node, const SymbolTable& symbols, std::vector<const ast::Node*>& owners) {
    if (symbols.scopeFor(node)) owners.push_back(&node);
    for (const auto& child : node.children) collectScopes(*child, symbols, owners);
}

void writeScopes(std::ostream& out, const ast::Node& ast, const SymbolTable& symbols) {
    std::vector<const ast::Node*> owners;
    collectScopes(ast, symbols, owners);
    std::unordered_map<const Scope*, std::size_t> ids;
    for (std::size_t i = 0; i < owners.size(); ++i) ids.emplace(symbols.scopeFor(*owners[i]), i);

    writeArray(out, owners, [&](auto& stream, const ast::Node* owner) {
        const auto* scope = symbols.scopeFor(*owner);
        stream << "{\"id\":" << ids.at(scope) << ",\"name\":";
        writeString(stream, owner->text);
        stream << ",\"kind\":";
        writeString(stream, scopeKind(scope->kind()));
        stream << ",\"line\":" << owner->location.line << ",\"col\":" << owner->location.col << ",\"parent\":";
        if (scope->parent()) stream << ids.at(scope->parent());
        else stream << "null";
        stream << ",\"entries\":";
        std::vector<std::string> names;
        for (const auto& entry : scope->entries()) names.push_back(entry.first);
        std::sort(names.begin(), names.end());
        writeArray(stream, names, [&](auto& entry, const std::string& name) {
            const auto& symbol = scope->entries().at(name);
            const auto* definition = symbol.definition();
            entry << "{\"name\":";
            writeString(entry, name);
            entry << ",\"kind\":";
            writeString(entry, symbolKind(symbol.kind));
            entry << ",\"line\":" << (symbol.declaration ? symbol.declaration->location.line : 0)
                  << ",\"col\":" << (symbol.declaration ? symbol.declaration->location.col : 0) << ",\"type\":";
            if (definition && definition->declaration && definition->declaration->type) {
                writeString(entry, *definition->declaration->type);
            } else {
                entry << "null";
            }
            entry << ",\"target\":";
            if (symbol.target) entry << ids.at(symbol.target->scope);
            else entry << "null";
            entry << '}';
        });
        stream << '}';
    });
}

class ReportStaging {
public:
    ReportStaging(const fs::path& parent, bool withCompanion) {
        if (withCompanion) {
            directory = parent / ".chocopy-ast-export";
            if (!fs::create_directory(directory)) {
                throw std::runtime_error("El directorio temporal de exportacion del AST ya existe.");
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

    fs::path file(const char* filename) const { return directory / filename; }

private:
    fs::path directory;
};

void checkDestination(const fs::path& source, const fs::path& destination) {
    const bool destinationExists = fs::exists(destination);
    const bool sameFile = destinationExists && fs::exists(source)
        ? fs::equivalent(source, destination)
        : fs::weakly_canonical(fs::absolute(source)) == fs::weakly_canonical(fs::absolute(destination));
    if (sameFile) {
        throw std::runtime_error("Un destino de la exportacion del AST coincide con el archivo fuente.");
    }
    if (destinationExists && !fs::is_regular_file(destination)) {
        throw std::runtime_error("Un destino de la exportacion del AST no es un archivo.");
    }
}

void stageReport(const fs::path& file, std::string_view data, const char* global = nullptr) {
    std::ofstream out(file, std::ios::binary);
    if (!out) {
        throw std::runtime_error("No se pudo abrir un archivo de exportacion del AST.");
    }
    if (global) out << "globalThis." << global << " = ";
    out << data;
    if (global) out << ";\n";
    out.close();
    if (!out) {
        throw std::runtime_error("No se pudo guardar un archivo de exportacion del AST.");
    }
}

void publish(const std::string& data, const fs::path& sourceFile, const fs::path& outputFile,
             const char* canonicalName, const char* companionName, const char* global) {
    const bool withCompanion = outputFile.filename() == canonicalName;
    const auto companion = outputFile.parent_path() / companionName;
    const ReportStaging staging(outputFile.parent_path(), withCompanion);
    checkDestination(sourceFile, outputFile);
    if (withCompanion) checkDestination(sourceFile, companion);

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
    if (hadCompanion) fs::copy_file(companion, previous);
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

void writeSource(std::ostream& out, const fs::path& sourceFile) {
    out << "{\n  \"source\": ";
    const auto filename = sourceFile.filename().u8string();
    writeString(out, std::string(filename.begin(), filename.end()));
}

} // namespace

void writeAstJson(const ast::Node& root, const fs::path& sourceFile, const fs::path& outputFile) {
    std::ostringstream out;
    writeSource(out, sourceFile);
    out << ",\n  \"ast\": ";
    writeNode(out, root);
    out << "\n}\n";
    publish(out.str(), sourceFile, outputFile, "ast.json", "ast-data.js", "chocopyAst");
}

void writeCompilationReport(const CompilationReport& report, const fs::path& sourceFile, const fs::path& outputFile) {
    std::ostringstream out;
    writeSource(out, sourceFile);
    out << ",\n  \"sourceText\": ";
    if (report.sourceText) writeString(out, *report.sourceText);
    else out << "null";
    out << ",\n  \"accepted\": " << (report.accepted ? "true" : "false") << ",\n  \"diagnostics\": {\"lexical\":";
    writeDiagnostics(out, report.lexical);
    out << ",\"syntax\":";
    if (report.parser) writeDiagnostics(out, report.parser->syntaxDiagnostics());
    else out << "[]";
    out << ",\"semantic\":";
    writeDiagnostics(out, report.semantic);
    out << "},\n  \"tokens\": ";
    if (report.parser) {
        writeArray(out, report.parser->tokens(), [](auto& stream, const TokenData& token) {
            stream << "{\"line\":" << token.row << ",\"col\":" << token.col << ",\"symbol\":";
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
        writeArray(out, report.parser->traceRows(), [](auto& stream, const TraceRow& row) {
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
    if (report.parser) report.parser->writeJSON(out);
    else out << "null";
    out << ",\n  \"scopes\": ";
    if (report.ast && report.symbols) writeScopes(out, *report.ast, *report.symbols);
    else out << "[]";
    out << ",\n  \"ast\": ";
    if (report.ast) writeNode(out, *report.ast);
    else out << "null";
    out << "\n}\n";
    publish(out.str(), sourceFile, outputFile, "report.json", "report-data.js", "chocopyReport");
}

} // namespace chocopy
