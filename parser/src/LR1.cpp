// Generador LR1

#include "LR1.h"

#include <iostream>
#include <vector>
#include <unordered_set>
#include <set>
#include <algorithm>
#include <map>
#include <fstream>
#include <unordered_map>

using namespace std;

LR1::LR1(Grammar g)
	: gr(g)
{
	gr.augment();
}

void LR1::first()
{
	for (auto &sy : gr.symbolTable)
	{
		Symbol s = gr.getSymbol(sy.name);
		if (sy.terminal)
		{
			firsts[s].symbols.insert(s);
		}
	}
	bool changed = true;
	while (changed)
	{
		changed = false;
		for (auto &prod : gr.productions)
		{
			Symbol A = prod.left;
			vector<Symbol> b = prod.right;
			int k = (int)b.size();

			if (k == 0)
			{
				if (!firsts[A].epsilon)
				{
					firsts[A].epsilon = 1;
					changed = 1;
				}
				continue;
			}

			unordered_set<Symbol> rhs = firsts[b[0]].symbols;
			int i = 1;
			for (; i <= k - 1 && firsts[b[i - 1]].epsilon; i++)
			{
				for (Symbol sym : firsts[b[i]].symbols)
				{
					rhs.insert(sym);
				}
			}
			bool rhsEpsilon = (i == k && firsts[b[k - 1]].epsilon);

			for (Symbol sym : rhs)
			{
				if (firsts[A].symbols.insert(sym).second)
				{
					changed = 1;
				}
			}
			if (rhsEpsilon && !firsts[A].epsilon)
			{
				firsts[A].epsilon = 1;
				changed = 1;
			}
		}
	}
}

void LR1::follow()
{
	follows.clear();
	follows[gr.augmentedStart].insert(gr.findSymbol("eof"));

	bool changed = true;
	while (changed)
	{
		changed = false;
		for (auto &prod : gr.productions)
		{
			for (size_t i = 0; i < prod.right.size(); i++)
			{
				Symbol B = prod.right[i];
				if (gr.symbolTable[B].terminal)
				{
					continue;
				}

				vector<Symbol> beta(prod.right.begin() + i + 1, prod.right.end());
				FirstSet fb = firstCadena(beta);

				for (Symbol sym : fb.symbols)
				{
					if (follows[B].insert(sym).second)
					{
						changed = true;
					}
				}
				if (fb.epsilon)
				{
					for (Symbol sym : follows[prod.left])
					{
						if (follows[B].insert(sym).second)
						{
							changed = true;
						}
					}
				}
			}
		}
	}
}

FirstSet LR1::firstCadena(vector<Symbol> &beta)
{
	FirstSet result;
	if (beta.empty())
	{
		result.epsilon = true;
		return result;
	}
	result.symbols = firsts[beta[0]].symbols;
	int i = 1;
	while (i <= beta.size() - 1 && firsts[beta[i - 1]].epsilon)
	{
		for (Symbol sym : firsts[beta[i]].symbols)
		{
			result.symbols.insert(sym);
		}
		i++;
	}
	if (i == beta.size() && firsts[beta[i - 1]].epsilon)
	{
		result.epsilon = true;
	}
	return result;
}

void LR1::closure(State &S)
{
	bool changed = 1;
	while (changed)
	{
		changed = 0;
		vector<Item> current(S.begin(), S.end());

		for (auto &item : current)
		{
			Production &p = gr.productions[item.production];
			if (item.dot >= p.right.size())
			{
				continue;
			}

			Symbol nextSymbol = p.right[item.dot];
			if (gr.symbolTable[nextSymbol].terminal)
			{
				continue;
			}

			vector<Symbol> nextCad;
			for (int i = item.dot + 1; i < p.right.size(); i++)
			{
				nextCad.push_back(p.right[i]);
			}
			nextCad.push_back(item.lookahead);

			FirstSet f = firstCadena(nextCad);

			for (int i = 0; i < gr.productions.size(); i++)
			{
				if (gr.productions[i].left == nextSymbol)
				{
					for (auto &lAhead : f.symbols)
					{
						Item newItem = {i, 0, lAhead};
						auto result = S.insert(newItem);
						if (result.second)
						{
							changed = 1;
						}
					}
				}
			}
		}
	}
}

