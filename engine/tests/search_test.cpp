#include <string>

#include "helpers.hpp"
#include "sudoku/search.hpp"

namespace sudoku {
namespace {

using testing::grid;
using testing::sample;

struct NamedSolver {
  std::string name;
  SearchResult (*solve)(const Grid&, const SearchOptions&);
};

class SearchSolvers : public ::testing::TestWithParam<NamedSolver> {
 protected:
  SearchResult solve(const Grid& puzzle, const SearchOptions& options = {}) const {
    return GetParam().solve(puzzle, options);
  }
};

TEST_P(SearchSolvers, SolvesTheClassic) {
  const SearchResult result = solve(sample("Classic"));
  ASSERT_EQ(result.status, SolveStatus::Solved);
  EXPECT_EQ(toString(result.solution), testing::kClassicSolution);
}

TEST_P(SearchSolvers, SolvesInkalasPuzzle) {
  const SearchResult result = solve(sample("Inkala 2012"));
  ASSERT_EQ(result.status, SolveStatus::Solved);
  EXPECT_EQ(toString(result.solution), testing::kInkalaSolution);
}

TEST_P(SearchSolvers, SolvesEverySample) {
  for (const auto& s : kSamples) {
    const Grid puzzle = grid(s.puzzle);
    const SearchResult result = solve(puzzle);
    ASSERT_EQ(result.status, SolveStatus::Solved) << s.name;
    EXPECT_TRUE(isSolutionOf(result.solution, puzzle)) << s.name;
  }
}

TEST_P(SearchSolvers, RejectsRepeatedGivens) {
  Grid puzzle{};
  puzzle[0] = puzzle[1] = 3;
  EXPECT_EQ(solve(puzzle).status, SolveStatus::Invalid);
}

TEST_P(SearchSolvers, ReportsPuzzlesWithoutSolution) {
  // r1c9 has no candidate: its row holds 1-8 and its column already has the 9.
  const Grid puzzle = grid("12345678.........9" + std::string(63, '.'));
  EXPECT_EQ(solve(puzzle).status, SolveStatus::Unsolvable);
}

TEST_P(SearchSolvers, CountsPastTheFirstSolution) {
  const SearchResult result = solve(Grid{}, {.solutionLimit = 2});
  EXPECT_EQ(result.status, SolveStatus::Solved);
  EXPECT_EQ(result.solutions, 2);
  EXPECT_TRUE(isSolutionOf(result.solution, Grid{}));
}

TEST_P(SearchSolvers, StopsAtTheNodeLimit) {
  const SearchResult result = solve(sample("Easter Monster"), {.nodeLimit = 5});
  EXPECT_EQ(result.status, SolveStatus::LimitReached);
  EXPECT_EQ(result.stats.placements, 5u);
}

TEST_P(SearchSolvers, TraceReplaysToTheSolution) {
  for (const auto* name : {"Classic", "AI Escargot"}) {
    const Grid puzzle = sample(name);
    SearchTrace trace(5'000'000);
    const SearchResult result = solve(puzzle, {.trace = &trace});
    ASSERT_EQ(result.status, SolveStatus::Solved);
    ASSERT_FALSE(trace.truncated());

    Grid replay = puzzle;
    std::uint64_t writes = 0;
    std::uint64_t erases = 0;
    for (const auto event : trace.events()) {
      const auto kind = static_cast<SearchTrace::Kind>(event >> 11);
      const int cell = static_cast<int>((event >> 4) & 0x7F);
      const int digit = static_cast<int>(event & 0xF);
      if (kind == SearchTrace::Kind::Undo) {
        ASSERT_EQ(replay[cell], digit);
        replay[cell] = 0;
        ++erases;
      } else {
        ASSERT_EQ(replay[cell], 0);
        replay[cell] = static_cast<std::uint8_t>(digit);
        ++writes;
      }
    }
    EXPECT_EQ(replay, result.solution) << name;
    EXPECT_EQ(writes, result.stats.placements);
    EXPECT_EQ(erases, result.stats.backtracks);
  }
}

TEST_P(SearchSolvers, MarksTraceTruncation) {
  SearchTrace trace(10);
  const SearchResult result = solve(sample("AI Escargot"), {.trace = &trace});
  EXPECT_EQ(result.status, SolveStatus::Solved);
  EXPECT_EQ(trace.events().size(), 10u);
  EXPECT_TRUE(trace.truncated());
}

INSTANTIATE_TEST_SUITE_P(All, SearchSolvers,
                         ::testing::Values(NamedSolver{"naive", solveNaive},
                                           NamedSolver{"smart", solveSmart},
                                           NamedSolver{"dlx", solveDlx}),
                         [](const auto& info) { return info.param.name; });

TEST(CountSolutions, StopsAtTheLimit) {
  EXPECT_EQ(countSolutions(sample("Classic")), 1);
  EXPECT_EQ(countSolutions(Grid{}), 2);
  EXPECT_EQ(countSolutions(Grid{}, 5), 5);
  Grid repeated{};
  repeated[0] = repeated[1] = 1;
  EXPECT_EQ(countSolutions(repeated), 0);
}

TEST(SmartSearch, NeedsFarFewerNodesThanNaiveBacktracking) {
  const Grid puzzle = sample("Anti-Brute-Force");
  const SearchResult naive = solveNaive(puzzle, {.nodeLimit = 1'000'000});
  EXPECT_EQ(naive.status, SolveStatus::LimitReached);
  const SearchResult smart = solveSmart(puzzle);
  EXPECT_EQ(smart.status, SolveStatus::Solved);
  EXPECT_LT(smart.stats.placements, 1000u);
}

}  // namespace
}  // namespace sudoku
