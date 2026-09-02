#include <iostream>
#include <vector>
#include <random>
#include <fstream>
#include <sstream>
#include <numeric>
#include <map>
#include <algorithm>
#include <cctype>

using namespace std;

const map<string, string> KEYWORDS = {
    {"def","def"}
};

struct TokenData {
    int row, col;
    string token;
    string value;
    TokenData(string _token, string _value, int _row, int _col): token(_token), value(_value), row(_row), col(_col) {}
};

char getchar(char* &p_buffer) {
    p_buffer++;
    return *p_buffer;
}

char peekchar(char* p_buffer) {
    return *(p_buffer+1);
}

string upper(string word) {
    string to_return = word;
    for (char &c : to_return) {
        c = toupper(c);
    }
    return to_return;
}

int main()
{
    string buffer = "hola def adios";
    char* p_buffer = &buffer[0];
    string token_buffer = "";

    char temp_char = (*p_buffer);
    int row = 0;
    int col = 0;

    while (true) {
        if (temp_char == ' ') {
            if (KEYWORDS.find(token_buffer) != KEYWORDS.end()) {
                TokenData* td = new TokenData(upper(token_buffer), token_buffer, row, col);
                std::cout << td->token << " " << td->value << " (" << td->row << "," << td->col << ")" << endl;
            }
            else {
                TokenData* td = new TokenData("ID", token_buffer, row, col);
                std::cout << td->token << " " << td->value << " (" << td->row << "," << td->col << ")" << endl;
            }
            token_buffer.clear();
        }
        else if (temp_char == '\n') {
            col = 0;
            row++;
        }
        else {
            token_buffer.push_back(temp_char);
        }
        col++;
        temp_char = getchar(p_buffer);
        if (temp_char == '\0') {
            if (KEYWORDS.find(token_buffer) != KEYWORDS.end()) {
                TokenData* td = new TokenData(upper(token_buffer), token_buffer, row, col);
                std::cout << td->token << " " << td->value << " (" << td->row << "," << td->col << ")" << endl;
            }
            else {
                TokenData* td = new TokenData("ID", token_buffer, row, col);
                std::cout << td->token << " " << td->value << " (" << td->row << "," << td->col << ")" << endl;
            }
            break;
        }
    }
    return 0;
}