State LR1::gotoState(State &S, Symbol x)
{
	State t;
	for (auto &item : S)
	{
		Production &p = gr.productions[item.production];
		if (item.dot < p.right.size() && p.right[item.dot] == x)
		{
			t.insert({item.production, item.dot + 1, item.lookahead});
		}
	}
	if (!t.empty())
	{
		closure(t);
	}
	return t;
}

void LR1::canonicalCollection()
{

	Symbol start = gr.productions[0].left;
	Symbol eof = gr.getSymbol("eof");
	State CC0 = {{0, 0, eof}};

	closure(CC0);
	states.push_back(CC0);

	for (int i = 0; i < states.size(); i++)
	{
		for (int idS = 0; idS < gr.symbolTable.size(); idS++)
		{
			if (idS == start || idS == eof)
			{
				continue;
			}

			State newState = gotoState(states[i], idS);
			if (newState.empty())
			{
				continue;
			}

			int dest = -1;
			for (int j = 0; j < states.size(); j++)
			{
				if (states[j] == newState)
				{
					dest = j;
					break;
				}
			}
			if (dest == -1)
			{
				dest = states.size();
				states.push_back(newState);
			}

			transitions[{i, idS}] = dest;
		}
	}
}

bool LR1::setAction(int i, Symbol s, TypeAction t, int j)
{

	Action newAction = {t, j};
	auto key = make_pair(i, s);
	if (action.find(key) == action.end())
	{
		action[key] = newAction;
		return 1;
	}

	if (action[key] == newAction)
	{
		return 1;
	}

	bool shift = action[key].type == Shift || newAction.type == Shift;

	vector<Item> causes;
	for (const Item &it : states[i])
	{
		const Production &p = gr.productions[it.production];
		bool red = it.dot >= (int)p.right.size() && it.lookahead == s;
		bool sh = it.dot < (int)p.right.size() && p.right[it.dot] == s;
		if (red || sh)
		{
			causes.push_back(it);
		}
	}

	conflicts.push_back({i, s, shift ? "shift/reduce" : "reduce/reduce", action[key], newAction, causes});

	cout << "CONFLICTO " << (shift ? "shift/reduce" : "reduce/reduce") << " en estado " << i << ", simbolo " << gr.symbolTable[s].name << endl;

	for (const Item &it : causes)
	{
		printItem(it);
	}

	return 0;
}

bool LR1::fillTable()
{
	bool NoConflict = 1;
	Symbol eof = gr.getSymbol("eof");
	for (int i = 0; i < states.size(); i++)
	{

		for (auto &item : states[i])
		{
			Production &p = gr.productions[item.production];

			if (item.dot >= p.right.size() && p.left != gr.augmentedStart)
			{
				if (!setAction(i, item.lookahead, Reduce, item.production))
				{
					NoConflict = 0;
				}
				continue;
			}
			if (item.dot >= p.right.size() && item.lookahead == eof)
			{
				setAction(i, eof, Accept);
				continue;
			}
		}
	}

	for (auto &t : transitions)
	{
		int i = t.first.first;
		Symbol c = t.first.second;
		if (gr.symbolTable[c].terminal)
		{
			if (!setAction(i, c, Shift, t.second))
			{
				NoConflict = 0;
			}
		}
		else
		{
			gotoT[{i, c}] = t.second;
		}
	}

	return NoConflict;
}

void LR1::printItem(const Item &item)
{
	const auto &prod = gr.productions[item.production];
	cout << "  [" << gr.symbolTable[prod.left].name << " -> ";
	for (int i = 0; i <= prod.right.size(); ++i)
	{
		if ((int)i == item.dot)
		{
			cout << ". ";
		}
		if (i < prod.right.size())
		{
			cout << gr.symbolTable[prod.right[i]].name << " ";
		}
	}
	cout << ", " << gr.symbolTable[item.lookahead].name << "]\n";
}

