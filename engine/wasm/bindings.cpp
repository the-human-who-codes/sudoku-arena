// Exposes the JSON API to JavaScript (browser, web worker or Node).

#include <emscripten/bind.h>

#include "sudoku/api.hpp"

EMSCRIPTEN_BINDINGS(sudoku_engine) {
  emscripten::function("solve", &sudoku::api::solve);
  emscripten::function("generate", &sudoku::api::generate);
  emscripten::function("analyze", &sudoku::api::analyze);
  emscripten::function("hint", &sudoku::api::hint);
  emscripten::function("benchmark", &sudoku::api::benchmark);
  emscripten::function("version", &sudoku::api::version);
}
