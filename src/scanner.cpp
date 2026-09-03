#include "scanner.h"

#include <iostream>
#include <map>
#include <stack>
#include <fstream>
#include <sstream>
#include < cctype >

using namespace std;

map<string, string> KEYWORDS = {
    {"False", "FALSE"},
    {"None", "NONE"},
    {"True", "TRUE"},
    {"and", "AND"},
    {"as", "AS"},
    {"assert", "ASSERT"},
    {"async", "ASYNC"},
    {"await", "AWAIT"},
    {"break", "BREAK"},
    {"class", "CLASS"},
    {"continue", "CONTINUE"},
    {"def", "DEF"},
    {"del", "DEL"},
    {"elif", "ELIF"},
    {"else", "ELSE"},
    {"except", "EXCEPT"},
    {"finally", "FINALLY"},
    {"for", "FOR"},
    {"from", "FROM"},
    {"global", "GLOBAL"},
    {"if", "IF"},
    {"import", "IMPORT"},
    {"in", "IN"},
    {"is", "IS"},
    {"lambda", "LAMBDA"},
    {"nonlocal", "NONLOCAL"},
    {"not", "NOT"},
    {"or", "OR"},
    {"pass", "PASS"},
    {"raise", "RAISE"},
    {"return", "RETURN"},
    {"try", "TRY"},
    {"while", "WHILE"},
    {"with", "WITH"},
    {"yield", "YIELD"},
    {"\n", "NEWLINE"},
    {"#", "COMMENT"},
    {"+", "PLUS"},
    {"-", "MINUS"},
    {"*", "STAR"},
    {"//", "DOUBLESLASH"},
    {"/", "SINGLESLASH_INVALID"},
    {"%", "PERCENT"},
    {"<", "LESS"},
    {">", "GREATER"},
    {"<=", "LESSEQUAL"},
    {">=", "GREATEREQUAL"},
    {"==", "EQUALEQUAL"},
    {"!=", "NOTEQUAL"},
    {"!", "NOT_INVALID"},
    {"=", "EQUAL"},
    {"(", "LPAREN"},
    {")", "RPAREN"},
    {"[", "LBRACKET"},
    {"]", "RBRACKET"},
    {",", "COMMA"},
    {":", "COLON"},
    {".", "DOT"},
    {"->", "ARROW"},
    {"INDENT", "INDENT"},
    {"DEDENT", "DEDENT"},
    {string(1,'"'), "QUOTES"}
};

Scanner::Scanner() {
    ifstream file("input.txt");
    if (!file.is_open()) {
        cout << "No se pudo abrir el archivo." << endl;
        return;
    }
    stringstream buffer_stream;
    buffer_stream << file.rdbuf();
    buffer = buffer_stream.str();
    file.close();

    p_buffer = &buffer[0];

    indent_stack.push(0);
    indent_counter = 0;

    row = 0;
    col = 0;

    scan();
}

char Scanner::get_char() {
    col++;
    p_buffer++;
    return *p_buffer;
}

char Scanner::peek_char() {
    return *(p_buffer + 1);
}

void Scanner::check_token(string& token_buffer) {
    if (token_buffer.empty()) return;
    TokenData* td = nullptr;
    if (KEYWORDS.find(token_buffer) != KEYWORDS.end()) {
        td = new TokenData(KEYWORDS[token_buffer], token_buffer == "\n" ? "\\n" : token_buffer, row, col);
    }
    else if (token_buffer[0] == '"') {
        td = new TokenData("STRING", token_buffer, row, col);
    }
    else if (token_buffer[0] > 47 && token_buffer[0] < 58) {
        int n = stoll(token_buffer);
        if (n < INT32_MAX && n > INT32_MIN) {
            td = new TokenData("NUMBER", token_buffer, row, col);
        }
        else {
            td = new TokenData("NUMBER_ERR", "0", row, col);
        }
    }
    else if (token_buffer[0] == '#') {
        std::cout << "COMMENT: " << token_buffer << " (" << row << "," << col << ")" << endl;
        token_buffer.clear();
        return;
    }
    else {
        td = new TokenData("ID", token_buffer, row, col);
    }
    std::cout << td->token << " [" << td->value << "] (" << td->row << "," << td->col << ")" << endl;
    token_buffer.clear();
}