void LR1::printFirsts()
{
	cout << "\n--- FIRST ---\n";
	for (size_t i = 0; i < gr.symbolTable.size(); ++i)
	{
		cout << "First(" << gr.symbolTable[i].name << ") = { ";
		string sep = "";

		if (firsts[i].epsilon)
		{
			cout << sep << "'' ";
			sep = ", ";
		}

		for (Symbol sym : firsts[i].symbols)
		{

			cout << sep << gr.symbolTable[sym].name << " ";
			sep = ", ";
		}

		cout << " }\n";
	}
}

void LR1::printFollows()
{
	cout << "\n--- FOLLOW ---\n";
	for (size_t i = 0; i < gr.symbolTable.size(); ++i)
	{
		if (gr.symbolTable[i].terminal)
		{
			continue;
		}
		cout << "Follow(" << gr.symbolTable[i].name << ") = { ";
		string sep = "";
		for (Symbol sym : follows[i])
		{
			cout << sep << gr.symbolTable[sym].name;
			sep = ", ";
		}
		cout << " }\n";
	}
}

void LR1::printState(int stateId, const State &S)
{
	std::cout << "CC" << stateId << ":" << std::endl;
	for (const auto &item : S)
	{
		const auto &prod = gr.productions[item.production];

		std::cout << "  [" << gr.symbolTable[prod.left].name << " -> ";

		for (size_t i = 0; i <= prod.right.size(); ++i)
		{
			if (i == item.dot)
			{
				std::cout << ". ";
			}
			if (i < prod.right.size())
			{
				std::cout << gr.symbolTable[prod.right[i]].name << " ";
			}
		}

		std::cout << ", " << gr.symbolTable[item.lookahead].name << "]" << std::endl;
	}
}

void LR1::printStates()
{
	cout << "\n--- STATES ---" << endl;
	for (int i = 0; i < states.size(); i++)
	{
		printState(i, states[i]);
	}

	cout << "\n--- TRANSICIONES ---\n";
	for (auto &t : transitions)
	{
		cout << "(CC" << t.first.first << ", " << gr.symbolTable[t.first.second].name << ") --> CC" << t.second << "\n";
	}
}

void LR1::printTables()
{
	vector<Symbol> terms, nonterms;
	for (int s = 0; s < (int)gr.symbolTable.size(); s++)
	{
		if (s == gr.augmentedStart)
		{
			continue;
		}
		(gr.symbolTable[s].terminal ? terms : nonterms).push_back(s);
	}
	cout << "\n---- ACTION | GOTO ----\n\nEstado";
	for (Symbol t : terms)
	{
		cout << "\t" << gr.symbolTable[t].name;
	}
	cout << " |";
	for (Symbol n : nonterms)
	{
		cout << "\t" << gr.symbolTable[n].name;
	}
	cout << "\n";
	for (int i = 0; i < (int)states.size(); i++)
	{
		cout << i;
		for (Symbol t : terms)
		{
			cout << "\t";
			auto it = action.find({i, (int)t});
			if (it != action.end())
			{
				if (it->second.type == Shift)
				{
					cout << "s" << it->second.v;
				}
				if (it->second.type == Reduce)
				{
					cout << "r" << it->second.v;
				}
				if (it->second.type == Accept)
				{
					cout << "acc";
				}
			}
		}
		for (Symbol n : nonterms)
		{
			cout << "\t";
			auto it = gotoT.find({i, (int)n});
			if (it != gotoT.end())
			{
				cout << it->second;
			}
		}
		cout << "\n";
	}
}

bool LR1::build(bool verbose)
{
	if (verbose)
	{
		gr.print();
	}
	first();
	follow();
	if (verbose)
	{
		printFirsts();
		printFollows();
	}
	canonicalCollection();
	if (verbose)
	{
		printStates();
	}

	bool success = fillTable();

	if (verbose)
	{
		printTables();
		exportJSON("../visualizer/lr1.json");
		exportJS("../visualizer/data.js");
	}

	return success;
}

