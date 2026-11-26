#include "ChocoPyAst.h"
#include <stdexcept>
#include <utility>

namespace chocopy {
namespace {

using ast::Location;
using ast::NodePtr;

template <typename... Children>
NodePtr node(std::string kind, Location location, std::string text, Children... children) {
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
    if (sequence.items.empty()) return node("Seq", {}, sequence.role);

    auto tail = std::move(sequence.items.back());
    for (std::size_t i = sequence.items.size() - 1; i > 0; --i) {
        auto& head = sequence.items[i - 1];
        const auto start = head->location;
        tail = node("Seq", start, sequence.role, std::move(head), std::move(tail));
    }
    return tail;
}

}

LR1::AstValue buildNode(const Grammar& grammar, const Production& rule, std::vector<LR1::AstValue> values) {
    const auto& lhs = grammar.symbolTable[rule.left].name;
    const auto rhs = [&](std::size_t i) -> const std::string& {
        return grammar.symbolTable[rule.right.at(i)].name;
    };
    const auto token = [&](std::size_t i) -> const TokenData& { return std::get<TokenData>(values.at(i)); };
    const auto take = [&](std::size_t i) {
        auto& value = values.at(i);
        if (auto* sequence = std::get_if<LR1::AstSequence>(&value)) {
            return buildSequence(std::move(*sequence));
        }
        return std::get<NodePtr>(std::move(value));
    };
    const auto at = [&](std::size_t i) -> Location {
        if (const auto* terminal = std::get_if<TokenData>(&values.at(i))) {
            return {terminal->row, terminal->col};
        }
        const auto& child = std::get<NodePtr>(values.at(i));
        return child ? child->location : Location{};
    };
    const auto sequence = [&](const char* role, std::size_t next) {
        auto items = !values.empty() && rhs(0) == lhs
            ? std::get<LR1::AstSequence>(std::move(values[0])) : LR1::AstSequence{role, {}};
        if (!values.empty()) items.items.push_back(take(next));
        return items;
    };
    const auto id = [&](std::size_t i) { return node("Id", at(i), token(i).value); };
    const auto size = values.size();

    if (lhs == "program") {
        auto declarations = take(0);
        auto statements = size == 2 ? take(1) : node("Seq", {}, "statements");
        Location start = declarations->location.line == 0 ? statements->location : declarations->location;
        if (start.line == 0) {
            start = {1, 1};
        }
        return node("Program", start, "", std::move(declarations), std::move(statements));
    }
    if (lhs == "decl_list" || lhs == "func_decl_list") return sequence("declarations", 1);
    if (lhs == "stmt_plus") return sequence("statements", size - 1);
    if (lhs == "class_item_list") return sequence("members", size - 1);
    if (lhs == "params") return sequence("parameters", size - 1);
    if (lhs == "expr_list") return sequence("expressions", size - 1);
    if (lhs == "assign_list") return sequence("targets", size - 2);

    if (lhs == "params_opt" && size == 0) return node("Seq", {}, "parameters");
    if (lhs == "expr_list_opt" && size == 0) return node("Seq", {}, "expressions");
    if ((lhs == "expr_opt" || lhs == "else_opt" || lhs == "ret_opt") && size == 0) return NodePtr{};
    if (lhs == "ret_opt") return take(1);
    if (lhs == "else_opt") return node("Else", at(0), "", take(2));
    if (lhs == "elif_list") {
        if (size == 0) return LR1::AstSequence{"elif", {}};
        auto branches = std::get<LR1::AstSequence>(std::move(values[0]));
        branches.items.push_back(node("Elif", at(1), "", take(2), take(4)));
        return branches;
    }

    if (lhs == "class_def") return node("ClassDef", at(0), token(1).value, id(3), take(8));
    if (lhs == "class_body" && size == 2) return node("Pass", at(0), "");
    if (lhs == "func_def") return node("FuncDef", at(0), token(1).value, take(3), take(5), take(9));
    if (lhs == "func_body") {
        auto declarations = take(0);
        auto statements = take(1);
        const auto start = declarations->location.line == 0 ? statements->location : declarations->location;
        return node("Body", start, "", std::move(declarations), std::move(statements));
    }
    if (lhs == "block") return take(2);
    if (lhs == "typed_var") return node("Param", at(0), token(0).value, take(2));
    if (lhs == "var_def") {
        auto variable = take(0);
        variable->kind = "VarDef";
        variable->children.push_back(take(2));
        return variable;
    }
    if (lhs == "type") {
        if (size == 3) return node("ListType", at(0), "", take(1));
        std::string name = token(0).value;
        if (rhs(0) == "IDSTRING") name = name.substr(1, name.size() - 2);
        return node("NamedType", at(0), std::move(name));
    }
    if (lhs == "global_decl") return node("GlobalDecl", at(0), token(1).value);
    if (lhs == "nonlocal_decl") return node("NonlocalDecl", at(0), token(1).value);

    if (lhs == "stmt") {
        if (rhs(0) == "if") return node("If", at(0), "", take(1), take(3), take(4), take(5));
        if (rhs(0) == "while") return node("While", at(0), "", take(1), take(3));
        if (rhs(0) == "for") return node("For", at(0), "", id(1), take(3), take(5));
        return take(0);
    }
    if (lhs == "simple_stmt") {
        if (rhs(0) == "pass") return node("Pass", at(0), "");
        if (rhs(0) == "return") return node("Return", at(0), "", take(1));
        auto first = take(0);
        const auto start = first->location;
        if (size == 2) return node("Assign", start, "", std::move(first), take(1));
        return node("ExprStmt", start, "", std::move(first));
    }
    if (lhs == "literal") {
        const auto& symbol = rhs(0);
        const auto location = at(0);
        const auto& lexeme = token(0).value;
        if (symbol == "INTEGER") return node("IntLit", location, lexeme);
        if (symbol == "STRING" || symbol == "IDSTRING") return node("StrLit", location, lexeme);
        if (symbol == "None") return node("NoneLit", location, lexeme);
        return node("BoolLit", location, lexeme);
    }

    if (lhs == "expr" && size == 5) {
        const auto start = at(0);
        return node("IfExpr", start, "", take(2), take(0), take(4));
    }
    if ((lhs == "or_expr" || lhs == "and_expr" || lhs == "arith" || lhs == "term") && size == 3) {
        return node("BinOp", at(1), token(1).value, take(0), take(2));
    }
    if ((lhs == "not_expr" || lhs == "unary") && size == 2) {
        return node("UnaryOp", at(0), token(0).value, take(1));
    }
    if (lhs == "cmp_op") return node("Operator", at(0), token(0).value);
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
        if (rhs(0) == "[") return node("ListExpr", at(0), "", take(1));
        if (rhs(0) == "(") return take(1);
        if (size == 4) {
            const auto start = at(0);
            auto callee = rhs(0) == "ID" ? id(0) : take(0);
            return node("Call", start, "", std::move(callee), take(2));
        }
    }
    if ((lhs == "primary" || lhs == "target") && rhs(0) == "ID") return id(0);

    if (size == 1 && !std::holds_alternative<TokenData>(values[0])) return std::move(values[0]);
    throw std::logic_error("Produccion sin accion para el AST: " + lhs);
}

void printAst(const ast::Node& node, std::ostream& output, int depth) {
    output << std::string(depth * 2, ' ') << node.kind;
    if (!node.text.empty()) output << ' ' << node.text;
    if (node.location.line > 0) output << " [" << node.location.line << ':' << node.location.col << ']';
    if (node.type) output << " : " << *node.type;
    output << '\n';

    for (const auto& child : node.children) {
        printAst(*child, output, depth + 1);
    }
}

}
