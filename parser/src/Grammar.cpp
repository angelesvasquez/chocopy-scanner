#include "Grammar.h"
#include <iostream>
#include <fstream>
#include <sstream>

Symbol Grammar::getSymbol(string name, bool isTerminalD)
{
	if (symbolMap.find(name) == symbolMap.end())
	{
		Symbol newId = symbolTable.size();
		symbolMap[name] = newId;
		symbolTable.push_back({name, isTerminalD});
	}
	else
	{
		if (!isTerminalD)
		{
			symbolTable[symbolMap[name]].terminal = 0;
		}
	}
	return symbolMap[name];
}

Symbol Grammar::findSymbol(string name)
{
	auto it = symbolMap.find(name);
	return it == symbolMap.end() ? -1 : it->second;
}

void Grammar::augment()
{
	Symbol originalStart = productions[0].left;
	string name = symbolTable[originalStart].name;

	Symbol id = getSymbol(name + "'", false);
	augmentedStart = id;

	productions.insert(productions.begin(), {id, {originalStart}});

	getSymbol("eof", true);
}

bool Grammar::loadFile(string filename)
{
	ifstream file(filename);
	if (!file.is_open())
	{
		std::cerr << "No se pudo abrir el archivo: " << filename << "\n";
		return 0;
	}
	string line;
	while (getline(file, line))
	{
		if (line.empty())
		{
			continue;
		}
		stringstream ss(line);
		string leftStr, arrow;
		ss >> leftStr >> arrow;
		if (arrow != "->")
		{
			continue;
		}

		Symbol leftId = getSymbol(leftStr, 0);
		Production prod;
		prod.left = leftId;

		string rightStr;
		while (ss >> rightStr)
		{
			if (rightStr == "''")
			{
				continue;
			}
			Symbol id = getSymbol(rightStr, 1);
			prod.right.push_back(id);
		}
		productions.push_back(prod);
	}
	file.close();
	return 1;
}

void Grammar::print()
{

	cout << "\n--- PRODUCCIONES ---\n";
	for (int i = 0; i < productions.size(); i++)
	{
		cout << "(" << i << ") " << symbolTable[productions[i].left].name << " -> ";
		if (productions[i].right.empty())
		{
			cout << "''";
		}
		for (Symbol sym : productions[i].right)
		{
			cout << symbolTable[sym].name << " ";
		}
		cout << "\n";
	}
}