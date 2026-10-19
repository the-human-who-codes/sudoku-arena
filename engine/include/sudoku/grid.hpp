#pragma once

#include <array>
#include <bit>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sudoku {

inline constexpr int kCells = 81;
inline constexpr int kUnits = 27;

/// A set of digits: bit (d - 1) is set when digit d is in the set.
using Mask = std::uint16_t;
inline constexpr Mask kAllDigits = 0x1FF;

/// 81 cells in reading order; 0 marks an empty cell.
using Grid = std::array<std::uint8_t, kCells>;

constexpr Mask digitBit(int digit) { return static_cast<Mask>(1u << (digit - 1)); }
constexpr int countDigits(Mask mask) { return std::popcount(mask); }
/// Smallest digit of a non-empty mask.
constexpr int firstDigit(Mask mask) { return std::countr_zero(mask) + 1; }

constexpr int rowOf(int cell) { return cell / 9; }
constexpr int colOf(int cell) { return cell % 9; }
constexpr int boxOf(int cell) { return (cell / 27) * 3 + (cell % 9) / 3; }

/// Units (rows, columns and boxes) are numbered 0-8, 9-17 and 18-26.
constexpr int rowUnit(int row) { return row; }
constexpr int colUnit(int col) { return 9 + col; }
constexpr int boxUnit(int box) { return 18 + box; }

/// True when two distinct cells share a row, column or box.
constexpr bool sees(int a, int b) {
  return a != b && (rowOf(a) == rowOf(b) || colOf(a) == colOf(b) || boxOf(a) == boxOf(b));
}

struct Topology {
  std::array<std::array<std::uint8_t, 9>, kUnits> unitCells{};
  std::array<std::array<std::uint8_t, 20>, kCells> peers{};
  std::array<std::array<std::uint8_t, 3>, kCells> cellUnits{};
};

constexpr Topology makeTopology() {
  Topology t{};
  for (int i = 0; i < 9; ++i) {
    for (int j = 0; j < 9; ++j) {
      t.unitCells[rowUnit(i)][j] = static_cast<std::uint8_t>(i * 9 + j);
      t.unitCells[colUnit(i)][j] = static_cast<std::uint8_t>(j * 9 + i);
      t.unitCells[boxUnit(i)][j] =
          static_cast<std::uint8_t>((i / 3) * 27 + (i % 3) * 3 + (j / 3) * 9 + j % 3);
    }
  }
  for (int cell = 0; cell < kCells; ++cell) {
    t.cellUnits[cell] = {static_cast<std::uint8_t>(rowUnit(rowOf(cell))),
                         static_cast<std::uint8_t>(colUnit(colOf(cell))),
                         static_cast<std::uint8_t>(boxUnit(boxOf(cell)))};
    int count = 0;
    for (int other = 0; other < kCells; ++other) {
      if (sees(cell, other)) t.peers[cell][count++] = static_cast<std::uint8_t>(other);
    }
  }
  return t;
}

inline constexpr Topology kTopology = makeTopology();

/// Parses 81 cells from text. Digits 1-9 are givens; '0', '.', '_' and '*' are
/// blanks; whitespace and the box-drawing characters "|+-=" are ignored.
std::optional<Grid> parseGrid(std::string_view text);

/// 81-character string, one character per cell.
std::string toString(const Grid& grid, char empty = '.');

/// Multi-line rendering with box separators.
std::string toPrettyString(const Grid& grid);

/// True when no row, column or box repeats a digit.
bool isConsistent(const Grid& grid);

/// Cells whose digit is repeated within one of their units.
std::vector<int> conflictingCells(const Grid& grid);

bool isFilled(const Grid& grid);
int clueCount(const Grid& grid);

/// True when `solution` is a complete valid grid that keeps every given of `puzzle`.
bool isSolutionOf(const Grid& solution, const Grid& puzzle);

/// "r3c5" style coordinates (1-based), the usual notation in Sudoku literature.
std::string cellName(int cell);

/// "row 3", "column 5" or "box 2".
std::string unitName(int unit);

}  // namespace sudoku
