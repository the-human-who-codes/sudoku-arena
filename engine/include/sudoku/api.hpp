#pragma once

#include <string>

/// String-in, JSON-out facade used by the WebAssembly bindings and the CLI.
/// Every response has "ok"; failures carry {"ok": false, "error": "..."}.
namespace sudoku::api {

/// Solves with one algorithm ("logic", "smart", "naive" or "dlx") and returns the
/// solution, timing, stats and a replayable trace of at most `traceLimit` events.
std::string solve(const std::string& puzzle, const std::string& algorithm, int traceLimit);

/// New puzzle at a level ("easy" ... "master"); the same seed always gives the same puzzle.
std::string generate(const std::string& level, const std::string& seed);

/// Validity, solution count (0, 1, or 2 meaning "several") and difficulty rating.
std::string analyze(const std::string& puzzle);

/// Next step for a player: mistakes in `current`, else the deductions leading
/// to the next placement, else a revealed cell.
std::string hint(const std::string& puzzle, const std::string& current);

/// Times every algorithm on the same puzzle.
std::string benchmark(const std::string& puzzle);

std::string version();

}  // namespace sudoku::api
