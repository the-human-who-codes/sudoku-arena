#include "sudoku/api.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <optional>
#include <string_view>
#include <vector>

#include "json.hpp"
#include "sudoku/generator.hpp"
#include "sudoku/logic.hpp"
#include "sudoku/search.hpp"

#ifndef SUDOKU_VERSION
#define SUDOKU_VERSION "dev"
#endif

namespace sudoku::api {
namespace {

using detail::JsonWriter;
using Clock = std::chrono::steady_clock;

enum class Algorithm { Logic, Smart, Naive, Dlx };

struct AlgorithmInfo {
  Algorithm algorithm;
  std::string_view id;
};

constexpr std::array<AlgorithmInfo, 4> kAlgorithms = {{
    {Algorithm::Dlx, "dlx"},
    {Algorithm::Smart, "smart"},
    {Algorithm::Logic, "logic"},
    {Algorithm::Naive, "naive"},
}};

// Naive backtracking needs hundreds of millions of placements on adversarial
// puzzles; the cap keeps a browser tab responsive.
constexpr std::uint64_t kNaiveNodeLimit = 100'000'000;

// Quick runs are repeated for at least this long so that coarse timers (browsers
// round performance.now()) still give stable averages.
constexpr double kTimingBudgetMicros = 20'000.0;

constexpr std::string_view kGridError =
    "A puzzle needs exactly 81 cells: digits 1-9 for givens and 0 or . for blanks.";

std::optional<Algorithm> parseAlgorithm(std::string_view id) {
  for (const auto& info : kAlgorithms) {
    if (info.id == id) return info.algorithm;
  }
  return std::nullopt;
}

std::string error(std::string_view message) {
  JsonWriter json;
  json.beginObject().field("ok", false).field("error", message).endObject();
  return json.take();
}

double microsSince(Clock::time_point start) {
  return std::chrono::duration<double, std::micro>(Clock::now() - start).count();
}

struct Timing {
  double micros;
  int runs;
};

// Average duration of `run`, given the duration of one run already made.
template <typename Run>
Timing timeRuns(double firstMicros, Run&& run) {
  if (firstMicros >= kTimingBudgetMicros) return {firstMicros, 1};
  int runs = 0;
  double elapsed = 0;
  const auto start = Clock::now();
  do {
    run();
    ++runs;
    elapsed = microsSince(start);
  } while (elapsed < kTimingBudgetMicros && runs < 2000);
  return {elapsed / runs, runs};
}

SearchResult runSearch(Algorithm algorithm, const Grid& puzzle, SearchTrace* trace) {
  const SearchOptions options{
      .nodeLimit = algorithm == Algorithm::Naive ? kNaiveNodeLimit : 0,
      .solutionLimit = 1,
      .trace = trace,
  };
  switch (algorithm) {
    case Algorithm::Naive: return solveNaive(puzzle, options);
    case Algorithm::Dlx: return solveDlx(puzzle, options);
    default: return solveSmart(puzzle, options);
  }
}

void writeGrid(JsonWriter& json, std::string_view name, const Grid& grid, bool present) {
  json.key(name);
  if (present) {
    json.value(toString(grid));
  } else {
    json.null();
  }
}

template <typename Numbers>
void writeNumbers(JsonWriter& json, const Numbers& numbers) {
  json.beginArray();
  for (const auto number : numbers) json.value(number);
  json.endArray();
}

void writeCandidates(JsonWriter& json, const std::vector<Candidate>& candidates) {
  json.beginArray();
  for (const auto& [cell, digit] : candidates) json.beginArray().value(cell).value(digit).endArray();
  json.endArray();
}

void writeDigits(JsonWriter& json, Mask digits) {
  json.beginArray();
  for (Mask m = digits; m != 0; m &= m - 1) json.value(firstDigit(m));
  json.endArray();
}

void writeRating(JsonWriter& json, Level level, Technique hardest) {
  const auto& info = techniqueInfo(hardest);
  json.key("rating")
      .beginObject()
      .field("level", levelName(level))
      .field("hardest", info.id)
      .field("hardestName", info.name)
      .endObject();
}

void writeTechniqueCounts(JsonWriter& json, const std::array<int, kTechniqueCount>& counts) {
  json.key("techniques").beginArray();
  for (std::size_t i = 0; i < counts.size(); ++i) {
    if (counts[i] == 0) continue;
    const auto& info = techniqueInfo(static_cast<Technique>(i));
    json.beginObject()
        .field("id", info.id)
        .field("name", info.name)
        .field("level", levelName(info.level))
        .field("count", counts[i])
        .endObject();
  }
  json.endArray();
}

void writeStep(JsonWriter& json, const LogicStep& step) {
  const auto& info = techniqueInfo(step.technique);
  json.beginObject()
      .field("technique", info.id)
      .field("name", info.name)
      .field("level", levelName(info.level))
      .field("text", step.explanation);
  json.key("place");
  writeCandidates(json, step.placements);
  json.key("eliminate");
  writeCandidates(json, step.eliminations);
  json.key("cells");
  writeNumbers(json, step.cells);
  json.key("units");
  writeNumbers(json, step.units);
  json.key("digits");
  writeDigits(json, step.digits);
  json.key("changes").beginArray();
  for (const auto& change : step.changes) {
    json.beginArray().value(change.cell).value(change.value).value(change.candidates).endArray();
  }
  json.endArray().endObject();
}

void writeSearchSolve(JsonWriter& json, const Grid& puzzle, Algorithm algorithm, int traceLimit) {
  SearchTrace trace(static_cast<std::size_t>(std::max(traceLimit, 0)));
  const auto start = Clock::now();
  const SearchResult result = runSearch(algorithm, puzzle, &trace);
  const Timing timing =
      timeRuns(microsSince(start), [&] { return runSearch(algorithm, puzzle, nullptr); });

  json.field("status", toString(result.status));
  writeGrid(json, "solution", result.solution, result.status == SolveStatus::Solved);
  json.field("micros", timing.micros);
  json.key("stats")
      .beginObject()
      .field("placements", result.stats.placements)
      .field("guesses", result.stats.guesses)
      .field("backtracks", result.stats.backtracks)
      .endObject();
  json.key("trace").beginObject().field("kind", "search").field("truncated", trace.truncated());
  json.key("events");
  writeNumbers(json, trace.events());
  json.endObject();
}

void writeLogicSolve(JsonWriter& json, const Grid& puzzle) {
  const LogicResult result = solveLogically(puzzle);
  // Time runs without explanations: writing prose costs more than the deductions.
  const auto start = Clock::now();
  solveLogically(puzzle, {.explain = false});
  const Timing timing = timeRuns(microsSince(start),
                                 [&] { return solveLogically(puzzle, {.explain = false}); });

  std::size_t placements = 0;
  for (const auto& step : result.steps) placements += step.placements.size();

  json.field("status", toString(result.status));
  writeGrid(json, "solution", result.grid, result.status == SolveStatus::Solved);
  json.field("micros", timing.micros);
  json.key("stats")
      .beginObject()
      .field("steps", result.steps.size())
      .field("placements", placements)
      .field("guesses", result.guesses)
      .field("backtracks", result.backtracks)
      .endObject();
  if (result.status == SolveStatus::Solved) {
    const Technique hardest = result.guesses > 0 ? Technique::Guess : result.hardest;
    writeRating(json, levelOf(hardest, clueCount(puzzle)), hardest);
  }
  writeTechniqueCounts(json, result.techniqueCounts);

  json.key("trace").beginObject().field("kind", "logic");
  json.key("candidates").beginArray();
  if (const auto board = Board::fromGrid(puzzle)) {
    for (int cell = 0; cell < kCells; ++cell) json.value(board->candidates(cell));
  }
  json.endArray();
  json.key("steps").beginArray();
  for (const auto& step : result.steps) writeStep(json, step);
  json.endArray().endObject();
}

}  // namespace

std::string solve(const std::string& puzzleText, const std::string& algorithmId, int traceLimit) {
  const auto puzzle = parseGrid(puzzleText);
  if (!puzzle) return error(kGridError);
  const auto algorithm = parseAlgorithm(algorithmId);
  if (!algorithm) return error("Unknown algorithm \"" + algorithmId + "\".");

  JsonWriter json;
  json.beginObject()
      .field("ok", true)
      .field("algorithm", algorithmId)
      .field("puzzle", toString(*puzzle));
  if (*algorithm == Algorithm::Logic) {
    writeLogicSolve(json, *puzzle);
  } else {
    writeSearchSolve(json, *puzzle, *algorithm, traceLimit);
  }
  json.endObject();
  return json.take();
}

std::string generate(const std::string& levelId, const std::string& seedText) {
  const auto level = parseLevel(levelId);
  if (!level) {
    return error("Unknown level \"" + levelId + "\". Use easy, medium, hard, expert or master.");
  }
  const auto start = Clock::now();
  const GeneratedPuzzle generated = sudoku::generate(*level, seedFromString(seedText));
  const double micros = microsSince(start);

  JsonWriter json;
  json.beginObject()
      .field("ok", true)
      .field("puzzle", toString(generated.puzzle))
      .field("solution", toString(generated.solution))
      .field("requested", levelName(*level))
      .field("seed", seedText)
      .field("clues", clueCount(generated.puzzle))
      .field("attempts", generated.attempts)
      .field("micros", micros);
  writeRating(json, generated.rating.level, generated.rating.hardest);
  json.endObject();
  return json.take();
}

std::string analyze(const std::string& puzzleText) {
  const auto puzzle = parseGrid(puzzleText);
  if (!puzzle) return error(kGridError);

  const std::vector<int> conflicts = conflictingCells(*puzzle);
  const int solutions = conflicts.empty() ? countSolutions(*puzzle, 2) : 0;

  JsonWriter json;
  json.beginObject()
      .field("ok", true)
      .field("puzzle", toString(*puzzle))
      .field("clues", clueCount(*puzzle))
      .field("valid", conflicts.empty())
      .field("solutions", solutions);
  json.key("conflicts");
  writeNumbers(json, conflicts);
  if (solutions == 1) {
    if (const auto rating = rate(*puzzle)) writeRating(json, rating->level, rating->hardest);
  }
  json.endObject();
  return json.take();
}

std::string hint(const std::string& puzzleText, const std::string& currentText) {
  const auto puzzle = parseGrid(puzzleText);
  const auto current = parseGrid(currentText);
  if (!puzzle || !current) return error(kGridError);
  for (int cell = 0; cell < kCells; ++cell) {
    if ((*puzzle)[cell] != 0 && (*current)[cell] != (*puzzle)[cell]) {
      return error("The current grid must keep every given of the puzzle.");
    }
  }

  JsonWriter json;
  json.beginObject().field("ok", true);
  const auto finish = [&json] {
    json.endObject();
    return json.take();
  };

  const SearchResult solved = solveSmart(*puzzle, {.solutionLimit = 2});
  if (solved.solutions == 0) {
    json.field("kind", "unsolvable").field("text", "This puzzle has no solution.");
    return finish();
  }
  const bool unique = solved.solutions == 1;

  // Entries that contradict the solution (or, without a unique one, each other).
  std::vector<int> mistakes;
  if (unique) {
    for (int cell = 0; cell < kCells; ++cell) {
      if ((*puzzle)[cell] == 0 && (*current)[cell] != 0 &&
          (*current)[cell] != solved.solution[cell]) {
        mistakes.push_back(cell);
      }
    }
  } else {
    mistakes = conflictingCells(*current);
  }
  if (!mistakes.empty()) {
    json.field("kind", "mistakes")
        .field("text", mistakes.size() == 1
                           ? std::string("One entry doesn't fit the solution.")
                           : std::to_string(mistakes.size()) + " entries don't fit the solution.");
    json.key("cells");
    writeNumbers(json, mistakes);
    return finish();
  }

  const auto board = Board::fromGrid(*current);
  if (!board || board->isSolved()) {
    json.field("kind", "solved").field("text", "The puzzle is solved.");
    return finish();
  }

  // Deduce from the current position until a digit can be placed.
  Board scratch = *board;
  std::vector<LogicStep> steps;
  for (int i = 0; i < 40 && !scratch.hasContradiction(); ++i) {
    auto step = findStep(scratch);
    if (!step) break;
    step->explanation = explain(*step, scratch);
    apply(*step, scratch);
    const bool placed = !step->placements.empty();
    steps.push_back(std::move(*step));
    if (placed) {
      json.field("kind", "step").field("text", steps.back().explanation);
      json.key("steps").beginArray();
      for (const auto& s : steps) writeStep(json, s);
      json.endArray();
      return finish();
    }
  }

  if (!unique) {
    json.field("kind", "none")
        .field("text", "This puzzle has several solutions, so there is no single next step.");
    return finish();
  }
  // Beyond the known techniques: reveal the most constrained cell.
  int cell = -1;
  for (int c = 0; c < kCells; ++c) {
    if ((*current)[c] != 0) continue;
    if (cell < 0 || countDigits(board->candidates(c)) < countDigits(board->candidates(cell))) {
      cell = c;
    }
  }
  const int digit = solved.solution[cell];
  json.field("kind", "reveal")
      .field("cell", cell)
      .field("digit", digit)
      .field("text", "This position needs trial and error, so here is a free cell: " +
                         cellName(cell) + " is " + std::to_string(digit) + ".");
  return finish();
}

std::string benchmark(const std::string& puzzleText) {
  const auto puzzle = parseGrid(puzzleText);
  if (!puzzle) return error(kGridError);

  JsonWriter json;
  json.beginObject().field("ok", true).field("puzzle", toString(*puzzle));
  json.key("results").beginArray();
  for (const auto& info : kAlgorithms) {
    json.beginObject().field("algorithm", info.id);
    const auto start = Clock::now();
    if (info.algorithm == Algorithm::Logic) {
      const LogicResult result = solveLogically(*puzzle, {.explain = false});
      const Timing timing = timeRuns(microsSince(start),
                                     [&] { return solveLogically(*puzzle, {.explain = false}); });
      int steps = 0;
      for (const int count : result.techniqueCounts) steps += count;
      json.field("status", toString(result.status))
          .field("micros", timing.micros)
          .field("runs", timing.runs)
          .field("steps", steps)
          .field("guesses", result.guesses)
          .field("backtracks", result.backtracks);
    } else {
      const SearchResult result = runSearch(info.algorithm, *puzzle, nullptr);
      const Timing timing = timeRuns(microsSince(start),
                                     [&] { return runSearch(info.algorithm, *puzzle, nullptr); });
      json.field("status", toString(result.status))
          .field("micros", timing.micros)
          .field("runs", timing.runs)
          .field("placements", result.stats.placements)
          .field("guesses", result.stats.guesses)
          .field("backtracks", result.stats.backtracks);
    }
    json.endObject();
  }
  json.endArray().endObject();
  return json.take();
}

std::string version() {
  JsonWriter json;
  json.beginObject().field("ok", true).field("version", SUDOKU_VERSION).endObject();
  return json.take();
}

}  // namespace sudoku::api
