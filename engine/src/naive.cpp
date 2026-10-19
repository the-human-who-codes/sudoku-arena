#include <vector>

#include "search_base.hpp"

namespace sudoku {
namespace {

class NaiveSearch : detail::SearchBase {
 public:
  using SearchBase::SearchBase;

  SearchResult run() {
    if (!occupancy_.load(grid_)) return invalid();
    for (int cell = 0; cell < kCells; ++cell) {
      if (grid_[cell] == 0) empty_.push_back(static_cast<std::uint8_t>(cell));
    }
    search(0);
    return finish();
  }

 private:
  // Returns true when the search should stop.
  bool search(std::size_t depth) {
    if (depth == empty_.size()) return foundSolution();
    const int cell = empty_[depth];
    for (int digit = 1; digit <= 9; ++digit) {
      if ((occupancy_.allowed(cell) & digitBit(digit)) == 0) continue;
      if (!place(cell, digit, SearchTrace::Kind::Guess)) return true;
      if (search(depth + 1)) return true;
      undo(cell);
    }
    return false;
  }

  std::vector<std::uint8_t> empty_;
};

}  // namespace

SearchResult solveNaive(const Grid& puzzle, const SearchOptions& options) {
  return NaiveSearch(puzzle, options).run();
}

}  // namespace sudoku
