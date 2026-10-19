#pragma once

#include <array>

#include "sudoku/search.hpp"

namespace sudoku::detail {

/// Digits used by each row, column and box.
struct Occupancy {
  std::array<Mask, 9> rows{};
  std::array<Mask, 9> cols{};
  std::array<Mask, 9> boxes{};

  Mask allowed(int cell) const {
    return static_cast<Mask>(kAllDigits &
                             ~(rows[rowOf(cell)] | cols[colOf(cell)] | boxes[boxOf(cell)]));
  }

  void set(int cell, int digit) {
    const Mask bit = digitBit(digit);
    rows[rowOf(cell)] |= bit;
    cols[colOf(cell)] |= bit;
    boxes[boxOf(cell)] |= bit;
  }

  void clear(int cell, int digit) {
    const auto keep = static_cast<Mask>(~digitBit(digit));
    rows[rowOf(cell)] &= keep;
    cols[colOf(cell)] &= keep;
    boxes[boxOf(cell)] &= keep;
  }

  /// Loads the givens; false when a digit repeats within a unit.
  bool load(const Grid& grid) {
    *this = {};
    for (int cell = 0; cell < kCells; ++cell) {
      const int digit = grid[cell];
      if (digit == 0) continue;
      if (digit > 9 || (allowed(cell) & digitBit(digit)) == 0) return false;
      set(cell, digit);
    }
    return true;
  }
};

/// State shared by the depth-first solvers: the grid, node budget, stats and trace.
class SearchBase {
 public:
  SearchBase(const Grid& puzzle, const SearchOptions& options)
      : grid_(puzzle), options_(options) {}

 protected:
  /// Writes a digit; returns false (and flags the abort) once the node budget is spent.
  bool place(int cell, int digit, SearchTrace::Kind kind) {
    if (options_.nodeLimit != 0 && result_.stats.placements >= options_.nodeLimit) {
      aborted_ = true;
      return false;
    }
    grid_[cell] = static_cast<std::uint8_t>(digit);
    occupancy_.set(cell, digit);
    ++result_.stats.placements;
    if (kind == SearchTrace::Kind::Guess) ++result_.stats.guesses;
    if (options_.trace != nullptr) options_.trace->record(kind, cell, digit);
    return true;
  }

  void undo(int cell) {
    const int digit = grid_[cell];
    grid_[cell] = 0;
    occupancy_.clear(cell, digit);
    ++result_.stats.backtracks;
    if (options_.trace != nullptr) options_.trace->record(SearchTrace::Kind::Undo, cell, digit);
  }

  /// Records the completed grid; returns true when the search should stop.
  bool foundSolution() {
    if (result_.solutions++ == 0) result_.solution = grid_;
    return result_.solutions >= options_.solutionLimit;
  }

  SearchResult finish() {
    if (aborted_) {
      result_.status = SolveStatus::LimitReached;
    } else {
      result_.status = result_.solutions > 0 ? SolveStatus::Solved : SolveStatus::Unsolvable;
    }
    return result_;
  }

  SearchResult invalid() {
    result_.status = SolveStatus::Invalid;
    return result_;
  }

  Grid grid_;
  Occupancy occupancy_;
  SearchOptions options_;
  SearchResult result_;
  bool aborted_ = false;
};

}  // namespace sudoku::detail