string LR1::typeName(TypeAction t)
{
	return t == Shift ? "shift" : t == Reduce ? "reduce"
											  : "accept";
}
string LR1::escape(const string &s)
{
	string o;
	for (char c : s)
	{
		if (c == '"')
		{
			o += "\\\"";
		}
		else if (c == '\\')
		{
			o += "\\\\";
		}
		else if (c == '\n')
		{
			o += "\\n";
		}
		else
		{
			o += c;
		}
	}
	return o;
}
void LR1::writeJSON(ostream &f) const
{
	f << "{";

	// symbols
	f << "\"symbols\":[";
	for (size_t i = 0; i < gr.symbolTable.size(); i++)
	{
		if (i)
		{
			f << ",";
		}
		f << "{\"id\":" << i
		  << ",\"name\":\"" << escape(gr.symbolTable[i].name) << "\""
		  << ",\"terminal\":" << (gr.symbolTable[i].terminal ? "true" : "false") << "}";
	}
	f << "],";

	// productions
	f << "\"productions\":[";
	for (size_t i = 0; i < gr.productions.size(); i++)
	{
		if (i)
		{
			f << ",";
		}
		f << "{\"left\":" << gr.productions[i].left << ",\"right\":[";
		for (size_t j = 0; j < gr.productions[i].right.size(); j++)
		{
			if (j)
			{
				f << ",";
			}
			f << gr.productions[i].right[j];
		}
		f << "]}";
	}
	f << "],";

	// states
	f << "\"states\":[";
	for (size_t i = 0; i < states.size(); i++)
	{
		if (i)
		{
			f << ",";
		}
		f << "{\"id\":" << i << ",\"items\":[";
		size_t k = 0;
		for (const auto &it : states[i])
		{
			if (k++)
			{
				f << ",";
			}
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
		for (auto &t : transitions)
		{
			if (n++)
			{
				f << ",";
			}
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
		for (auto &kv : firsts)
		{
			if (n++)
			{
				f << ",";
			}
			f << "{\"symbol\":" << kv.first << ",\"set\":[";
			size_t m = 0;
			for (Symbol s : kv.second.symbols)
			{
				if (m++)
				{
					f << ",";
				}
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
		for (auto &kv : action)
		{
			if (n++)
			{
				f << ",";
			}
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
		for (auto &kv : gotoT)
		{
			if (n++)
			{
				f << ",";
			}
			f << "{\"state\":" << kv.first.first
			  << ",\"symbol\":" << kv.first.second
			  << ",\"to\":" << kv.second << "}";
		}
	}
	f << "],";

	// conflicts
	f << "\"conflicts\":[";
	for (size_t i = 0; i < conflicts.size(); i++)
	{
		if (i)
		{
			f << ",";
		}
		const auto &c = conflicts[i];
		f << "{\"state\":" << c.state
		  << ",\"terminal\":" << c.terminal
		  << ",\"kind\":\"" << c.kind << "\""
		  << ",\"existing\":{\"type\":\"" << typeName(c.existing.type)
		  << "\",\"value\":" << c.existing.v << "}"
		  << ",\"incoming\":{\"type\":\"" << typeName(c.incoming.type)
		  << "\",\"value\":" << c.incoming.v << "}"
		  << ",\"causes\":[";
		for (size_t j = 0; j < c.causes.size(); j++)
		{
			if (j)
			{
				f << ",";
			}
			f << "{\"production\":" << c.causes[j].production
			  << ",\"dot\":" << c.causes[j].dot
			  << ",\"lookahead\":" << c.causes[j].lookahead << "}";
		}
		f << "]}";
	}
	f << "]";

	f << "}";
}

void LR1::exportJSON(const string &filename) const
{
	ofstream f(filename);
	if (!f)
	{
		cerr << "No se pudo crear " << filename << "\n";
		return;
	}
	writeJSON(f);
}
void LR1::exportJS(const string &filename) const
{
	ofstream f(filename);
	if (!f)
	{
		cerr << "No se pudo crear " << filename << "\n";
		return;
	}
	f << "window.LR1_DATA = ";
	writeJSON(f);
	f << ";\n";
}

string LR1::stackStatesStr()
{
	string o;
	for (size_t i = 0; i < stateStack.size(); i++)
	{
		if (i)
		{
			o += " ";
		}
		o += to_string(stateStack[i]);
	}
	return o;
}

string LR1::stackSymbolsStr()
{
	string o = "$";
	for (Symbol s : symStack)
	{
		o += " " + gr.symbolTable[s].name;
	}
	return o;
}

string LR1::productionStr(int p)
{
	Production &pr = gr.productions[p];
	string o = gr.symbolTable[pr.left].name + " -> ";
	if (pr.right.empty())
	{
		o += "''";
	}
	for (size_t i = 0; i < pr.right.size(); i++)
	{
		if (i)
		{
			o += " ";
		}
		o += gr.symbolTable[pr.right[i]].name;
	}
	return o;
}

const TokenData &LR1::tokenAt(size_t i)
{
	while (tokBuf.size() <= i && !scnEof)
	{
		TokenData t = scn->gettoken();
		tokBuf.push_back(t);
		if (t.symbol == "eof")
		{
			scnEof = true;
		}
	}
	return i < tokBuf.size() ? tokBuf[i] : tokBuf.back();
}

string LR1::inputStr(int pos)
{
	string o;
	for (int k = 0; k < inputWindow; k++)
	{
		const TokenData &t = tokenAt(pos + k);
		if (k)
		{
			o += " ";
		}
		if (t.symbol == "eof")
		{
			return o + "$";
		}
		o += t.symbol;
	}
	return o + " ...";
}

void LR1::record(int pos, const string &action)
{
	trace.push_back({stackStatesStr(), stackSymbolsStr(), inputStr(pos), action});
}

void LR1::setPanicSet(const vector<string> &names)
{
	panicSet.clear();
	for (const string &n : names)
	{
		Symbol s = gr.findSymbol(n);
		if (s >= 0 && !gr.symbolTable[s].terminal)
		{
			panicSet.push_back(s);
		}
	}
}

bool LR1::inFollow(Symbol A, const TokenData &t)
{
	Symbol a = gr.findSymbol(t.symbol);
	return a >= 0 && follows[A].count(a) > 0;
}

Symbol LR1::chooseAnchor(int state, const TokenData &t)
{
	Symbol fallback = -1;
	for (Symbol A : panicSet)
	{
		if (gotoT.find({state, A}) == gotoT.end())
		{
			continue;
		}
		if (inFollow(A, t))
		{
			return A;
		}
		if (fallback < 0)
		{
			fallback = A;
		}
	}
	return fallback;
}

bool LR1::panicRecover(int &pos, int &lastRecoveryPos)
{
	if (pos == lastRecoveryPos)
	{
		if (tokenAt(pos).symbol == "eof")
		{
			record(pos, "aborta");
			return false;
		}
		string d = tokenAt(pos).symbol;
		pos++;
		record(pos, "descarta " + d);
	}

	Symbol A = -1;
	int popped = 0;
	while (true)
	{
		A = chooseAnchor(stateStack.back(), tokenAt(pos));
		if (A >= 0)
		{
			break;
		}
		if (stateStack.size() == 1)
		{
			record(pos, "aborto");
			return false;
		}
		stateStack.pop_back();
		symStack.pop_back();
		popped++;
	}
	record(pos, "1: desapila " + to_string(popped) + ", el tope tiene Goto para " + gr.symbolTable[A].name);

	string skipped;
	while (!inFollow(A, tokenAt(pos)))
	{
		if (tokenAt(pos).symbol == "eof")
		{
			record(pos, "2: llega a $ sin FOLLOW(" + gr.symbolTable[A].name + "), aborta");
			return false;
		}
		skipped += (skipped.empty() ? "" : " ") + tokenAt(pos).symbol;
		pos++;
	}
	record(pos, "2: " + (skipped.empty() ? string("no consume nada") : "consume " + skipped) + " hasta FOLLOW(" + gr.symbolTable[A].name + ")");

	int g = gotoT[{stateStack.back(), A}];
	symStack.push_back(A);
	stateStack.push_back(g);
	record(pos, "3 y 4: apila " + gr.symbolTable[A].name + " y Goto = " + to_string(g));

	lastRecoveryPos = pos;
	return true;
}

bool LR1::parse(Scanner *scanner)
{
	stateStack.assign(1, 0);
	symStack.clear();
	trace.clear();
	syntaxErrors.clear();

	scn = scanner;
	tokBuf.clear();
	scnEof = false;

	int pos = 0;
	int lastRecoveryPos = -1;

	while (true)
	{
		TokenData la = tokenAt(pos);
		Symbol a = gr.findSymbol(la.symbol);
		int s = stateStack.back();

		auto it = a < 0 ? action.end() : action.find({s, a});
		if (it == action.end())
		{
			string lex = la.symbol == "eof" ? "fin de archivo" : "'" + (la.value.empty() ? la.symbol : la.value) + "'";
			syntaxErrors.push_back("fila " + to_string(la.row) + ", col " + to_string(la.col) + ": token inesperado " + lex);
			record(pos, "error: Action[" + to_string(s) + ", " + la.symbol + "] vacia");

			if (!panicRecover(pos, lastRecoveryPos))
			{
				return false;
			}
			continue;
		}

		Action &act = it->second;
		if (act.type == Shift)
		{
			record(pos, "s" + to_string(act.v));
			symStack.push_back(a);
			stateStack.push_back(act.v);
			pos++;
		}
		else if (act.type == Reduce)
		{
			Production &pr = gr.productions[act.v];
			record(pos, "r" + to_string(act.v));
			for (size_t i = 0; i < pr.right.size(); i++)
			{
				symStack.pop_back();
				stateStack.pop_back();
			}
			symStack.push_back(pr.left);
			stateStack.push_back(gotoT[{stateStack.back(), pr.left}]);
		}
		else
		{
			record(pos, "acepta");
			return syntaxErrors.empty();
		}
	}
}

void LR1::printSyntaxErrors()
{
	if (syntaxErrors.empty())
	{
		return;
	}
	cout << "\n--- ERRORES SINTACTICOS ---\n";
	for (size_t i = 0; i < syntaxErrors.size(); i++)
	{
		cout << (i + 1) << ". " << syntaxErrors[i] << "\n";
	}
}

void LR1::printTrace()
{
	vector<string> head = {"Estados", "Simbolos", "Entrada", "Accion"};
	size_t w[4] = {head[0].size(), head[1].size(), head[2].size(), head[3].size()};
	for (auto &r : trace)
	{
		w[0] = max(w[0], r.states.size());
		w[1] = max(w[1], r.symbols.size());
		w[2] = max(w[2], r.input.size());
	}
	auto pad = [](string x, size_t n)
	{
		return x + string(n > x.size() ? n - x.size() : 0, ' ');
	};

	cout << "\n--- SEGUIMIENTO DEL ANALISIS ---\n";
	if (!panicSet.empty())
	{
		cout << "Modo panico, N = {";
		for (size_t i = 0; i < panicSet.size(); i++)
		{
			cout << (i ? ", " : "") << gr.symbolTable[panicSet[i]].name;
		}
		cout << "}\n";
	}
	cout << pad(head[0], w[0]) << "  " << pad(head[1], w[1]) << "  " << pad(head[2], w[2]) << "  " << head[3] << "\n";
	cout << string(w[0] + w[1] + w[2] + 6 + 20, '-') << "\n";
	for (auto &r : trace)
	{
		cout << pad(r.states, w[0]) << "  " << pad(r.symbols, w[1]) << "  " << pad(r.input, w[2]) << "  " << r.action << "\n";
	}
}