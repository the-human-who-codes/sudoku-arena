#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "sudoku/grid.hpp"
#include "sudoku/search.hpp"

namespace sudoku {

/// Human solving techniques, ordered from easiest to hardest.
enum class Technique : std::uint8_t {
  FullHouse,
  HiddenSingle,
  NakedSingle,
  Pointing,
  Claiming,
  NakedPair,
  HiddenPair,
  XWing,
  NakedTriple,
  HiddenTriple,
  Swordfish,
  XYWing,
  XYZWing,
  NakedQuad,
  HiddenQuad,
  Guess,      ///< Trial and error once no technique applies.
  Backtrack,  ///< Undoing a guess that led to a contradiction.
};
inline constexpr int kTechniqueCount = 17;

/// Difficulty levels: Easy and Medium need only singles (Medium has fewer givens),
/// Hard adds locked candidates and pairs, Expert fish, triples and wings, and
/// Master puzzles defeat every technique here, so they need trial and error.
enum class Level : std::uint8_t { Easy = 1, Medium, Hard, Expert, Master };

/// Puzzles that need only singles but have fewer givens than this rate Medium.
inline constexpr int kEasyMinClues = 36;

struct TechniqueInfo {
  std::string_view id;    ///< Stable kebab-case identifier.
  std::string_view name;  ///< Display name.
  Level level;
};

const TechniqueInfo& techniqueInfo(Technique technique);

/// Level of a puzzle with `clues` givens whose hardest required technique is `hardest`.
Level levelOf(Technique hardest, int clues);

std::string_view levelName(Level level);
std::optional<Level> parseLevel(std::string_view name);

struct Candidate {
  std::uint8_t cell;
  std::uint8_t digit;
  friend bool operator==(const Candidate&, const Candidate&) = default;
};

struct CellState {
  std::uint8_t cell;
  std::uint8_t value;  ///< 0 while empty.
  Mask candidates;     ///< 0 once filled.
};

/// Pencil-mark state: placed digits plus the remaining candidates of empty cells.
class Board {
 public:
  /// Fills in every candidate allowed by the givens; nullopt when the givens conflict.
  static std::optional<Board> fromGrid(const Grid& grid);

  std::uint8_t value(int cell) const { return values_[cell]; }
  Mask candidates(int cell) const { return candidates_[cell]; }
  const Grid& values() const { return values_; }
  bool isSolved() const;

  /// Writes a digit and removes it from the candidates of every peer.
  void place(int cell, int digit);
  /// Removes one candidate; returns false if it was already gone.
  bool eliminate(int cell, int digit);

  /// Bit i is set when the i-th cell of `unit` is empty and allows `digit`.
  Mask positions(int unit, int digit) const;
  Mask placedDigits(int unit) const;

  /// True when an empty cell has no candidates or a unit has no room for a missing digit.
  bool hasContradiction() const;
  std::string describeContradiction() const;

  friend bool operator==(const Board&, const Board&) = default;

 private:
  Grid values_{};
  std::array<Mask, kCells> candidates_{};
};

/// One deduction: the digits it places, the candidates it removes and the
/// pattern (cells, units and digits) that justifies it.
struct LogicStep {
  Technique technique{};
  std::vector<Candidate> placements;
  std::vector<Candidate> eliminations;
  std::vector<std::uint8_t> cells;
  std::vector<std::uint8_t> units;
  Mask digits = 0;
  std::string explanation;          ///< Only filled when explaining.
  std::vector<CellState> changes;   ///< Cells the step changed; only filled when explaining.
};

/// The easiest applicable technique on `board`, or nullopt when none applies.
std::optional<LogicStep> findStep(const Board& board);

/// Plain-English justification of `step`, given the board it was found on.
std::string explain(const LogicStep& step, const Board& before);

/// Applies a step's placements, then its eliminations.
void apply(const LogicStep& step, Board& board);

struct LogicOptions {
  bool allowGuessing = true;  ///< Fall back to trial and error when stuck.
  bool explain = true;        ///< Record steps with explanations and cell changes.
  std::size_t stepLimit = 5000;
};

struct LogicResult {
  SolveStatus status = SolveStatus::Unsolvable;
  Grid grid{};  ///< Final grid: the solution when solved.
  std::vector<LogicStep> steps;
  std::array<int, kTechniqueCount> techniqueCounts{};
  Technique hardest = Technique::FullHouse;
  int guesses = 0;
  int backtracks = 0;
};

/// Solves like a person: always applies the easiest technique that makes progress.
LogicResult solveLogically(const Grid& puzzle, const LogicOptions& options = {});

struct Rating {
  Level level = Level::Easy;
  Technique hardest = Technique::FullHouse;
  std::array<int, kTechniqueCount> techniqueCounts{};
};

/// Grades a puzzle by the hardest technique it needs; Master when the techniques
/// run out. Nullopt for invalid or unsolvable puzzles.
std::optional<Rating> rate(const Grid& puzzle);

}  // namespace sudoku
