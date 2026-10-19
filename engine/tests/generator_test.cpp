#include <set>

#include "helpers.hpp"
#include "sudoku/generator.hpp"
#include "sudoku/search.hpp"

namespace sudoku {
namespace {

TEST(Rng, IsReproducibleAndUnbiasedInRange) {
  Rng a(7);
  Rng b(7);
  for (int i = 0; i < 1000; ++i) EXPECT_EQ(a.next(), b.next());

  Rng rng(1);
  std::array<int, 9> counts{};
  for (int i = 0; i < 90'000; ++i) {
    const auto value = rng.below(9);
    ASSERT_LT(value, 9u);
    ++counts[value];
  }
  for (const int count : counts) EXPECT_NEAR(count, 10'000, 500);
}

// SplitMix64's published reference output for seed 0: the generator must produce
// identical numbers in every build (native, Linux CI, WebAssembly).
TEST(Rng, MatchesTheReferenceSequence) {
  Rng rng(0);
  EXPECT_EQ(rng.next(), 0xE220A8397B1DCDAFULL);
  EXPECT_EQ(rng.next(), 0x6E789E6AA1B965F4ULL);
  EXPECT_EQ(rng.next(), 0x06C45D188009454FULL);
}

TEST(Seeds, ParseNumbersAndHashText) {
  EXPECT_EQ(seedFromString("42"), 42u);
  EXPECT_EQ(seedFromString("18446744073709551615"), seedFromString("18446744073709551615"));
  EXPECT_EQ(seedFromString("daily-2026-10-19"), seedFromString("daily-2026-10-19"));
  EXPECT_NE(seedFromString("daily-2026-10-19"), seedFromString("daily-2026-10-20"));
  EXPECT_EQ(seedFromString(""), 0xCBF29CE484222325ULL);  // FNV-1a offset basis
}

TEST(Generator, RandomSolutionsAreCompleteAndVaried) {
  Rng rng(3);
  std::set<std::string> seen;
  for (int i = 0; i < 20; ++i) {
    const Grid solution = randomSolution(rng);
    EXPECT_TRUE(isFilled(solution));
    EXPECT_TRUE(isConsistent(solution));
    seen.insert(toString(solution));
  }
  EXPECT_EQ(seen.size(), 20u);
}

TEST(Generator, IsDeterministicPerSeed) {
  EXPECT_EQ(generate(Level::Hard, 42).puzzle, generate(Level::Hard, 42).puzzle);
  EXPECT_NE(generate(Level::Hard, 42).puzzle, generate(Level::Hard, 43).puzzle);
}

// Pins the exact output for one seed so that any change to the generator, or a
// platform difference, is caught. Two players given the same seed must get the
// same puzzle.
TEST(Generator, ProducesTheGoldenPuzzle) {
  EXPECT_EQ(toString(generate(Level::Medium, 2026).puzzle),
            "3....5...1..8.794.....9....7.918.6....8...5....6.597.2....2.....634.1..5...5....3");
}

class GeneratorLevels : public ::testing::TestWithParam<Level> {};

TEST_P(GeneratorLevels, ProducesUniqueSymmetricPuzzlesAtTheLevel) {
  for (std::uint64_t seed = 1; seed <= 12; ++seed) {
    const GeneratedPuzzle generated = generate(GetParam(), seed);
    EXPECT_EQ(countSolutions(generated.puzzle), 1);
    EXPECT_TRUE(isSolutionOf(generated.solution, generated.puzzle));
    EXPECT_EQ(generated.rating.level, GetParam()) << "seed " << seed;
    for (int cell = 0; cell < kCells; ++cell) {
      EXPECT_EQ(generated.puzzle[cell] != 0, generated.puzzle[kCells - 1 - cell] != 0);
    }
  }
}

INSTANTIATE_TEST_SUITE_P(All, GeneratorLevels,
                         ::testing::Values(Level::Easy, Level::Medium, Level::Hard,
                                           Level::Expert, Level::Master),
                         [](const auto& info) { return std::string(levelName(info.param)); });

}  // namespace
}  // namespace sudoku
