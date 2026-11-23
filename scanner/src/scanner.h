#ifndef SCANNER_H
#define SCANNER_H

#include <iostream>
#include <string>
#include <stack>
#include <queue>
#include <cstddef>

using namespace std;

struct TokenData {
    string token;
    string value;
    string symbol;
    int row, col;
    TokenData(string _token, string _value, int _row, int _col);
    string grammarSymbol() const;
};

class Scanner {
    string buffer;
    size_t pos;
    int row;
    int col;

    stack<int> indent_stack;
    queue<TokenData> pending;

    bool at_line_start;
    bool line_has_tokens;
    int paren_depth;
    bool started;
    bool finished;
    bool verbose;
    int error_count;

    bool at_end() const;
    void push_token(const string& token, const string& value, int r, int c);
    void error(int r, int c, const string& msg);
    void skip_comment();
    void handle_indentation();
    void scan_word();
    void scan_number();
    void scan_string();
    void scan_operator();
    void finish();
    void scan_step();
public:
    Scanner(const string& filename = "input.txt", bool verbose = true);
    char get_char();
    char peek_char();
    TokenData gettoken();
    int errors() const { return error_count; }
};

#endif