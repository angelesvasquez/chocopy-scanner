#ifndef FRONTEND_H
#define FRONTEND_H

#include <filesystem>

#include "../../parser/src/LR1.h"
#include "../../scanner/src/scanner.h"

class Frontend {
  std::filesystem::path sourceFile;
  Scanner scanner;
  LR1 parser;

public:
  explicit Frontend(const std::filesystem::path &sourceFile);

  bool run(const std::filesystem::path &reportFile);
};

#endif
