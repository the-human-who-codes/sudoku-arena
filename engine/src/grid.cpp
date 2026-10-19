#include "sudoku/grid.hpp"

namespace sudoku {

std::optional<Grid> parseGrid(std::string_view text) {
  Grid grid{};
  int count = 0;
  for (const char ch : text) {
    int value = 0;
    if (ch >= '1' && ch <= '9') {
      value = ch - '0';
    } else if (ch == '0' || ch == '.' || ch == '_' || ch == '*') {
      value = 0;
    } else if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '|' || ch == '+' ||
               ch == '-' || ch == '=') {
      continue;
    } else {
      return std::nullopt;
    }
    if (count == kCells) return std::nullopt;
    grid[count++] = static_cast<std::uint8_t>(value);
  }
  if (count != kCells) return std::nullopt;
  return grid;
}

std::string toString(const Grid& grid, char empty) {
  std::string text(kCells, empty);
  for (int cell = 0; cell < kCells; ++cell) {
    if (grid[cell] != 0) text[cell] = static_cast<char>('0' + grid[cell]);
  }
  return text;
}

std::string toPrettyString(const Grid& grid) {
  std::string out;
  for (int row = 0; row < 9; ++row) {
    if (row == 3 || row == 6) out += "------+-------+------\n";
    for (int col = 0; col < 9; ++col) {
      if (col == 3 || col == 6) out += "| ";
      const int value = grid[row * 9 + col];
      out += value == 0 ? '.' : static_cast<char>('0' + value);
      if (col != 8) out += ' ';
    }
    out += '\n';
  }
  return out;
}

bool isConsistent(const Grid& grid) {
  for (const auto& unit : kTopology.unitCells) {
    Mask seen = 0;
    for (const int cell : unit) {
      const int value = grid[cell];
      if (value == 0) continue;
      if (value > 9 || (seen & digitBit(value)) != 0) return false;
      seen |= digitBit(value);
    }
  }
  return true;
}

std::vector<int> conflictingCells(const Grid& grid) {
  std::array<bool, kCells> conflict{};
  for (const auto& unit : kTopology.unitCells) {
    for (int i = 0; i < 9; ++i) {
      for (int j = i + 1; j < 9; ++j) {
        if (grid[unit[i]] != 0 && grid[unit[i]] == grid[unit[j]]) {
          conflict[unit[i]] = conflict[unit[j]] = true;
        }
      }
    }
  }
  std::vector<int> cells;
  for (int cell = 0; cell < kCells; ++cell) {
    if (conflict[cell]) cells.push_back(cell);
  }
  return cells;
}

bool isFilled(const Grid& grid) {
  for (const auto value : grid) {
    if (value == 0) return false;
  }
  return true;
}

int clueCount(const Grid& grid) {
  int count = 0;
  for (const auto value : grid) count += value != 0 ? 1 : 0;
  return count;
}

bool isSolutionOf(const Grid& solution, const Grid& puzzle) {
  if (!isFilled(solution) || !isConsistent(solution)) return false;
  for (int cell = 0; cell < kCells; ++cell) {
    if (puzzle[cell] != 0 && puzzle[cell] != solution[cell]) return false;
  }
  return true;
}

std::string cellName(int cell) {
  return "r" + std::to_string(rowOf(cell) + 1) + "c" + std::to_string(colOf(cell) + 1);
}

std::string unitName(int unit) {
  static constexpr const char* kKinds[] = {"row ", "column ", "box "};
  return kKinds[unit / 9] + std::to_string(unit % 9 + 1);
}

}  // namespace sudoku
