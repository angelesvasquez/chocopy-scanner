#include "Grammar.cpp"
#include <iostream>
#include <vector>
#include <unordered_set>
#include <set>
#include <algorithm>
#include <map>
#include <unordered_map>

using namespace std;

struct Item {
	int production;
	int dot;
	Symbol lookahead;

	bool operator==(const Item& i) const {
		return (production == i.production &&
			dot == i.dot &&
			lookahead == i.lookahead);
	}

	bool operator<(const Item& i) const {
		if (production != i.production) return production < i.production;
		if (dot != i.dot) return dot < i.dot;
		return lookahead < i.lookahead;
	}
};

struct FirstSet {
	unordered_set<Symbol> symbols;
	bool epsilon = 0;
};

using FirstTable = unordered_map<Symbol, FirstSet>;
using State = set<Item>;

class LR1 {
	Grammar gr;
	map<pair<int, int>, int> transitions;
	vector<State> states;
	FirstTable firsts;

	void first() {
		for (auto& sy : gr.symbolTable) {
			Symbol s = gr.getSymbol(sy.name);
			if (sy.terminal) {
				firsts[s].symbols.insert(s);
			}
		}
		bool changed = true;
		while (changed) {
			changed = false;
			for (auto& prod : gr.productions) {
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

	void closure(State& S) {
		bool changed = 1;
		while (changed) {
			changed = 0;
			vector<Item> current(S.begin(), S.end());

			for (auto& item : current) {
				Production& p = gr.productions[item.production];
				if (item.dot >= p.right.size()) continue;

				Symbol nextSymbol = p.right[item.dot];
				if (gr.symbolTable[nextSymbol].terminal) continue;

				vector<Symbol> nextCad;
				for (int i = item.dot + 1; i < p.right.size(); i++) {
					nextCad.push_back(p.right[i]);
				}
				nextCad.push_back(item.lookahead);

				FirstSet f = firstCadena(nextCad);

				for (int i = 0; i < gr.productions.size(); i++) {
					if (gr.productions[i].left == nextSymbol) {
						for (auto& lAhead : f.symbols) {
							Item newItem = { i,0,lAhead };
							auto result = S.insert(newItem);
							if (result.second) changed = 1;
						}
					}
				}
			}
		}
	}

	State gotoState(State& S, Symbol x) {
		State t; //
		for (auto& item : S) {
			Production& p = gr.productions[item.production];
			if (item.dot < p.right.size() && p.right[item.dot] == x) {
				t.insert({ item.production,item.dot + 1,item.lookahead });
			}
		}
		if (!t.empty()) closure(t);
		return t;
	}

	void collecionCanonica() {

		Symbol start = gr.productions[0].left;
		Symbol eof = gr.getSymbol("eof");
		State CC0 = { {0,0,eof} };

		closure(CC0);
		states.push_back(CC0);

		for (int i = 0; i < states.size(); i++) {
			for (int idS = 0; idS < gr.symbolTable.size(); idS++) {
				if (idS == start || idS == eof) continue;

				State newState = gotoState(states[i], idS);
				if (newState.empty()) continue;
				
				int dest = -1;
				for (int j = 0; j < states.size(); j++) {
					if (states[j] == newState) {
						dest = j;
						break;
					}
				}
				if (dest == -1) {
					dest = states.size();
					states.push_back(newState);
				}

				transitions[{i, idS}] = dest;

			}
		}
	}

	
public:
	LR1(Grammar g) : gr(g) {
		gr.augment();
		first();
		collecionCanonica();
	}

	void printFirsts() {
		cout << "\n--- FIRST ---\n";
		for (size_t i = 0; i < gr.symbolTable.size(); ++i) {
			cout << "First(" << gr.symbolTable[i].name << ") = { ";
			string sep = "";

			if (firsts[i].epsilon) {
				cout << sep <<"'' ";
				sep = ", ";
			}

			for (Symbol sym : firsts[i].symbols) {
				
				cout << sep<< gr.symbolTable[sym].name << " ";
				sep = ", ";
			}

			cout << " }\n";
		}
	}

	void printGrammar() {
		gr.print();
	}

	void printState(int stateId, const State& S) {
		std::cout << "CC" << stateId << ":" << std::endl;
		for (const auto& item : S) {
			const auto& prod = gr.productions[item.production];

			std::cout << "  [" << gr.symbolTable[prod.left].name << " -> ";

			for (size_t i = 0; i <= prod.right.size(); ++i) {
				if (i == item.dot) {
					std::cout << ". ";
				}
				if (i < prod.right.size()) {
					std::cout << gr.symbolTable[prod.right[i]].name << " ";
				}
			}

			std::cout << ", " << gr.symbolTable[item.lookahead].name << "]" << std::endl;
		}
	}
		
	void printStates() {
		cout << "\n--- STATES ---" << endl;
		for (int i = 0; i < states.size(); i++)
			printState(i, states[i]);

		cout << "\n--- TRANSICIONES ---\n";
		for (auto& t : transitions)
			cout << "(CC" << t.first.first << ", " << gr.symbolTable[t.first.second].name
			<< ") --> CC" << t.second << "\n";
	}
};

int main() {
	Grammar gr;
	gr.loadFile("input.txt");
	LR1 lr(gr);
	lr.printGrammar();
	lr.printFirsts();
	lr.printStates();

	return 0;
}