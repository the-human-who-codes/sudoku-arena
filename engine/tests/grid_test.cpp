#include <algorithm>
#include <set>
#include <string>

#include "helpers.hpp"

namespace sudoku {
namespace {

using testing::grid;
using testing::sample;

TEST(Grid, ParsesDotsAndZerosAlike) {
  std::string zeros(kSamples[0].puzzle);
  std::replace(zeros.begin(), zeros.end(), '.', '0');
  const auto parsed = parseGrid(zeros);
  ASSERT_TRUE(parsed);
  EXPECT_EQ(*parsed, sample("Classic"));
  EXPECT_EQ(toString(*parsed), kSamples[0].puzzle);
}

TEST(Grid, RoundTripsThePrettyFormat) {
  const Grid g = sample("AI Escargot");
  EXPECT_EQ(parseGrid(toPrettyString(g)), g);
}

TEST(Grid, RejectsMalformedText) {
  EXPECT_FALSE(parseGrid(""));
  EXPECT_FALSE(parseGrid(std::string(80, '.')));
  EXPECT_FALSE(parseGrid(std::string(82, '.')));
  EXPECT_FALSE(parseGrid(std::string(80, '.') + "x"));
}

TEST(Topology, UnitsHoldNineCellsAndCellsTwentyPeers) {
  for (const auto& unit : kTopology.unitCells) {
    EXPECT_EQ(std::set<int>(unit.begin(), unit.end()).size(), 9u);
  }
  for (int cell = 0; cell < kCells; ++cell) {
    const auto& peers = kTopology.peers[cell];
    const std::set<int> distinct(peers.begin(), peers.end());
    EXPECT_EQ(distinct.size(), 20u);
    EXPECT_FALSE(distinct.contains(cell));
    for (const int unit : kTopology.cellUnits[cell]) {
      const auto& cells = kTopology.unitCells[unit];
      EXPECT_NE(std::find(cells.begin(), cells.end(), cell), cells.end());
    }
  }
  EXPECT_EQ(boxOf(30), 4);
  EXPECT_EQ(boxOf(80), 8);
}

TEST(Grid, FindsRepeatedDigits) {
  Grid g{};
  g[0] = 5;
  g[8] = 5;  // same row
  EXPECT_FALSE(isConsistent(g));
  EXPECT_EQ(conflictingCells(g), (std::vector<int>{0, 8}));

  EXPECT_TRUE(isConsistent(sample("Classic")));
  EXPECT_TRUE(conflictingCells(sample("Classic")).empty());
}

TEST(Grid, ChecksSolutionsAgainstGivens) {
  const Grid puzzle = sample("Classic");
  Grid solution = grid(testing::kClassicSolution);
  EXPECT_TRUE(isSolutionOf(solution, puzzle));
  std::swap(solution[2], solution[3]);  // breaks rows... and a given
  EXPECT_FALSE(isSolutionOf(solution, puzzle));
  EXPECT_FALSE(isSolutionOf(puzzle, puzzle));
}

TEST(Grid, NamesCellsAndUnits) {
  EXPECT_EQ(cellName(0), "r1c1");
  EXPECT_EQ(cellName(12), "r2c4");
  EXPECT_EQ(cellName(80), "r9c9");
  EXPECT_EQ(unitName(0), "row 1");
  EXPECT_EQ(unitName(10), "column 2");
  EXPECT_EQ(unitName(26), "box 9");
}

}  // namespace
}  // namespace sudoku
