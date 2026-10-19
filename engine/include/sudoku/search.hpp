#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include "sudoku/grid.hpp"

namespace sudoku {

enum class SolveStatus : std::uint8_t {
  Solved,        ///< A solution was found.
  Unsolvable,    ///< The givens are consistent but admit no solution.
  Invalid,       ///< A row, column or box repeats a given.
  Stuck,         ///< Logic only: no known technique applies and guessing is off.
  LimitReached,  ///< The node or step budget ran out first.
};

std::string_view toString(SolveStatus status);

struct SearchStats {
  std::uint64_t placements = 0;  ///< Digits written, forced or guessed.
  std::uint64_t guesses = 0;     ///< Placements chosen among several options.
  std::uint64_t backtracks = 0;  ///< Placements undone after a dead end.
};

/// Replayable log of a depth-first search: one packed event per write or erase.
class SearchTrace {
 public:
  enum class Kind : std::uint8_t { Guess = 0, Forced = 1, Undo = 2 };

  explicit SearchTrace(std::size_t limit) : limit_(limit) {}

  void record(Kind kind, int cell, int digit) {
    if (events_.size() < limit_) {
      events_.push_back(encode(kind, cell, digit));
    } else {
      truncated_ = true;
    }
  }

  /// Packs an event as kind << 11 | cell << 4 | digit.
  static constexpr std::uint32_t encode(Kind kind, int cell, int digit) {
    return static_cast<std::uint32_t>(kind) << 11 | static_cast<std::uint32_t>(cell) << 4 |
           static_cast<std::uint32_t>(digit);
  }

  const std::vector<std::uint32_t>& events() const { return events_; }
  bool truncated() const { return truncated_; }

 private:
  std::size_t limit_;
  std::vector<std::uint32_t> events_;
  bool truncated_ = false;
};

struct SearchOptions {
  std::uint64_t nodeLimit = 0;   ///< Maximum placements; 0 means unlimited.
  int solutionLimit = 1;         ///< Stop once this many solutions are found.
  SearchTrace* trace = nullptr;  ///< Optional event log for visualisation.
};

struct SearchResult {
  SolveStatus status = SolveStatus::Unsolvable;
  int solutions = 0;  ///< Solutions found, capped at SearchOptions::solutionLimit.
  Grid solution{};    ///< The first solution found.
  SearchStats stats;
};

/// Textbook backtracking: fills empty cells in reading order, trying 1-9 in turn.
SearchResult solveNaive(const Grid& puzzle, const SearchOptions& options = {});

/// Backtracking with constraint propagation (naked and hidden singles) that
/// always branches on the cell with the fewest candidates.
SearchResult solveSmart(const Grid& puzzle, const SearchOptions& options = {});

/// Knuth's Algorithm X over the 324-column exact-cover matrix, using Dancing Links.
SearchResult solveDlx(const Grid& puzzle, const SearchOptions& options = {});

/// Number of solutions, counting no further than `limit`. Invalid puzzles have none.
int countSolutions(const Grid& puzzle, int limit = 2);

}  // namespace sudoku
