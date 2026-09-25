// LR1

#include "Grammar.cpp"
#include <iostream>
#include <vector>
#include <unordered_set>
#include <unordered_map>

using namespace std;

struct Item {
	int production;
	int dot; // posicion del punto
	Symbol lookahead;
	//unordered_set<Symbol>
};

//struct Transition {
//	int orig;
//	int dest;
//	int symbol;
//};

struct FirstSet {
	unordered_set<Symbol> symbols;
	bool epsilon = 0;
};

using FirstTable = unordered_map<Symbol, FirstSet>;

class LR1 {
	Grammar gr;
	map<pair<int, int>, int> transitions;
	vector<vector<Item>> states;
	FirstTable firsts;

	void first() {
		for (auto sy : gr.symbolTable) {
			Symbol s = gr.getSymbol(sy.name);
			if (sy.terminal) {
				firsts[s].symbols.insert(s);
			}
		}
		bool changed = true;
		while (changed) {
			changed = false;
			for (auto prod : gr.productions) {
				Symbol A = prod.left;
				vector<Symbol> b = prod.right;
				int k = (int)b.size();

				if (k == 0) {
					if (!firsts[A].epsilon) {
						firsts[A].epsilon = 1;
						changed = 1;
					}
					continue;
				}

				unordered_set<Symbol> rhs = firsts[b[0]].symbols;
				int i = 1;
				for (; i <= k - 1 && firsts[b[i - 1]].epsilon; i++) {
					for (Symbol sym : firsts[b[i]].symbols) rhs.insert(sym);
				}
				bool rhsEpsilon = (i == k && firsts[b[k - 1]].epsilon);

				for (Symbol sym : rhs) {
					if (firsts[A].symbols.insert(sym).second)
						changed = 1;
				}
				if (rhsEpsilon && !firsts[A].epsilon) {
					firsts[A].epsilon = 1;
					changed = 1;
				}
			}
		}
	}
	FirstSet firstCadena(vector<Symbol>& beta) {
		FirstSet result;
		if (beta.empty()) { result.epsilon = true; return result; }
		result.symbols = firsts[beta[0]].symbols;
		int i = 1;
		while (i <= beta.size() - 1 && firsts[beta[i - 1]].epsilon) {
			for (Symbol sym : firsts[beta[i]].symbols) result.symbols.insert(sym);
			i++;
		}
		if (i == beta.size() && firsts[beta[i - 1]].epsilon) result.epsilon = true;
		return result;
	}

public:
	LR1(Grammar g) : gr(g) {
		//gr.getSymbol("eof", 1);
		first();
	}

	void printFirsts() {
		cout << "\n--- FIRSTS ---\n";
		for (size_t i = 0; i < gr.symbolTable.size(); ++i) {
			cout << "First(" << gr.symbolTable[i].name << ") = { ";

			for (Symbol sym : firsts[i].symbols) {
				cout << gr.symbolTable[sym].name << " ";
			}

			if (firsts[i].epsilon) {
				cout << "epsilon";
			}

			cout << " }\n";
		}
	}

};


int main() {
	Grammar g;
	g.loadFile("input.txt");
	g.print();
	LR1 lr(g);
	lr.printFirsts();
	return 0;
}