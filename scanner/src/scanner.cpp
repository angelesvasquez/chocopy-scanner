#include "scanner.h"

#include <cctype>
#include <fstream>
#include <map>
#include <sstream>
#include <vector>

using namespace std;

static const size_t MAX_STRING_LEN = 255;
static const long long MAX_INT32 = 2147483647LL;

static const map<string, string> KEYWORDS = {
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
    {"yield", "YIELD"}};

static string pad(const string &s, size_t w)
{
    return s.size() >= w ? s : s + string(w - s.size(), ' ');
}

static string describe_char(char ch)
{
    unsigned char uc = static_cast<unsigned char>(ch);
    if (isprint(uc))
        return string("'") + ch + "'";
    ostringstream o;
    o << "un caracter de codigo " << static_cast<int>(uc);
    return o.str();
}

Scanner::Scanner(const string &filename, bool verbose_) : pos(0), row(1), col(1), at_line_start(true), line_has_tokens(false), paren_depth(0), started(false), finished(false), verbose(verbose_), error_count(0)
{
    indent_stack.push(0);

    ifstream file(filename.c_str(), ios::binary);
    if (!file.is_open())
    {
        cout << "ERROR SCAN - No se pudo abrir el archivo '" << filename << "'" << endl;
        error_count++;
        return;
    }
    stringstream buffer_stream;
    buffer_stream << file.rdbuf();
    buffer = buffer_stream.str();
}

char Scanner::get_char()
{
    if (pos >= buffer.size())
        return '\0';
    char ch = buffer[pos++];
    if (ch == '\n')
    {
        row++;
        col = 1;
    }
    else
    {
        col++;
    }
    return ch;
}

char Scanner::peek_char()
{
    return pos >= buffer.size() ? '\0' : buffer[pos];
}

TokenData::TokenData(string _token, string _value, int _row, int _col) : token(_token), value(_value), row(_row), col(_col)
{
    symbol = grammarSymbol();
}

string TokenData::grammarSymbol() const
{
    if (token == "EOF")
    {
        return "eof";
    }
    if (token == "ID" || token == "STRING" || token == "NEWLINE" || token == "INDENT" || token == "DEDENT" || token == "INTEGER")
    {
        return token;
    }
    return value;
}

void Scanner::push_token(const string &token, const string &value, int r, int c)
{
    pending.push(TokenData(token, value, r, c));
    if (token != "INDENT" && token != "DEDENT" && token != "NEWLINE" && token != "EOF")
    {
        line_has_tokens = true;
    }
}

void Scanner::error(int r, int c, const string &msg)
{
    error_count++;
    cout << "ERROR SCAN - (" << r << ":" << c << ") " << msg;
    cout << endl;
}

void Scanner::skip_comment()
{
    while (!pos >= buffer.size() && peek_char() != '\n')
        get_char();
}

void Scanner::handle_indentation()
{
    int n = 0;
    bool has_tab = false;
    int tab_row = 0, tab_col = 0;

    while (!pos >= buffer.size())
    {
        char c = peek_char();
        if (c == ' ')
        {
            n++;
            get_char();
        }
        else if (c == '\t')
        {
            if (!has_tab)
            {
                has_tab = true;
                tab_row = row;
                tab_col = col;
            }
            get_char();
        }
        else if (c == '\r')
        {
            get_char();
        }
        else
        {
            break;
        }
    }

    if (pos >= buffer.size())
    {
        at_line_start = false;
        return;
    }

    char c = peek_char();
    if (c == '\n')
    {
        get_char();
        return;
    }
    if (c == '#')
    {
        skip_comment();
        return;
    }

    at_line_start = false;

    if (has_tab)
    {
        error(tab_row, tab_col, "tabulacion no valida");
    }

    int top = indent_stack.top();
    if (n > top)
    {
        indent_stack.push(n);
        push_token("INDENT", "", row, col);
    }
    else if (n < top)
    {
        while (indent_stack.top() > n)
        {
            indent_stack.pop();
            push_token("DEDENT", "", row, col);
        }
        if (indent_stack.top() != n)
        {
            error(row, col, "indentacion inconsistente");
        }
    }
}

void Scanner::scan_word()
{
    int r = row, c = col;
    string w;
    while (isalnum(static_cast<unsigned char>(peek_char())) || peek_char() == '_')
    {
        w += get_char();
    }
    map<string, string>::const_iterator it = KEYWORDS.find(w);
    if (it != KEYWORDS.end())
    {
        push_token(it->second, w, r, c);
    }
    else
    {
        push_token("ID", w, r, c);
    }
}

void Scanner::scan_number()
{
    int r = row, c = col;
    string s;
    while (isdigit(static_cast<unsigned char>(peek_char())))
    {
        s += get_char();
    }

    char nx = peek_char();
    if (isalpha(static_cast<unsigned char>(nx)) || nx == '_')
    {
        while (isalnum(static_cast<unsigned char>(peek_char())) || peek_char() == '_')
        {
            s += get_char();
        }
        error(r, c, "literal numerico mal formado '" + s + "'");
        return;
    }

    if (s.size() > 1 && s[0] == '0')
    {
        error(r, c, "ceros a la izquierda '" + s + "'");
        return;
    }

    if (s.size() > 10 || stoll(s) > MAX_INT32)
    {
        error(r, c, "entero fuera de rango '" + s + "'");
        return;
    }

    push_token("INTEGER", s, r, c);
}

