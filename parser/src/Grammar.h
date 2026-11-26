#ifndef GRAMMAR_H
#define GRAMMAR_H

#include <iosfwd>
#include <map>
#include <string>
#include <vector>

using namespace std;

using Symbol = int;

struct SymbolInfo {
  string name;
  bool terminal;
};

struct Production {
  Symbol left;
  vector<Symbol> right;
};

class Grammar {
  map<string, Symbol> symbolMap;

public:
  vector<SymbolInfo> symbolTable;
  vector<Production> productions;

  Symbol augmentedStart = -1;

  Symbol getSymbol(string name, bool isTerminalD = true);
  Symbol findSymbol(string name);
  void augment();
  void load(std::istream &input);
  bool loadFile(string filename);
  void print();
};

#endif