void Scanner::scan() {
    string token_buffer = "";
    char value_char = (*p_buffer);

    bool on_comment = false;
    bool on_string = false;
    bool on_check_indent = false;

    while (true) {

        if (KEYWORDS.find(string(1, value_char)) != KEYWORDS.end() && !on_comment && !on_string) {
            if (!token_buffer.empty()) {
                check_token(token_buffer);
            }

            if (value_char == '\n') {
                token_buffer += value_char;
                value_char = get_char();

                while (value_char == '\n')
                {
                    value_char = get_char();
                    row++;
                    col = 0;
                }
                check_token(token_buffer);
                row++;
                col = 0;

                on_check_indent = true;
                continue;
            }

            if (value_char == '#') {
                on_comment = true;
            }
            else if (value_char == '"') {
                on_string = true;
            }

            token_buffer += value_char;

            char last_value_char = value_char;
            value_char = get_char();

            if (on_string || on_comment) {
                continue;
            }

            if (last_value_char == '/') {
                if (value_char == '/') {
                    token_buffer += value_char;
                    value_char = get_char();
                }
                else {
                    cout << "OPERADOR NO VALIDO: " << token_buffer << endl;
                    token_buffer.clear();
                }
            }
            else if (last_value_char == '!') {
                if (value_char == '=') {
                    token_buffer += value_char;
                    value_char = get_char();
                }
                else {
                    cout << "OPERADOR NO VALIDO: " << token_buffer << endl;
                    token_buffer.clear();
                }
            }
            else if (last_value_char == '=') {
                if (value_char == '=') {
                    token_buffer += value_char;
                    value_char = get_char();
                }
            }
            else if (last_value_char == '<') {
                if (value_char == '=') {
                    token_buffer += value_char;
                    value_char = get_char();
                }
            }
            else if (last_value_char == '>') {
                if (value_char == '=') {
                    token_buffer += value_char;
                    value_char = get_char();
                }
            }
            else if (last_value_char == '-') {
                if (value_char == '>') {
                    token_buffer += value_char;
                    value_char = get_char();
                }
            }

            check_token(token_buffer);
        }
        else if (on_comment) {
            while (value_char != '\n' && value_char != '\0') {
                token_buffer += value_char;
                value_char = get_char();
            }
            check_token(token_buffer);
            on_comment = false;
        }
        else if (on_string) {
            while (value_char != '"') {
                if (value_char == '\\') {
                    value_char = get_char();
                    if (value_char == '"') {
                        token_buffer += value_char;
                        value_char = get_char();
                        continue;
                    }
                }
                token_buffer += value_char;
                value_char = get_char();
            }
            token_buffer += value_char;
            check_token(token_buffer);
            on_string = false;
            value_char = get_char();
        }
        else if ((value_char > 47 && value_char < 58 && token_buffer.empty())) {
            while (value_char > 47 && value_char < 58) {
                token_buffer += value_char;
                value_char = get_char();
            }
        }
        else if (value_char == ' ') {
            if (!token_buffer.empty()) {
                check_token(token_buffer);
            }
            value_char = get_char();

        }
        else if (value_char == '\t' || on_check_indent) {
            on_check_indent = false;

            if (value_char == '\t') {
                value_char = get_char();
                indent_counter++;
            }

            while (value_char == '\t') {
                value_char = get_char();
                indent_counter++;
            }

            if (indent_stack.top() < indent_counter) {
                indent_stack.push(indent_counter);
                token_buffer = "INDENT";
                check_token(token_buffer);
            }
            else if (indent_stack.top() > indent_counter) {
                while (indent_stack.top() > indent_counter) {
                    indent_stack.pop();
                    token_buffer = "DEDENT";
                    check_token(token_buffer);
                }
                if (indent_stack.top() != indent_counter) {
                    indent_counter = 0;
                    //error
                    continue;
                }
            }
            indent_counter = 0;
        }
        else if (value_char == '\0') {
            check_token(token_buffer);
            break;
        }
        else if (isalpha(static_cast<unsigned char>(value_char)) || value_char == '_') {
            token_buffer.push_back(value_char);
            value_char = get_char();
        }
        else {
            cout << "CARACTER NO VALIDO: " << value_char << endl;
            value_char = get_char();
        }

    }

}