void Scanner::scan_string()
{
    int r = row, c = col;
    string s;
    s += get_char();

    bool closed = false;
    bool bad_escape = false;

    while (!pos >= buffer.size() && peek_char() != '\n')
    {
        char ch = peek_char();
        if (ch == '"')
        {
            s += get_char();
            closed = true;
            break;
        }
        if (ch == '\\')
        {
            int er = row, ec = col;
            s += get_char();
            if (pos >= buffer.size() || peek_char() == '\n')
                break;
            char e = peek_char();
            if (e == 'n' || e == 't' || e == '"' || e == '\\')
            {
                s += get_char();
            }
            else
            {
                error(er, ec, "escape invalido " + describe_char(e));
                bad_escape = true;
                s += get_char();
            }
            continue;
        }
        s += get_char();
    }

    if (!closed)
    {
        error(r, c, "cadena sin cerrar");
        return;
    }
    if (bad_escape)
        return;

    if (s.size() - 2 > MAX_STRING_LEN)
    {
        error(r, c, "cadena demasiado larga");
        return;
    }

    push_token("STRING", s, r, c);
}

void Scanner::scan_operator()
{
    int r = row, c = col;
    char ch = get_char();

    switch (ch)
    {
    case '+':
        push_token("PLUS", "+", r, c);
        break;
    case '*':
        push_token("STAR", "*", r, c);
        break;
    case '%':
        push_token("PERCENT", "%", r, c);
        break;
    case ',':
        push_token("COMMA", ",", r, c);
        break;
    case ':':
        push_token("COLON", ":", r, c);
        break;
    case '.':
        push_token("DOT", ".", r, c);
        break;
    case '(':
        paren_depth++;
        push_token("LPAREN", "(", r, c);
        break;
    case ')':
        if (paren_depth > 0)
            paren_depth--;
        push_token("RPAREN", ")", r, c);
        break;
    case '[':
        paren_depth++;
        push_token("LBRACKET", "[", r, c);
        break;
    case ']':
        if (paren_depth > 0)
            paren_depth--;
        push_token("RBRACKET", "]", r, c);
        break;
    case '-':
        if (peek_char() == '>')
        {
            get_char();
            push_token("ARROW", "->", r, c);
        }
        else
        {
            push_token("MINUS", "-", r, c);
        }
        break;
    case '/':
        if (peek_char() == '/')
        {
            get_char();
            push_token("DOUBLESLASH", "//", r, c);
        }
        else
        {
            error(r, c, "Tipo operador incorrecto");
        }
        break;
    case '!':
        if (peek_char() == '=')
        {
            get_char();
            push_token("NOTEQUAL", "!=", r, c);
        }
        else
        {
            error(r, c, "Tipo operador incorrecto");
        }
        break;
    case '<':
        if (peek_char() == '=')
        {
            get_char();
            push_token("LESSEQUAL", "<=", r, c);
        }
        else
        {
            push_token("LESS", "<", r, c);
        }
        break;
    case '>':
        if (peek_char() == '=')
        {
            get_char();
            push_token("GREATEREQUAL", ">=", r, c);
        }
        else
        {
            push_token("GREATER", ">", r, c);
        }
        break;
    case '=':
        if (peek_char() == '=')
        {
            get_char();
            push_token("EQUALEQUAL", "==", r, c);
        }
        else
        {
            push_token("EQUAL", "=", r, c);
        }
        break;
    default:
        if (static_cast<unsigned char>(ch) >= 0x80)
        {
            while ((static_cast<unsigned char>(peek_char()) & 0xC0) == 0x80)
                get_char();
            error(r, c, "caracter no valido");
        }
        else
        {
            error(r, c, "caracter no valido " + describe_char(ch));
        }
        break;
    }
}

void Scanner::finish()
{
    if (paren_depth > 0)
    {
        error(row, col, "delimitador sin cerrar");
    }
    if (line_has_tokens)
    {
        push_token("NEWLINE", "", row, col);
        line_has_tokens = false;
    }
    while (indent_stack.top() > 0)
    {
        indent_stack.pop();
        push_token("DEDENT", "", row, col);
    }
    push_token("EOF", "", row, col);
    finished = true;
}

void Scanner::scan_step()
{
    if (at_line_start)
    {
        handle_indentation();
        return;
    }
    if (pos >= buffer.size())
    {
        finish();
        return;
    }

    char c = peek_char();
    if (c == ' ' || c == '\t' || c == '\r')
    {
        get_char();
        return;
    }
    if (c == '\n')
    {
        int r = row, cl = col;
        get_char();
        if (paren_depth > 0)
            return;
        if (line_has_tokens)
        {
            push_token("NEWLINE", "", r, cl);
            line_has_tokens = false;
        }
        at_line_start = true;
        return;
    }
    if (c == '#')
    {
        skip_comment();
        return;
    }
    if (c == '"')
    {
        scan_string();
        return;
    }

    unsigned char uc = static_cast<unsigned char>(c);
    if (isdigit(uc))
    {
        scan_number();
        return;
    }
    if (isalpha(uc) || c == '_')
    {
        scan_word();
        return;
    }
    scan_operator();
}

TokenData Scanner::gettoken()
{
    if (!started)
    {
        started = true;
        if (verbose)
            cout << "INFO  SCAN - Start scanning..." << endl;
    }

    while (pending.empty() && !finished)
    {
        scan_step();
    }

    if (pending.empty())
    {
        return TokenData("EOF", "", row, col);
    }

    TokenData t = pending.front();
    pending.pop();

    if (verbose)
    {
        cout << "DEBUG SCAN - " << pad(t.token, t.token.size() >= 8 ? t.token.size() + 1 : 8) << "[ " << pad(t.value, 6) << " ] ("
             << t.row << ":" << t.col << ")" << endl;
    }
    if (verbose && t.token == "EOF")
    {
        cout << "INFO  SCAN - Completed with " << error_count << " errors" << endl;
    }
    return t;
}