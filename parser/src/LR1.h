#ifndef LR1_H
#define LR1_H

#include "Grammar.h"
#include "../../scanner/src/scanner.h"
#include <iostream>
#include <vector>
#include <unordered_set>
#include <set>
#include <algorithm>
#include <map>
#include <unordered_map>
#include <string>

using namespace std;

struct Item
{
	int production;
	int dot;
	Symbol lookahead;

	bool operator==(const Item &i) const
	{
		return (production == i.production &&
				dot == i.dot &&
				lookahead == i.lookahead);
	}

	bool operator<(const Item &i) const
	{
		if (production != i.production)
		{
			return production < i.production;
		}
		if (dot != i.dot)
		{
			return dot < i.dot;
		}
		return lookahead < i.lookahead;
	}
};

struct FirstSet
{
	unordered_set<Symbol> symbols;
	bool epsilon = 0;
};

using FirstTable = unordered_map<Symbol, FirstSet>;
using State = set<Item>;

enum TypeAction
{
	Shift,
	Reduce,
	Accept
};

struct Action
{
	TypeAction type;
	int v;

	bool operator==(const Action &a) const
	{
		return type == a.type && v == a.v;
	}
};

struct Conflict
{
	int state;
	int terminal;
	string kind;
	Action existing, incoming;
	vector<Item> causes;
};

struct TraceRow
{
	string states;
	string symbols;
	string input;
	string action;
};

class LR1
{
	Grammar gr;
	map<pair<int, int>, int> transitions;
	vector<State> states;
	FirstTable firsts;
	unordered_map<Symbol, set<Symbol>> follows;
	map<pair<int, Symbol>, Action> action;
	map<pair<int, int>, int> gotoT;
	vector<Conflict> conflicts;

	vector<int> stateStack;
	vector<Symbol> symStack;
	vector<TraceRow> trace;

	vector<Symbol> panicSet;
	vector<string> syntaxErrors;

	bool inFollow(Symbol A, const TokenData &t);
	Symbol chooseAnchor(int state, const TokenData &t);
	bool panicRecover(int &pos, int &lastRecoveryPos);

	Scanner *scn = nullptr;
	vector<TokenData> tokBuf;
	bool scnEof = false;
	static const int inputWindow = 8;

	string stackStatesStr();
	string stackSymbolsStr();
	string productionStr(int p);
	void record(int pos, const string &action);
	const TokenData &tokenAt(size_t i);
	string inputStr(int pos);

	void first();
	void follow();
	FirstSet firstCadena(vector<Symbol> &beta);
	void closure(State &S);
	State gotoState(State &S, Symbol x);
	void canonicalCollection();

	static string typeName(TypeAction t);
	static string escape(const string &s);
	void writeJSON(ostream &f) const;

public:
	LR1(Grammar g);

	bool setAction(int i, Symbol s, TypeAction t, int j = 0);
	bool fillTable();

	bool build(bool verbose = true);

	void printItem(const Item &item);
	void printFirsts();
	void printFollows();
	void printGrammar();

	void printState(int stateId, const State &S);
	void printStates();
	void printTables();

	void exportJSON(const string &filename) const;
	void exportJS(const string &filename) const;

	void setPanicSet(const vector<string> &names);
	bool parse(Scanner *scanner);
	int syntaxErrorCount() const { return (int)syntaxErrors.size(); }
	void printSyntaxErrors();
	void printTrace();
};

#endif