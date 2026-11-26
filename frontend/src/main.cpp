#include "Frontend.h"

int main() {
  Frontend frontEnd("programa.py");
  return frontEnd.run("report.json") ? 0 : 1;
}
