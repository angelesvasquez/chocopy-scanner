#ifndef FRONTEND_H
#define FRONTEND_H

#include <filesystem>

#include "../../parser/src/Grammar.h"
#include "../../parser/src/LR1.h"
#include "../../scanner/src/scanner.h"

using namespace std;

class Frontend {
  std::filesystem::path sourceFile;
  Scanner scanner;
  LR1 parser;

public:
  explicit Frontend(const std::filesystem::path &sourceFile);

  bool run(bool trace, const std::filesystem::path &astFile,
           const std::filesystem::path &reportFile);
};

#endif
