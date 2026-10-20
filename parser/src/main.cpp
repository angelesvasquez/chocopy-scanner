#include "Grammar.h"
#include "LR1.h"
 
int main() {
	Grammar g;
	g.loadFile("input.txt");
	LR1 lr(g);

	return lr.build() ? 0 : 1;
}