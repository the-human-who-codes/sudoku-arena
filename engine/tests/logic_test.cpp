#include <array>

#include "helpers.hpp"
#include "sudoku/generator.hpp"
#include "sudoku/logic.hpp"

namespace sudoku {
namespace {

using testing::grid;
using testing::sample;

TEST(Board, StartsWithEveryCandidateTheGivensAllow) {
  const auto board = Board::fromGrid(sample("Classic"));
  ASSERT_TRUE(board);
  // r1c3: its row has 3, 5, 7, its column 8 and its box 3, 5, 6, 8, 9.
  EXPECT_EQ(board->candidates(2), digitBit(1) | digitBit(2) | digitBit(4));
  EXPECT_EQ(board->candidates(0), 0);
  EXPECT_FALSE(board->hasContradiction());
}

TEST(Board, PlacingRemovesTheDigitFromPeers) {
  auto board = Board::fromGrid(Grid{});
  ASSERT_TRUE(board);
  board->place(40, 7);
  EXPECT_EQ(board->value(40), 7);
  for (const int peer : kTopology.peers[40]) {
    EXPECT_EQ(board->candidates(peer) & digitBit(7), 0) << cellName(peer);
  }
  EXPECT_EQ(board->candidates(0), kAllDigits);
}

TEST(Board, RejectsRepeatedGivens) {
  Grid g{};
  g[0] = g[80] = 4;  // no shared unit: fine
  EXPECT_TRUE(Board::fromGrid(g));
  g[1] = 4;
  EXPECT_FALSE(Board::fromGrid(g));
}

TEST(Logic, SolvesTheClassicWithSinglesAlone) {
  const LogicResult result = solveLogically(sample("Classic"));
  ASSERT_EQ(result.status, SolveStatus::Solved);
  EXPECT_EQ(toString(result.grid), testing::kClassicSolution);
  EXPECT_EQ(result.guesses, 0);
  EXPECT_EQ(techniqueInfo(result.hardest).level, Level::Easy);
  for (const auto& step : result.steps) {
    EXPECT_FALSE(step.explanation.empty());
    EXPECT_FALSE(step.changes.empty());
  }
}

TEST(Logic, FallsBackToGuessingOnTheHardestPuzzles) {
  for (const auto* name : {"AI Escargot", "Inkala 2012", "Easter Monster"}) {
    const Grid puzzle = sample(name);
    const LogicResult result = solveLogically(puzzle);
    ASSERT_EQ(result.status, SolveStatus::Solved) << name;
    EXPECT_TRUE(isSolutionOf(result.grid, puzzle)) << name;
    EXPECT_GT(result.guesses, 0) << name;
    EXPECT_EQ(rate(puzzle)->level, Level::Master) << name;
  }
}

TEST(Logic, StopsWhenStuckIfGuessingIsOff) {
  const LogicResult result = solveLogically(sample("Easter Monster"), {.allowGuessing = false});
  EXPECT_EQ(result.status, SolveStatus::Stuck);
}

TEST(Logic, DetectsContradictions) {
  const Grid puzzle = grid("12345678.........9" + std::string(63, '.'));
  EXPECT_EQ(solveLogically(puzzle).status, SolveStatus::Unsolvable);
  EXPECT_FALSE(rate(puzzle));
}

// The UI replays a solve by applying each step's cell changes; that must land
// exactly on the solution, even through guesses and backtracking.
TEST(Logic, StepChangesReplayToTheSolution) {
  for (const auto& s : kSamples) {
    const Grid puzzle = grid(s.puzzle);
    const LogicResult result = solveLogically(puzzle);
    ASSERT_EQ(result.status, SolveStatus::Solved) << s.name;
    const auto start = Board::fromGrid(puzzle);
    Grid values = puzzle;
    std::array<Mask, kCells> candidates{};
    for (int cell = 0; cell < kCells; ++cell) candidates[cell] = start->candidates(cell);
    for (const auto& step : result.steps) {
      for (const auto& change : step.changes) {
        values[change.cell] = change.value;
        candidates[change.cell] = change.candidates;
      }
    }
    EXPECT_EQ(values, result.grid) << s.name;
  }
}

// Soundness: from any position consistent with the unique solution, no technique
// may place a wrong digit or remove a correct candidate.
TEST(Logic, TechniquesNeverContradictTheSolution) {
  std::array<int, kTechniqueCount> used{};
  for (const Level level : {Level::Hard, Level::Expert, Level::Master}) {
    for (std::uint64_t seed = 1; seed <= 60; ++seed) {
      const GeneratedPuzzle generated = generate(level, seed);
      auto board = Board::fromGrid(generated.puzzle);
      ASSERT_TRUE(board);
      while (!board->isSolved()) {
        const auto step = findStep(*board);
        if (!step) break;
        const std::string_view name = techniqueInfo(step->technique).name;
        for (const auto& [cell, digit] : step->placements) {
          ASSERT_EQ(generated.solution[cell], digit) << name << " at " << cellName(cell);
        }
        for (const auto& [cell, digit] : step->eliminations) {
          ASSERT_NE(generated.solution[cell], digit) << name << " at " << cellName(cell);
        }
        EXPECT_FALSE(explain(*step, *board).empty());
        ++used[static_cast<std::size_t>(step->technique)];
        apply(*step, *board);
      }
    }
  }
  // The common techniques must have been exercised, or the test proves little.
  for (const Technique t : {Technique::FullHouse, Technique::HiddenSingle, Technique::NakedSingle,
                            Technique::Pointing, Technique::Claiming, Technique::NakedPair,
                            Technique::HiddenPair, Technique::XYWing}) {
    EXPECT_GT(used[static_cast<std::size_t>(t)], 0) << techniqueInfo(t).name;
  }
}

// Hidden quads almost never decide generated puzzles, so build one by hand: in
// row 1, digits 1-4 are only possible in r1c1-r1c4.
TEST(Logic, FindsAHiddenQuad) {
  auto board = Board::fromGrid(Grid{});
  ASSERT_TRUE(board);
  for (int cell = 4; cell < 9; ++cell) {
    for (int digit = 1; digit <= 4; ++digit) board->eliminate(cell, digit);
  }
  const auto step = findStep(*board);
  ASSERT_TRUE(step);
  EXPECT_EQ(step->technique, Technique::HiddenQuad);
  EXPECT_EQ(step->cells, (std::vector<std::uint8_t>{0, 1, 2, 3}));
  EXPECT_EQ(step->digits, 0x0F);
  EXPECT_EQ(step->eliminations.size(), 20u);  // 5-9 from each of the four cells
}

TEST(Rating, GradesByTheHardestTechniqueAndGivens) {
  const auto classic = rate(sample("Classic"));
  ASSERT_TRUE(classic);
  EXPECT_EQ(classic->level, Level::Medium);  // singles only, but just 30 givens
  EXPECT_EQ(classic->hardest, Technique::HiddenSingle);
  EXPECT_EQ(levelOf(Technique::HiddenSingle, 40), Level::Easy);
  EXPECT_EQ(levelOf(Technique::HiddenSingle, 30), Level::Medium);
  EXPECT_EQ(levelOf(Technique::NakedPair, 30), Level::Hard);
  EXPECT_EQ(levelOf(Technique::XWing, 30), Level::Expert);
  EXPECT_EQ(levelOf(Technique::Guess, 30), Level::Master);
}

TEST(Levels, RoundTripNames) {
  for (const Level level :
       {Level::Easy, Level::Medium, Level::Hard, Level::Expert, Level::Master}) {
    EXPECT_EQ(parseLevel(levelName(level)), level);
  }
  EXPECT_FALSE(parseLevel("impossible"));
}

}  // namespace
}  // namespace sudoku
