// LR1 leer gramatica

#include <iostream>
#include <map>
#include <stack>
#include <fstream>
#include <sstream>
#include <cctype>
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
	Symbol getSymbol(string name, bool isTerminalD = true) {
		if (symbolMap.find(name) == symbolMap.end()) {
			Symbol newId = symbolTable.size();
			symbolMap[name] = newId;
			symbolTable.push_back({ name, isTerminalD });
		}
		else {
			if (!isTerminalD) {
				symbolTable[symbolMap[name]].terminal = 0;
			}
		}
		return symbolMap[name];
	}
	bool loadFile(string filename) {
		ifstream file(filename);
		if (!file.is_open()) {
			std::cerr << "No se pudo abrir el archivo: " << filename << "\n";
			return 0;
		}
		string line;
		while (getline(file, line)) {
			if (line.empty()) continue;
			stringstream ss(line);
			string leftStr, arrow;
			ss >> leftStr >> arrow;
			if (arrow != "->") continue;

			Symbol leftId = getSymbol(leftStr, 0);
			Production prod;
			prod.left = leftId;

			string rightStr;
			while (ss >> rightStr) {
				Symbol id = getSymbol(rightStr, 1);
				prod.right.push_back(id);
			}
			productions.push_back(prod);
			// agregar el EOF
		}
		file.close();
		return 1;
	}

	void print() {
		// for (int i = 0; i < symbolTable.size(); i++) {
		// 	cout << i << " = " << symbolTable[i].name << "\n";
		// }

		cout << "\n--- PRODUCCIONES ---\n";
		for (const auto& prod : productions) {
			cout << symbolTable[prod.left].name << " -> ";

			for (Symbol sym : prod.right) {
				cout << symbolTable[sym].name << " ";
			}
			cout << "\n";
		}
	}
};
