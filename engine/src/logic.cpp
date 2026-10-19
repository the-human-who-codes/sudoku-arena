#include <utility>
#include <vector>

#include "sudoku/logic.hpp"

namespace sudoku {
namespace {

std::vector<CellState> diff(const Board& before, const Board& after) {
  std::vector<CellState> changes;
  for (int cell = 0; cell < kCells; ++cell) {
    if (before.value(cell) != after.value(cell) ||
        before.candidates(cell) != after.candidates(cell)) {
      changes.push_back({static_cast<std::uint8_t>(cell), after.value(cell), after.candidates(cell)});
    }
  }
  return changes;
}

// Cell with the fewest candidates; ties go to the earliest cell.
int mostConstrainedCell(const Board& board) {
  int best = -1;
  for (int cell = 0; cell < kCells; ++cell) {
    if (board.value(cell) != 0) continue;
    if (best < 0 || countDigits(board.candidates(cell)) < countDigits(board.candidates(best))) {
      best = cell;
    }
  }
  return best;
}

}  // namespace

LogicResult solveLogically(const Grid& puzzle, const LogicOptions& options) {
  LogicResult result;
  result.grid = puzzle;
  const auto start = Board::fromGrid(puzzle);
  if (!start) {
    result.status = SolveStatus::Invalid;
    return result;
  }

  // Each guess remembers the board before it, so a contradiction can rewind to it.
  struct Branch {
    Board board;
    Candidate guess;
  };
  std::vector<Branch> branches;
  Board board = *start;

  const auto record = [&](LogicStep&& step, const Board& before) {
    ++result.techniqueCounts[static_cast<std::size_t>(step.technique)];
    if (step.technique > result.hardest) result.hardest = step.technique;
    if (!options.explain) return;
    if (step.explanation.empty()) step.explanation = explain(step, before);
    step.changes = diff(before, board);
    result.steps.push_back(std::move(step));
  };

  for (std::size_t count = 0;; ++count) {
    if (board.isSolved()) {
      result.status = SolveStatus::Solved;
      break;
    }
    if (count == options.stepLimit) {
      result.status = SolveStatus::LimitReached;
      break;
    }

    if (board.hasContradiction()) {
      if (branches.empty()) {
        result.status = SolveStatus::Unsolvable;
        break;
      }
      const Branch branch = std::move(branches.back());
      branches.pop_back();
      const auto [cell, digit] = branch.guess;

      LogicStep step;
      step.technique = Technique::Backtrack;
      step.eliminations = {branch.guess};
      step.cells = {cell};
      step.digits = digitBit(digit);
      if (options.explain) {
        step.explanation = "Contradiction: " + board.describeContradiction() + ". So the guess " +
                           std::to_string(digit) + " for " + cellName(cell) +
                           " was wrong: rewind to it and rule " + std::to_string(digit) + " out.";
      }
      const Board before = board;
      board = branch.board;
      board.eliminate(cell, digit);
      ++result.backtracks;
      record(std::move(step), before);
      continue;
    }

    if (auto step = findStep(board)) {
      const Board before = board;
      apply(*step, board);
      record(std::move(*step), before);
      continue;
    }

    if (!options.allowGuessing) {
      result.status = SolveStatus::Stuck;
      break;
    }

    const int cell = mostConstrainedCell(board);
    const Candidate guess{static_cast<std::uint8_t>(cell),
                          static_cast<std::uint8_t>(firstDigit(board.candidates(cell)))};
    branches.push_back({board, guess});
    LogicStep step;
    step.technique = Technique::Guess;
    step.placements = {guess};
    step.cells = {guess.cell};
    step.digits = digitBit(guess.digit);
    const Board before = board;
    apply(step, board);
    ++result.guesses;
    record(std::move(step), before);
  }

  result.grid = board.values();
  return result;
}

std::optional<Rating> rate(const Grid& puzzle) {
  const LogicResult result =
      solveLogically(puzzle, {.allowGuessing = false, .explain = false});
  if (result.status == SolveStatus::Invalid || result.status == SolveStatus::Unsolvable) {
    return std::nullopt;
  }
  Rating rating;
  rating.techniqueCounts = result.techniqueCounts;
  if (result.status == SolveStatus::Solved) {
    rating.hardest = result.hardest;
    rating.level = levelOf(result.hardest, clueCount(puzzle));
  } else {
    rating.hardest = Technique::Guess;
    rating.level = Level::Master;
  }
  return rating;
}

}  // namespace sudoku
