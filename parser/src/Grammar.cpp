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

	void augment() {
		Symbol originalStart = productions[0].left;
		string name = symbolTable[originalStart].name;

		Symbol id = getSymbol(name + "'", false);

		productions.insert(productions.begin(), { id, {originalStart} });

		getSymbol("eof", true);
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
				if (rightStr == "''") continue;
				Symbol id = getSymbol(rightStr, 1);
				prod.right.push_back(id);
			}
			productions.push_back(prod);
		}
		file.close();
		return 1;
	}

	void print() {
		// for (int i = 0; i < symbolTable.size(); i++) {
		// 	cout << i << " = " << symbolTable[i].name << "\n";
		// }

		cout << "\n--- PRODUCCIONES ---\n";
		for (int i = 0; i < productions.size(); i++) {
			cout << "(" << i << ") " << symbolTable[productions[i].left].name << " -> ";
			if (productions[i].right.empty()) cout << "''";
			for (Symbol sym : productions[i].right) {
				cout << symbolTable[sym].name << " ";
			}
			cout << "\n";
		}
	}
};
