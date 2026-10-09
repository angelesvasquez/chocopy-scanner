// LR1

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

enum TypeAction { Shift, Reduce, Accept};

struct Action{
	TypeAction type;
	int v;

	bool operator==(const Action& a) const {
		return type == a.type && v == a.v;
	}
};

struct Conflict {
	int state;
	int terminal;
	string kind;
	Action existing, incoming;
	vector<Item> causes;
};

class LR1 {
	Grammar gr;
	map<pair<int, int>, int> transitions;
	vector<State> states;
	FirstTable firsts;
	// Action[Estado i,symbol] = Shift 7, Reduce 2, accept
	map<pair<int, Symbol>, Action> action; 
	map<pair<int, int>, int> gotoT;
	vector<Conflict> conflicts;

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

	void clousure(State& S) {
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
		State t;
		for (auto& item : S) {
			Production& p = gr.productions[item.production];
			if (item.dot < p.right.size() && p.right[item.dot] == x) {
				t.insert({ item.production,item.dot + 1,item.lookahead });
			}
		}
		if (!t.empty()) clousure(t);
		return t;
	}

	void canonicalCollection() {

		Symbol start = gr.productions[0].left;
		Symbol eof = gr.getSymbol("eof");
		State CC0 = { {0,0,eof} };

		clousure(CC0);
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

	static string typeName(TypeAction t) {
		return t == Shift ? "shift" : t == Reduce ? "reduce" : "accept";
	}
	static string escape(const string& s) {
		string o;
		for (char c : s) {
			if (c == '"')       o += "\\\"";
			else if (c == '\\') o += "\\\\";
			else if (c == '\n') o += "\\n";
			else                o += c;
		}
		return o;
	}

	void writeJSON(ostream& f) const {
		f << "{";

		// symbols
		f << "\"symbols\":[";
		for (size_t i = 0; i < gr.symbolTable.size(); i++) {
			if (i) f << ",";
			f << "{\"id\":" << i
				<< ",\"name\":\"" << escape(gr.symbolTable[i].name) << "\""
				<< ",\"terminal\":" << (gr.symbolTable[i].terminal ? "true" : "false") << "}";
		}
		f << "],";

		// productions
		f << "\"productions\":[";
		for (size_t i = 0; i < gr.productions.size(); i++) {
			if (i) f << ",";
			f << "{\"left\":" << gr.productions[i].left << ",\"right\":[";
			for (size_t j = 0; j < gr.productions[i].right.size(); j++) {
				if (j) f << ",";
				f << gr.productions[i].right[j];
			}
			f << "]}";
		}
		f << "],";

		// states
		f << "\"states\":[";
		for (size_t i = 0; i < states.size(); i++) {
			if (i) f << ",";
			f << "{\"id\":" << i << ",\"items\":[";
			size_t k = 0;
			for (const auto& it : states[i]) {
				if (k++) f << ",";
				f << "{\"production\":" << it.production
					<< ",\"dot\":" << it.dot
					<< ",\"lookahead\":" << it.lookahead << "}";
			}
			f << "]}";
		}
		f << "],";

		// transitions
		f << "\"transitions\":[";
		{
			size_t n = 0;
			for (auto& t : transitions) {
				if (n++) f << ",";
				f << "{\"from\":" << t.first.first
					<< ",\"symbol\":" << t.first.second
					<< ",\"to\":" << t.second << "}";
			}
		}
		f << "],";

		// firsts
		f << "\"firsts\":[";
		{
			size_t n = 0;
			for (auto& kv : firsts) {
				if (n++) f << ",";
				f << "{\"symbol\":" << kv.first << ",\"set\":[";
				size_t m = 0;
				for (Symbol s : kv.second.symbols) {
					if (m++) f << ",";
					f << s;
				}
				f << "],\"epsilon\":" << (kv.second.epsilon ? "true" : "false") << "}";
			}
		}
		f << "],";

		// action
		f << "\"action\":[";
		{
			size_t n = 0;
			for (auto& kv : action) {
				if (n++) f << ",";
				f << "{\"state\":" << kv.first.first
					<< ",\"symbol\":" << kv.first.second
					<< ",\"type\":\"" << typeName(kv.second.type) << "\""
					<< ",\"value\":" << kv.second.v << "}";
			}
		}
		f << "],";

		// goto
		f << "\"goto\":[";
		{
			size_t n = 0;
			for (auto& kv : gotoT) {
				if (n++) f << ",";
				f << "{\"state\":" << kv.first.first
					<< ",\"symbol\":" << kv.first.second
					<< ",\"to\":" << kv.second << "}";
			}
		}
		f << "],";

		// conflicts
		f << "\"conflicts\":[";
		for (size_t i = 0; i < conflicts.size(); i++) {
			if (i) f << ",";
			const auto& c = conflicts[i];
			f << "{\"state\":" << c.state
				<< ",\"terminal\":" << c.terminal
				<< ",\"kind\":\"" << c.kind << "\""
				<< ",\"existing\":{\"type\":\"" << typeName(c.existing.type)
				<< "\",\"value\":" << c.existing.v << "}"
				<< ",\"incoming\":{\"type\":\"" << typeName(c.incoming.type)
				<< "\",\"value\":" << c.incoming.v << "}"
				<< ",\"causes\":[";
			for (size_t j = 0; j < c.causes.size(); j++) {
				if (j) f << ",";
				f << "{\"production\":" << c.causes[j].production
					<< ",\"dot\":" << c.causes[j].dot
					<< ",\"lookahead\":" << c.causes[j].lookahead << "}";
			}
			f << "]}";
		}
		f << "]";

		f << "}";
	}

public:
	LR1(Grammar g) : gr(g) {
		gr.augment();
		first();
		canonicalCollection();
	}

	void printItem(const Item& item) {
		const auto& prod = gr.productions[item.production];
		cout << "  [" << gr.symbolTable[prod.left].name << " -> ";
		for (int i = 0; i <= prod.right.size(); ++i) {
			if ((int)i == item.dot) cout << ". ";
			if (i < prod.right.size()) cout << gr.symbolTable[prod.right[i]].name << " ";
		}
		cout << ", " << gr.symbolTable[item.lookahead].name << "]\n";
	}

	bool setAction(int i, Symbol s, TypeAction t, int j = 0) {
		Action newAction = { t,j };
		auto key = make_pair(i, s);
		if (action.find(key) == action.end()) {
			action[key] = newAction;
			return 1;
		}
		
		if (action[key] == newAction) return 1;

		bool shift = action[key].type == Shift|| newAction.type == Shift;
		
		vector<Item> causes;
		for (const Item& it : states[i]) {
			const Production& p = gr.productions[it.production];
			bool red = it.dot >= (int)p.right.size() && it.lookahead == s;
			bool sh = it.dot < (int)p.right.size() && p.right[it.dot] == s;
			if (red || sh) causes.push_back(it);
		}

		conflicts.push_back({i, s, shift ? "shift/reduce" : "reduce/reduce", action[key], newAction, causes});

		cout << "CONFLICTO " << (shift ? "shift/reduce" : "reduce/reduce")
			<< " en estado " << i
			<< ", simbolo " << gr.symbolTable[s].name << endl;

		for (const Item& it : causes) printItem(it);

		return 0;
	}

	bool fillTable() {
		bool NoConflict = 1;
		Symbol eof = gr.getSymbol("eof");
		for (int i = 0; i < states.size(); i++) {

			for (auto& item : states[i]) {
				Production& p = gr.productions[item.production];

				if (item.dot >= p.right.size() && p.left != gr.symbolTable.size()-2) {
					if(!setAction(i, item.lookahead, Reduce, item.production)) NoConflict = 0;  // conflicto
					continue;
				}
				if (item.dot >= p.right.size() && item.lookahead == eof) {
					setAction(i, eof, Accept);
					continue;
				}
				// Ante un terminal
				/*Symbol c = p.right[item.dot];
				if (gr.symbolTable[c].terminal) {
					if(!setAction(i, c, Shift, transitions[{i, c}])) conflict = 0;
				}
				else {
					gotoT[{i, c}] = transitions[{i, c}];
				}*/

			}
		}

		for (auto& t : transitions) {
			int i = t.first.first;
			Symbol c = t.first.second;
			if (gr.symbolTable[c].terminal) {
				if (!setAction(i, c, Shift, t.second)) NoConflict = 0;
			}
			else {
				gotoT[{i, c}] = t.second;
			}
		}

		return NoConflict;
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
	
	void printTables() {
		vector<Symbol> terms, nonterms;
		for (int s = 0; s < (int)gr.symbolTable.size(); s++) {
			if (s == gr.symbolTable.size()-2) continue;
			(gr.symbolTable[s].terminal ? terms : nonterms).push_back(s);
		}
		cout << "\n---- ACTION | GOTO ----\n\nEstado";
		for (Symbol t : terms)    cout << "\t" << gr.symbolTable[t].name;
		cout << " |";
		for (Symbol n : nonterms) cout << "\t" << gr.symbolTable[n].name;
		cout << "\n";
		for (int i = 0; i < (int)states.size(); i++) {
			cout << i;
			for (Symbol t : terms) {
				cout << "\t";
				auto it = action.find({ i, (int)t });
				if (it != action.end()) {
					if (it->second.type == Shift)  cout << "s" << it->second.v;
					if (it->second.type == Reduce) cout << "r" << it->second.v;
					if (it->second.type == Accept) cout << "acc";
				}
			}
			for (Symbol n : nonterms) {
				cout << "\t";
				auto it = gotoT.find({ i, (int)n });
				if (it != gotoT.end()) cout << it->second;
			}
			cout << "\n";
		}
	}

	void exportJSON(const string& filename) const {
		ofstream f(filename);
		if (!f) { cerr << "No se pudo crear " << filename << "\n"; return; }
		writeJSON(f);
	}
	void exportJS(const string& filename) const {
		ofstream f(filename);
		if (!f) { cerr << "No se pudo crear " << filename << "\n"; return; }
		f << "window.LR1_DATA = ";
		writeJSON(f);
		f << ";\n";
	}
};


int main() {
	Grammar g;
	g.loadFile("input.txt");
	LR1 lr(g);
	lr.printGrammar();
	lr.printFirsts();
	lr.printStates();

	bool ok = lr.fillTable();
	lr.printTables();

	lr.exportJSON("../visualizer/lr1.json");
	lr.exportJS("../visualizer/data.js");
	return ok ? 0 : 1;
}