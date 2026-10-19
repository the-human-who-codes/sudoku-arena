#include <array>
#include <memory>
#include <vector>

#include "search_base.hpp"

namespace sudoku {
namespace {

// Exact cover formulation: each of the 729 rows places one digit in one cell and
// covers four of the 324 columns (cell filled, digit in row, digit in column,
// digit in box). A solution picks rows that cover every column exactly once.
constexpr int kColumns = 324;
constexpr int kRows = 729;
constexpr int kRoot = 0;
constexpr int kNodes = 1 + kColumns + kRows * 4;

class DlxSearch : detail::SearchBase {
 public:
  using SearchBase::SearchBase;

  SearchResult run() {
    if (!occupancy_.load(grid_)) return invalid();
    build();
    for (int cell = 0; cell < kCells; ++cell) {
      if (grid_[cell] != 0) select(rowStart_[cell * 9 + grid_[cell] - 1]);
    }
    search();
    return finish();
  }

 private:
  void build() {
    left_.resize(kNodes);
    right_.resize(kNodes);
    up_.resize(kNodes);
    down_.resize(kNodes);
    column_.resize(kNodes);
    row_.resize(kNodes);
    rowStart_.resize(kRows);

    for (int c = 0; c <= kColumns; ++c) {
      left_[c] = c - 1;
      right_[c] = c + 1;
      up_[c] = down_[c] = column_[c] = c;
      size_[c] = 0;
    }
    left_[kRoot] = kColumns;
    right_[kColumns] = kRoot;

    int next = kColumns + 1;
    for (int cell = 0; cell < kCells; ++cell) {
      for (int d = 0; d < 9; ++d) {
        const int row = cell * 9 + d;
        const std::array<int, 4> columns = {1 + cell, 1 + 81 + rowOf(cell) * 9 + d,
                                            1 + 162 + colOf(cell) * 9 + d,
                                            1 + 243 + boxOf(cell) * 9 + d};
        const int first = next;
        rowStart_[row] = first;
        for (int k = 0; k < 4; ++k) {
          const int node = next++;
          const int c = columns[k];
          column_[node] = c;
          row_[node] = row;
          up_[node] = up_[c];
          down_[node] = c;
          down_[up_[c]] = node;
          up_[c] = node;
          ++size_[c];
          left_[node] = k == 0 ? node : node - 1;
          right_[node] = first;
          right_[left_[node]] = node;
          left_[first] = node;
        }
      }
    }
  }

  void cover(int c) {
    right_[left_[c]] = right_[c];
    left_[right_[c]] = left_[c];
    for (int i = down_[c]; i != c; i = down_[i]) {
      for (int j = right_[i]; j != i; j = right_[j]) {
        down_[up_[j]] = down_[j];
        up_[down_[j]] = up_[j];
        --size_[column_[j]];
      }
    }
  }

  void uncover(int c) {
    for (int i = up_[c]; i != c; i = up_[i]) {
      for (int j = left_[i]; j != i; j = left_[j]) {
        ++size_[column_[j]];
        down_[up_[j]] = j;
        up_[down_[j]] = j;
      }
    }
    right_[left_[c]] = c;
    left_[right_[c]] = c;
  }

  // Commits a given: covers every column of its row.
  void select(int node) {
    cover(column_[node]);
    for (int j = right_[node]; j != node; j = right_[j]) cover(column_[j]);
  }

  // Returns true when the search should stop.
  bool search() {
    if (right_[kRoot] == kRoot) return foundSolution();

    int best = right_[kRoot];
    for (int c = right_[best]; c != kRoot && size_[best] > 1; c = right_[c]) {
      if (size_[c] < size_[best]) best = c;
    }
    if (size_[best] == 0) return false;

    const auto kind = size_[best] == 1 ? SearchTrace::Kind::Forced : SearchTrace::Kind::Guess;
    cover(best);
    for (int r = down_[best]; r != best; r = down_[r]) {
      const int cell = row_[r] / 9;
      if (!place(cell, row_[r] % 9 + 1, kind)) return true;
      for (int j = right_[r]; j != r; j = right_[j]) cover(column_[j]);
      if (search()) return true;
      for (int j = left_[r]; j != r; j = left_[j]) uncover(column_[j]);
      undo(cell);
    }
    uncover(best);
    return false;
  }

  std::vector<int> left_, right_, up_, down_, column_, row_, rowStart_;
  std::array<int, kColumns + 1> size_{};
};

}  // namespace

SearchResult solveDlx(const Grid& puzzle, const SearchOptions& options) {
  // The link arrays are large, so keep the solver off the (small, in WebAssembly) stack.
  auto search = std::make_unique<DlxSearch>(puzzle, options);
  return search->run();
}

}  // namespace sudoku
