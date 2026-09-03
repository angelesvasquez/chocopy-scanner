#ifndef SCANNER_H
#define SCANNER_H

#include <iostream>
#include <string>
#include <stack>

using namespace std;

struct TokenData {
    int row, col;
    string token;
    string value;
    TokenData(string _token, string _value, int _row, int _col) : token(_token), value(_value), row(_row), col(_col) {}
};

class Scanner {
    string buffer;
    char* p_buffer;
    int row;
    int col;

    stack<int> indent_stack;
    int indent_counter;
public:
    Scanner();
    char get_char();
    char peek_char();
    void check_token(string& token_buffer);
    void scan();
};

#endif