#include <algorithm>
#include <array>
#include <string>
#include <vector>

#include "sudoku/logic.hpp"

namespace sudoku {
namespace {

using Cells = std::vector<std::uint8_t>;

constexpr std::uint8_t u8(int value) { return static_cast<std::uint8_t>(value); }

// Units in the order people usually scan them: boxes, then rows, then columns.
constexpr std::array<std::uint8_t, kUnits> kScanOrder = {18, 19, 20, 21, 22, 23, 24, 25, 26,
                                                         0,  1,  2,  3,  4,  5,  6,  7,  8,
                                                         9,  10, 11, 12, 13, 14, 15, 16, 17};

constexpr std::array<TechniqueInfo, kTechniqueCount> kTechniques = {{
    {"full-house", "Full House", Level::Easy},
    {"hidden-single", "Hidden Single", Level::Easy},
    {"naked-single", "Naked Single", Level::Easy},
    {"pointing", "Pointing", Level::Hard},
    {"claiming", "Claiming", Level::Hard},
    {"naked-pair", "Naked Pair", Level::Hard},
    {"hidden-pair", "Hidden Pair", Level::Hard},
    {"x-wing", "X-Wing", Level::Expert},
    {"naked-triple", "Naked Triple", Level::Expert},
    {"hidden-triple", "Hidden Triple", Level::Expert},
    {"swordfish", "Swordfish", Level::Expert},
    {"xy-wing", "XY-Wing", Level::Expert},
    {"xyz-wing", "XYZ-Wing", Level::Expert},
    {"naked-quad", "Naked Quad", Level::Expert},
    {"hidden-quad", "Hidden Quad", Level::Expert},
    {"guess", "Guess", Level::Master},
    {"backtrack", "Backtrack", Level::Master},
}};

constexpr std::array<std::string_view, 5> kLevelNames = {"easy", "medium", "hard", "expert",
                                                         "master"};

bool contains(const Cells& cells, int cell) {
  return std::find(cells.begin(), cells.end(), cell) != cells.end();
}

bool inUnit(int cell, int unit) {
  const auto& units = kTopology.cellUnits[cell];
  return units[0] == unit || units[1] == unit || units[2] == unit;
}

LogicStep makeStep(Technique technique) {
  LogicStep step;
  step.technique = technique;
  return step;
}

// Calls visit(subset) for each `size`-element subset of {0, ..., n - 1}, given as
// a bitmask, until visit returns true. Returns whether it did.
template <typename Visit>
bool forEachSubset(int n, int size, Visit&& visit) {
  if (size <= 0 || size > n) return false;
  const unsigned end = 1u << n;
  for (unsigned subset = (1u << size) - 1; subset < end;) {
    if (visit(subset)) return true;
    const unsigned low = subset & (~subset + 1);  // Gosper's hack: next subset of equal size.
    const unsigned ripple = subset + low;
    subset = (((ripple ^ subset) >> 2) / low) | ripple;
  }
  return false;
}

// --- Singles ----------------------------------------------------------------

std::optional<LogicStep> findFullHouse(const Board& board) {
  for (const int unit : kScanOrder) {
    int empty = -1;
    int emptyCount = 0;
    for (const int cell : kTopology.unitCells[unit]) {
      if (board.value(cell) == 0) {
        empty = cell;
        ++emptyCount;
      }
    }
    if (emptyCount != 1 || countDigits(board.candidates(empty)) != 1) continue;
    LogicStep step = makeStep(Technique::FullHouse);
    step.placements.push_back({u8(empty), u8(firstDigit(board.candidates(empty)))});
    step.cells = {u8(empty)};
    step.units = {u8(unit)};
    step.digits = board.candidates(empty);
    return step;
  }
  return std::nullopt;
}

std::optional<LogicStep> findHiddenSingle(const Board& board) {
  for (const int unit : kScanOrder) {
    for (int digit = 1; digit <= 9; ++digit) {
      const Mask where = board.positions(unit, digit);
      if (countDigits(where) != 1) continue;
      const int cell = kTopology.unitCells[unit][std::countr_zero(where)];
      LogicStep step = makeStep(Technique::HiddenSingle);
      step.placements.push_back({u8(cell), u8(digit)});
      step.cells = {u8(cell)};
      step.units = {u8(unit)};
      step.digits = digitBit(digit);
      return step;
    }
  }
  return std::nullopt;
}

std::optional<LogicStep> findNakedSingle(const Board& board) {
  for (int cell = 0; cell < kCells; ++cell) {
    const Mask candidates = board.candidates(cell);
    if (countDigits(candidates) != 1) continue;
    LogicStep step = makeStep(Technique::NakedSingle);
    step.placements.push_back({u8(cell), u8(firstDigit(candidates))});
    step.cells = {u8(cell)};
    const auto& units = kTopology.cellUnits[cell];
    step.units.assign(units.begin(), units.end());
    step.digits = candidates;
    return step;
  }
  return std::nullopt;
}

// --- Locked candidates --------------------------------------------------------

Cells cellsAt(int unit, Mask where) {
  Cells cells;
  for (Mask m = where; m != 0; m &= m - 1) {
    cells.push_back(kTopology.unitCells[unit][std::countr_zero(m)]);
  }
  return cells;
}

// A digit confined to one row or column of a box can't appear elsewhere in that line.
std::optional<LogicStep> findPointing(const Board& board) {
  for (int box = 0; box < 9; ++box) {
    const int boxU = boxUnit(box);
    for (int digit = 1; digit <= 9; ++digit) {
      const Mask where = board.positions(boxU, digit);
      if (countDigits(where) < 2) continue;
      const Cells cells = cellsAt(boxU, where);
      unsigned rows = 0;
      unsigned cols = 0;
      for (const int cell : cells) {
        rows |= 1u << rowOf(cell);
        cols |= 1u << colOf(cell);
      }
      const std::array<int, 2> lines = {
          std::popcount(rows) == 1 ? rowUnit(std::countr_zero(rows)) : -1,
          std::popcount(cols) == 1 ? colUnit(std::countr_zero(cols)) : -1};
      for (const int line : lines) {
        if (line < 0) continue;
        LogicStep step = makeStep(Technique::Pointing);
        for (const int cell : kTopology.unitCells[line]) {
          if (boxOf(cell) != box && (board.candidates(cell) & digitBit(digit)) != 0) {
            step.eliminations.push_back({u8(cell), u8(digit)});
          }
        }
        if (step.eliminations.empty()) continue;
        step.cells = cells;
        step.units = {u8(boxU), u8(line)};
        step.digits = digitBit(digit);
        return step;
      }
    }
  }
  return std::nullopt;
}

// A digit confined to one box within a row or column can't appear elsewhere in that box.
std::optional<LogicStep> findClaiming(const Board& board) {
  for (int line = 0; line < 18; ++line) {
    for (int digit = 1; digit <= 9; ++digit) {
      const Mask where = board.positions(line, digit);
      if (countDigits(where) < 2) continue;
      const Cells cells = cellsAt(line, where);
      unsigned boxes = 0;
      for (const int cell : cells) boxes |= 1u << boxOf(cell);
      if (std::popcount(boxes) != 1) continue;
      const int boxU = boxUnit(std::countr_zero(boxes));
      LogicStep step = makeStep(Technique::Claiming);
      for (const int cell : kTopology.unitCells[boxU]) {
        if (!inUnit(cell, line) && (board.candidates(cell) & digitBit(digit)) != 0) {
          step.eliminations.push_back({u8(cell), u8(digit)});
        }
      }
      if (step.eliminations.empty()) continue;
      step.cells = cells;
      step.units = {u8(line), u8(boxU)};
      step.digits = digitBit(digit);
      return step;
    }
  }
  return std::nullopt;
}

// --- Subsets --------------------------------------------------------------------

// N cells of a unit whose candidates together are N digits: those digits can't go
// anywhere else in the unit.
std::optional<LogicStep> findNakedSubset(const Board& board, int size, Technique technique) {
  for (const int unit : kScanOrder) {
    std::array<std::uint8_t, 9> pool{};
    int poolSize = 0;
    int empty = 0;
    for (const int cell : kTopology.unitCells[unit]) {
      if (board.value(cell) != 0) continue;
      ++empty;
      const int count = countDigits(board.candidates(cell));
      if (count >= 2 && count <= size) pool[poolSize++] = u8(cell);
    }
    if (empty <= size) continue;

    std::optional<LogicStep> found;
    forEachSubset(poolSize, size, [&](unsigned subset) {
      Mask digits = 0;
      Cells cells;
      for (unsigned s = subset; s != 0; s &= s - 1) {
        cells.push_back(pool[std::countr_zero(s)]);
        digits |= board.candidates(cells.back());
      }
      if (countDigits(digits) != size) return false;
      LogicStep step = makeStep(technique);
      for (const int cell : kTopology.unitCells[unit]) {
        if (board.value(cell) != 0 || contains(cells, cell)) continue;
        for (Mask m = board.candidates(cell) & digits; m != 0; m &= m - 1) {
          step.eliminations.push_back({u8(cell), u8(firstDigit(m))});
        }
      }
      if (step.eliminations.empty()) return false;
      step.cells = std::move(cells);
      step.units = {u8(unit)};
      step.digits = digits;
      found = std::move(step);
      return true;
    });
    if (found) return found;
  }
  return std::nullopt;
}

// N digits of a unit that fit in only N cells: those cells can't hold anything else.
std::optional<LogicStep> findHiddenSubset(const Board& board, int size, Technique technique) {
  for (const int unit : kScanOrder) {
    int empty = 0;
    for (const int cell : kTopology.unitCells[unit]) empty += board.value(cell) == 0 ? 1 : 0;
    if (empty <= size) continue;

    std::array<int, 9> pool{};
    std::array<Mask, 9> where{};
    int poolSize = 0;
    for (int digit = 1; digit <= 9; ++digit) {
      const Mask positions = board.positions(unit, digit);
      const int count = countDigits(positions);
      if (count >= 2 && count <= size) {
        pool[poolSize] = digit;
        where[poolSize] = positions;
        ++poolSize;
      }
    }

    std::optional<LogicStep> found;
    forEachSubset(poolSize, size, [&](unsigned subset) {
      Mask positions = 0;
      Mask digits = 0;
      for (unsigned s = subset; s != 0; s &= s - 1) {
        const int i = std::countr_zero(s);
        positions |= where[i];
        digits |= digitBit(pool[i]);
      }
      if (countDigits(positions) != size) return false;
      LogicStep step = makeStep(technique);
      step.cells = cellsAt(unit, positions);
      for (const int cell : step.cells) {
        for (Mask m = board.candidates(cell) & ~digits; m != 0; m &= m - 1) {
          step.eliminations.push_back({u8(cell), u8(firstDigit(m))});
        }
      }
      if (step.eliminations.empty()) return false;
      step.units = {u8(unit)};
      step.digits = digits;
      found = std::move(step);
      return true;
    });
    if (found) return found;
  }
  return std::nullopt;
}

// --- Fish -----------------------------------------------------------------------

// N rows (or columns) whose candidates for a digit all lie in the same N columns
// (or rows): the digit can't appear elsewhere in those columns.
std::optional<LogicStep> findFish(const Board& board, int size, Technique technique) {
  for (int digit = 1; digit <= 9; ++digit) {
    for (const bool rowBased : {true, false}) {
      std::array<int, 9> pool{};
      std::array<Mask, 9> cover{};
      int poolSize = 0;
      for (int line = 0; line < 9; ++line) {
        const Mask where = board.positions(rowBased ? rowUnit(line) : colUnit(line), digit);
        const int count = countDigits(where);
        if (count >= 2 && count <= size) {
          pool[poolSize] = line;
          cover[poolSize] = where;
          ++poolSize;
        }
      }

      std::optional<LogicStep> found;
      forEachSubset(poolSize, size, [&](unsigned subset) {
        Mask covered = 0;
        unsigned baseLines = 0;
        for (unsigned s = subset; s != 0; s &= s - 1) {
          const int i = std::countr_zero(s);
          covered |= cover[i];
          baseLines |= 1u << pool[i];
        }
        if (countDigits(covered) != size) return false;

        LogicStep step = makeStep(technique);
        for (Mask m = covered; m != 0; m &= m - 1) {
          const int crossUnit = rowBased ? colUnit(std::countr_zero(m)) : rowUnit(std::countr_zero(m));
          for (int j = 0; j < 9; ++j) {
            const int cell = kTopology.unitCells[crossUnit][j];
            if ((baseLines & (1u << j)) == 0 && (board.candidates(cell) & digitBit(digit)) != 0) {
              step.eliminations.push_back({u8(cell), u8(digit)});
            }
          }
        }
        if (step.eliminations.empty()) return false;

        for (unsigned s = subset; s != 0; s &= s - 1) {
          const int i = std::countr_zero(s);
          for (Mask m = cover[i]; m != 0; m &= m - 1) {
            const int cross = std::countr_zero(m);
            step.cells.push_back(u8(rowBased ? pool[i] * 9 + cross : cross * 9 + pool[i]));
          }
          step.units.push_back(u8(rowBased ? rowUnit(pool[i]) : colUnit(pool[i])));
        }
        for (Mask m = covered; m != 0; m &= m - 1) {
          const int cross = std::countr_zero(m);
          step.units.push_back(u8(rowBased ? colUnit(cross) : rowUnit(cross)));
        }
        step.digits = digitBit(digit);
        found = std::move(step);
        return true;
      });
      if (found) return found;
    }
  }
  return std::nullopt;
}

// --- Wings ----------------------------------------------------------------------

// Pivot {x,y} sees wings {x,z} and {y,z}: one wing is z whatever the pivot holds,
// so z can go in no cell that sees both wings.
std::optional<LogicStep> findXYWing(const Board& board) {
  for (int pivot = 0; pivot < kCells; ++pivot) {
    const Mask pivotMask = board.candidates(pivot);
    if (countDigits(pivotMask) != 2) continue;
    for (const int a : kTopology.peers[pivot]) {
      const Mask aMask = board.candidates(a);
      if (countDigits(aMask) != 2 || countDigits(aMask & pivotMask) != 1) continue;
      const auto z = static_cast<Mask>(aMask & ~pivotMask);
      const auto bMask = static_cast<Mask>((pivotMask & ~aMask) | z);
      for (const int b : kTopology.peers[pivot]) {
        if (board.candidates(b) != bMask) continue;
        LogicStep step = makeStep(Technique::XYWing);
        for (const int cell : kTopology.peers[a]) {
          if (cell != pivot && cell != b && sees(cell, b) && (board.candidates(cell) & z) != 0) {
            step.eliminations.push_back({u8(cell), u8(firstDigit(z))});
          }
        }
        if (step.eliminations.empty()) continue;
        step.cells = {u8(pivot), u8(a), u8(b)};
        step.digits = static_cast<Mask>(pivotMask | z);
        return step;
      }
    }
  }
  return std::nullopt;
}

// Pivot {x,y,z} sees wings {x,z} and {y,z}: one of the three is z, so z can go in
// no cell that sees all of them.
std::optional<LogicStep> findXYZWing(const Board& board) {
  for (int pivot = 0; pivot < kCells; ++pivot) {
    const Mask pivotMask = board.candidates(pivot);
    if (countDigits(pivotMask) != 3) continue;
    for (const int a : kTopology.peers[pivot]) {
      const Mask aMask = board.candidates(a);
      if (countDigits(aMask) != 2 || (aMask & ~pivotMask) != 0) continue;
      for (const int b : kTopology.peers[pivot]) {
        const Mask bMask = board.candidates(b);
        if (b <= a || countDigits(bMask) != 2 || (bMask & ~pivotMask) != 0 || bMask == aMask) {
          continue;
        }
        const auto z = static_cast<Mask>(aMask & bMask);
        LogicStep step = makeStep(Technique::XYZWing);
        for (const int cell : kTopology.peers[pivot]) {
          if (cell != a && cell != b && sees(cell, a) && sees(cell, b) &&
              (board.candidates(cell) & z) != 0) {
            step.eliminations.push_back({u8(cell), u8(firstDigit(z))});
          }
        }
        if (step.eliminations.empty()) continue;
        step.cells = {u8(pivot), u8(a), u8(b)};
        step.digits = pivotMask;
        return step;
      }
    }
  }
  return std::nullopt;
}

// --- Explanations ------------------------------------------------------------------

std::string joinList(const std::vector<std::string>& items) {
  std::string out;
  for (std::size_t i = 0; i < items.size(); ++i) {
    if (i > 0) out += i + 1 == items.size() ? " and " : ", ";
    out += items[i];
  }
  return out;
}

// "4 and 8"
std::string digitsText(Mask digits) {
  std::vector<std::string> items;
  for (Mask m = digits; m != 0; m &= m - 1) items.push_back(std::to_string(firstDigit(m)));
  return joinList(items);
}

// "{4,8}"
std::string candidatesText(Mask digits) {
  std::string out = "{";
  for (Mask m = digits; m != 0; m &= m - 1) {
    if (out.size() > 1) out += ',';
    out += static_cast<char>('0' + firstDigit(m));
  }
  return out + "}";
}

std::string cellsText(const Cells& cells) {
  constexpr std::size_t kShown = 6;
  std::vector<std::string> items;
  for (std::size_t i = 0; i < cells.size() && i < kShown; ++i) items.push_back(cellName(cells[i]));
  if (cells.size() > kShown) items.push_back(std::to_string(cells.size() - kShown) + " more");
  return joinList(items);
}

// "rows 2 and 7" or "columns 3 and 8"
std::string linesText(const Cells& units) {
  std::vector<std::string> numbers;
  for (const int unit : units) numbers.push_back(std::to_string(unit % 9 + 1));
  return (units.front() < 9 ? "rows " : "columns ") + joinList(numbers);
}

Cells eliminatedCells(const LogicStep& step) {
  Cells cells;
  for (const auto& elimination : step.eliminations) {
    if (!contains(cells, elimination.cell)) cells.push_back(elimination.cell);
  }
  return cells;
}

std::string withCandidates(int cell, const Board& board) {
  return cellName(cell) + " " + candidatesText(board.candidates(cell));
}

}  // namespace

const TechniqueInfo& techniqueInfo(Technique technique) {
  return kTechniques[static_cast<std::size_t>(technique)];
}

Level levelOf(Technique hardest, int clues) {
  const Level level = techniqueInfo(hardest).level;
  return level == Level::Easy && clues < kEasyMinClues ? Level::Medium : level;
}

std::string_view levelName(Level level) {
  return kLevelNames[static_cast<std::size_t>(level) - 1];
}

std::optional<Level> parseLevel(std::string_view name) {
  for (std::size_t i = 0; i < kLevelNames.size(); ++i) {
    if (kLevelNames[i] == name) return static_cast<Level>(i + 1);
  }
  return std::nullopt;
}

std::optional<LogicStep> findStep(const Board& board) {
  if (auto step = findFullHouse(board)) return step;
  if (auto step = findHiddenSingle(board)) return step;
  if (auto step = findNakedSingle(board)) return step;
  if (auto step = findPointing(board)) return step;
  if (auto step = findClaiming(board)) return step;
  if (auto step = findNakedSubset(board, 2, Technique::NakedPair)) return step;
  if (auto step = findHiddenSubset(board, 2, Technique::HiddenPair)) return step;
  if (auto step = findFish(board, 2, Technique::XWing)) return step;
  if (auto step = findNakedSubset(board, 3, Technique::NakedTriple)) return step;
  if (auto step = findHiddenSubset(board, 3, Technique::HiddenTriple)) return step;
  if (auto step = findFish(board, 3, Technique::Swordfish)) return step;
  if (auto step = findXYWing(board)) return step;
  if (auto step = findXYZWing(board)) return step;
  if (auto step = findNakedSubset(board, 4, Technique::NakedQuad)) return step;
  if (auto step = findHiddenSubset(board, 4, Technique::HiddenQuad)) return step;
  return std::nullopt;
}

void apply(const LogicStep& step, Board& board) {
  for (const auto& placement : step.placements) board.place(placement.cell, placement.digit);
  for (const auto& elimination : step.eliminations) {
    board.eliminate(elimination.cell, elimination.digit);
  }
}

std::string explain(const LogicStep& step, const Board& before) {
  const std::string name(techniqueInfo(step.technique).name);
  const std::string removed = cellsText(eliminatedCells(step));
  const std::string digits = digitsText(step.digits);

  switch (step.technique) {
    case Technique::FullHouse: {
      const auto& p = step.placements.front();
      return "Full House: " + cellName(p.cell) + " is the last empty cell in " +
             unitName(step.units.front()) + ", so it must be " + std::to_string(p.digit) + ".";
    }
    case Technique::HiddenSingle: {
      const auto& p = step.placements.front();
      return "Hidden Single: within " + unitName(step.units.front()) + ", " +
             std::to_string(p.digit) + " fits only in " + cellName(p.cell) + ".";
    }
    case Technique::NakedSingle: {
      const auto& p = step.placements.front();
      return "Naked Single: every other digit already appears in the row, column or box of " +
             cellName(p.cell) + ", leaving only " + std::to_string(p.digit) + ".";
    }
    case Technique::Pointing:
      return "Pointing: in " + unitName(step.units[0]) + ", every " + digits + " lies in " +
             unitName(step.units[1]) + ", so " + digits + " can be removed from the rest of " +
             unitName(step.units[1]) + " (" + removed + ").";
    case Technique::Claiming:
      return "Claiming: in " + unitName(step.units[0]) + ", every " + digits + " lies inside " +
             unitName(step.units[1]) + ", so " + digits + " can be removed from the rest of " +
             unitName(step.units[1]) + " (" + removed + ").";
    case Technique::NakedPair:
    case Technique::NakedTriple:
    case Technique::NakedQuad:
      return name + ": " + cellsText(step.cells) + " can only hold " + digits +
             ", so those digits are removed from the rest of " + unitName(step.units.front()) +
             " (" + removed + ").";
    case Technique::HiddenPair:
    case Technique::HiddenTriple:
    case Technique::HiddenQuad:
      return name + ": in " + unitName(step.units.front()) + ", " + digits + " fit only in " +
             cellsText(step.cells) + ", so every other candidate is removed from those cells.";
    case Technique::XWing:
    case Technique::Swordfish: {
      const auto half = static_cast<std::ptrdiff_t>(step.units.size() / 2);
      const std::string base = linesText(Cells(step.units.begin(), step.units.begin() + half));
      const std::string cover = linesText(Cells(step.units.begin() + half, step.units.end()));
      return name + " on " + digits + ": in " + base + ", " + digits + " can only go in " + cover +
             ". Those lines take every " + digits + " of " + cover + ", so " + digits +
             " is removed from the rest of them (" + removed + ").";
    }
    case Technique::XYWing: {
      const std::string z = std::to_string(step.eliminations.front().digit);
      return "XY-Wing: pivot " + withCandidates(step.cells[0], before) + " sees " +
             withCandidates(step.cells[1], before) + " and " +
             withCandidates(step.cells[2], before) +
             ". Whichever digit the pivot takes, one of those two cells becomes " + z + ", so " +
             z + " is removed from every cell that sees both (" + removed + ").";
    }
    case Technique::XYZWing: {
      const std::string z = std::to_string(step.eliminations.front().digit);
      return "XYZ-Wing: pivot " + withCandidates(step.cells[0], before) + " sees " +
             withCandidates(step.cells[1], before) + " and " +
             withCandidates(step.cells[2], before) + ". One of the three must be " + z + ", so " +
             z + " is removed from every cell that sees all three (" + removed + ").";
    }
    case Technique::Guess: {
      const auto& p = step.placements.front();
      return "No technique applies, so we guess " + std::to_string(p.digit) + " for " +
             withCandidates(p.cell, before) +
             ". If that leads to a contradiction, we will come back and rule it out.";
    }
    case Technique::Backtrack:
      break;  // The solver writes these, since only it knows the contradiction.
  }
  return step.explanation;
}

}  // namespace sudoku
