#pragma once

#include <gtest/gtest.h>

#include <string_view>

#include "sudoku/grid.hpp"
#include "sudoku/samples.hpp"

namespace sudoku::testing {

inline constexpr std::string_view kClassicSolution =
    "534678912672195348198342567859761423426853791713924856961537284287419635345286179";
inline constexpr std::string_view kInkalaSolution =
    "812753649943682175675491283154237896369845721287169534521974368438526917796318452";

inline Grid grid(std::string_view text) {
  const auto parsed = parseGrid(text);
  EXPECT_TRUE(parsed.has_value()) << "not a grid: " << text;
  return parsed.value_or(Grid{});
}

inline Grid sample(std::string_view name) {
  for (const auto& s : kSamples) {
    if (s.name == name) return grid(s.puzzle);
  }
  ADD_FAILURE() << "no sample named " << name;
  return Grid{};
}

}  // namespace sudoku::testing
