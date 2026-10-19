// Command-line front end for the engine: solve, rate, generate and benchmark.

#include <chrono>
#include <cstdio>
#include <iostream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "sudoku/generator.hpp"
#include "sudoku/logic.hpp"
#include "sudoku/samples.hpp"
#include "sudoku/search.hpp"

namespace {

using namespace sudoku;
using Clock = std::chrono::steady_clock;

int usage() {
  std::cerr << "Usage:\n"
               "  sudoku solve <puzzle|-> [--algo logic|smart|naive|dlx] [--steps]\n"
               "  sudoku rate <puzzle|->\n"
               "  sudoku generate [--level easy|medium|hard|expert|master] [--seed S] [--count N]\n"
               "  sudoku bench\n"
               "Puzzles are 81 characters: digits 1-9 for givens, 0 or . for blanks.\n";
  return 2;
}

struct Args {
  std::vector<std::string> positional;
  std::vector<std::pair<std::string, std::string>> options;
  bool steps = false;

  std::optional<std::string> option(std::string_view name) const {
    for (const auto& [key, value] : options) {
      if (key == name) return value;
    }
    return std::nullopt;
  }
};

// Every option takes a value except the --steps flag.
Args parseArgs(int argc, char** argv) {
  Args args;
  for (int i = 2; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--steps") {
      args.steps = true;
    } else if (arg.starts_with("--") && i + 1 < argc) {
      args.options.emplace_back(arg, argv[++i]);
    } else {
      args.positional.push_back(arg);
    }
  }
  return args;
}

std::optional<Grid> readPuzzle(const Args& args) {
  if (args.positional.empty()) return std::nullopt;
  std::string text = args.positional.front();
  if (text == "-") text.assign(std::istreambuf_iterator<char>(std::cin), {});
  const auto grid = parseGrid(text);
  if (!grid) std::cerr << "Not a puzzle: expected 81 cells.\n";
  return grid;
}

double microsSince(Clock::time_point start) {
  return std::chrono::duration<double, std::micro>(Clock::now() - start).count();
}

std::string formatMicros(double micros) {
  char buffer[32];
  if (micros < 1000) {
    std::snprintf(buffer, sizeof buffer, "%.1f us", micros);
  } else if (micros < 1e6) {
    std::snprintf(buffer, sizeof buffer, "%.2f ms", micros / 1000);
  } else {
    std::snprintf(buffer, sizeof buffer, "%.2f s", micros / 1e6);
  }
  return buffer;
}

int solveCommand(const Args& args) {
  const auto puzzle = readPuzzle(args);
  if (!puzzle) return 1;
  const std::string algorithm = args.option("--algo").value_or("smart");

  const auto start = Clock::now();
  if (algorithm == "logic") {
    const LogicResult result = solveLogically(*puzzle);
    const double micros = microsSince(start);
    if (args.steps) {
      for (std::size_t i = 0; i < result.steps.size(); ++i) {
        std::cout << i + 1 << ". " << result.steps[i].explanation << '\n';
      }
      std::cout << '\n';
    }
    std::cout << toPrettyString(result.grid) << '\n'
              << toString(result.status) << " in " << formatMicros(micros) << ", "
              << result.steps.size() << " steps, " << result.guesses << " guesses\n";
    return result.status == SolveStatus::Solved ? 0 : 1;
  }

  SearchResult result;
  if (algorithm == "naive") {
    result = solveNaive(*puzzle, {.nodeLimit = 500'000'000});
  } else if (algorithm == "dlx") {
    result = solveDlx(*puzzle);
  } else if (algorithm == "smart") {
    result = solveSmart(*puzzle);
  } else {
    return usage();
  }
  const double micros = microsSince(start);
  if (result.status == SolveStatus::Solved) std::cout << toPrettyString(result.solution) << '\n';
  std::cout << toString(result.status) << " in " << formatMicros(micros) << ", "
            << result.stats.placements << " placements, " << result.stats.guesses
            << " guesses, " << result.stats.backtracks << " backtracks\n";
  return result.status == SolveStatus::Solved ? 0 : 1;
}

int rateCommand(const Args& args) {
  const auto puzzle = readPuzzle(args);
  if (!puzzle) return 1;
  const auto rating = rate(*puzzle);
  if (!rating) {
    std::cout << "invalid or unsolvable\n";
    return 1;
  }
  std::cout << levelName(rating->level) << " (hardest: " << techniqueInfo(rating->hardest).name
            << ")\n";
  for (int i = 0; i < kTechniqueCount; ++i) {
    if (rating->techniqueCounts[i] > 0) {
      std::cout << "  " << techniqueInfo(static_cast<Technique>(i)).name << ": "
                << rating->techniqueCounts[i] << '\n';
    }
  }
  return 0;
}

int generateCommand(const Args& args) {
  const auto level = parseLevel(args.option("--level").value_or("medium"));
  if (!level) return usage();
  const std::uint64_t seed = seedFromString(
      args.option("--seed").value_or(std::to_string(Clock::now().time_since_epoch().count())));
  const int count = std::stoi(args.option("--count").value_or("1"));
  for (int i = 0; i < count; ++i) {
    const auto start = Clock::now();
    const GeneratedPuzzle generated = generate(*level, seed + static_cast<std::uint64_t>(i));
    std::cout << toString(generated.puzzle) << "  " << levelName(generated.rating.level) << "  "
              << clueCount(generated.puzzle) << " clues  "
              << techniqueInfo(generated.rating.hardest).name << "  "
              << formatMicros(microsSince(start)) << " (" << generated.attempts << " attempts)\n";
  }
  return 0;
}

int benchCommand() {
  std::printf("%-18s %12s %12s %12s %12s\n", "puzzle", "dlx", "smart", "logic", "naive");
  for (const auto& sample : kSamples) {
    const Grid puzzle = *parseGrid(sample.puzzle);
    std::printf("%-18s", std::string(sample.name).c_str());
    for (int algorithm = 0; algorithm < 4; ++algorithm) {
      int runs = 0;
      const auto start = Clock::now();
      bool solved = true;
      do {
        switch (algorithm) {
          case 0: solved = solveDlx(puzzle).status == SolveStatus::Solved; break;
          case 1: solved = solveSmart(puzzle).status == SolveStatus::Solved; break;
          case 2: solved = solveLogically(puzzle, {.explain = false}).status == SolveStatus::Solved; break;
          default:
            solved = solveNaive(puzzle, {.nodeLimit = 500'000'000}).status == SolveStatus::Solved;
        }
        ++runs;
      } while (microsSince(start) < 200'000 && runs < 10'000);
      const std::string cell = solved ? formatMicros(microsSince(start) / runs) : "gave up";
      std::printf(" %12s", cell.c_str());
    }
    std::printf("\n");
  }
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) return usage();
  const std::string_view command = argv[1];
  const Args args = parseArgs(argc, argv);
  if (command == "solve") return solveCommand(args);
  if (command == "rate") return rateCommand(args);
  if (command == "generate") return generateCommand(args);
  if (command == "bench") return benchCommand();
  return usage();
}
