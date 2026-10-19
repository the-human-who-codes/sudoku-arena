#include "sudoku/logic.hpp"

namespace sudoku {

std::optional<Board> Board::fromGrid(const Grid& grid) {
  if (!isConsistent(grid)) return std::nullopt;
  Board board;
  board.values_ = grid;
  for (int cell = 0; cell < kCells; ++cell) {
    if (grid[cell] != 0) continue;
    Mask seen = 0;
    for (const int peer : kTopology.peers[cell]) {
      if (grid[peer] != 0) seen |= digitBit(grid[peer]);
    }
    board.candidates_[cell] = static_cast<Mask>(kAllDigits & ~seen);
  }
  return board;
}

bool Board::isSolved() const { return isFilled(values_); }

void Board::place(int cell, int digit) {
  values_[cell] = static_cast<std::uint8_t>(digit);
  candidates_[cell] = 0;
  const auto keep = static_cast<Mask>(~digitBit(digit));
  for (const int peer : kTopology.peers[cell]) candidates_[peer] &= keep;
}

bool Board::eliminate(int cell, int digit) {
  const Mask bit = digitBit(digit);
  if ((candidates_[cell] & bit) == 0) return false;
  candidates_[cell] &= static_cast<Mask>(~bit);
  return true;
}

Mask Board::positions(int unit, int digit) const {
  const Mask bit = digitBit(digit);
  Mask where = 0;
  for (int i = 0; i < 9; ++i) {
    if ((candidates_[kTopology.unitCells[unit][i]] & bit) != 0) where |= static_cast<Mask>(1u << i);
  }
  return where;
}

Mask Board::placedDigits(int unit) const {
  Mask placed = 0;
  for (const int cell : kTopology.unitCells[unit]) {
    if (values_[cell] != 0) placed |= digitBit(values_[cell]);
  }
  return placed;
}

bool Board::hasContradiction() const {
  for (int cell = 0; cell < kCells; ++cell) {
    if (values_[cell] == 0 && candidates_[cell] == 0) return true;
  }
  for (int unit = 0; unit < kUnits; ++unit) {
    Mask covered = 0;
    for (const int cell : kTopology.unitCells[unit]) {
      covered |= values_[cell] != 0 ? digitBit(values_[cell]) : candidates_[cell];
    }
    if (covered != kAllDigits) return true;
  }
  return false;
}

std::string Board::describeContradiction() const {
  for (int cell = 0; cell < kCells; ++cell) {
    if (values_[cell] == 0 && candidates_[cell] == 0) {
      return cellName(cell) + " has no candidates left";
    }
  }
  for (int unit = 0; unit < kUnits; ++unit) {
    Mask covered = 0;
    for (const int cell : kTopology.unitCells[unit]) {
      covered |= values_[cell] != 0 ? digitBit(values_[cell]) : candidates_[cell];
    }
    if (covered != kAllDigits) {
      const int digit = firstDigit(static_cast<Mask>(kAllDigits & ~covered));
      return std::to_string(digit) + " has no place left in " + unitName(unit);
    }
  }
  return "no contradiction";
}

}  // namespace sudoku
