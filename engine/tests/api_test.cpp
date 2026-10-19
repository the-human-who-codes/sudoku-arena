#include <string>
#include <string_view>

#include "../src/json.hpp"
#include "helpers.hpp"
#include "sudoku/api.hpp"

namespace sudoku {
namespace {

bool has(const std::string& json, std::string_view needle) {
  return json.find(needle) != std::string::npos;
}

// Value of a string field, e.g. stringField(R"({"a":"b"})", "a") == "b".
std::string stringField(const std::string& json, std::string_view name) {
  const std::string key = "\"" + std::string(name) + "\":\"";
  const auto start = json.find(key);
  if (start == std::string::npos) return {};
  const auto from = start + key.size();
  return json.substr(from, json.find('"', from) - from);
}

const std::string kClassic(kSamples[0].puzzle);

TEST(JsonWriter, SeparatesAndEscapes) {
  detail::JsonWriter json;
  json.beginObject()
      .field("text", "say \"hi\"\n")
      .field("n", 3)
      .field("x", 1.5)
      .key("list")
      .beginArray()
      .value(1)
      .value(true)
      .null()
      .beginObject()
      .endObject()
      .endArray()
      .endObject();
  EXPECT_EQ(json.take(), R"({"text":"say \"hi\"\n","n":3,"x":1.500,"list":[1,true,null,{}]})");
}

TEST(Api, SolvesWithEveryAlgorithm) {
  for (const auto* algorithm : {"logic", "smart", "naive", "dlx"}) {
    const std::string json = api::solve(kClassic, algorithm, 1000);
    EXPECT_TRUE(has(json, R"("ok":true)")) << algorithm;
    EXPECT_TRUE(has(json, R"("status":"solved")")) << algorithm;
    EXPECT_EQ(stringField(json, "solution"), testing::kClassicSolution) << algorithm;
  }
  const std::string logic = api::solve(kClassic, "logic", 0);
  EXPECT_TRUE(has(logic, R"("kind":"logic")"));
  EXPECT_TRUE(has(logic, R"("technique":"hidden-single")"));
  EXPECT_TRUE(has(logic, R"("level":"medium")"));
}

TEST(Api, ReportsBadRequests) {
  EXPECT_TRUE(has(api::solve("123", "smart", 0), R"("ok":false)"));
  EXPECT_TRUE(has(api::solve(kClassic, "quantum", 0), R"("ok":false)"));
  EXPECT_TRUE(has(api::generate("impossible", "1"), R"("ok":false)"));
  EXPECT_TRUE(has(api::hint(kClassic, "1"), R"("ok":false)"));
}

TEST(Api, GeneratesTheSamePuzzleForTheSameSeed) {
  const std::string a = api::generate("hard", "duel-123");
  const std::string b = api::generate("hard", "duel-123");
  EXPECT_EQ(stringField(a, "puzzle"), stringField(b, "puzzle"));
  EXPECT_EQ(stringField(a, "puzzle").size(), 81u);
  EXPECT_TRUE(has(a, R"("level":"hard")"));
  EXPECT_TRUE(has(a, R"("seed":"duel-123")"));
}

TEST(Api, AnalyzesValidityAndUniqueness) {
  EXPECT_TRUE(has(api::analyze(kClassic), R"("solutions":1)"));
  EXPECT_TRUE(has(api::analyze(kClassic), R"("rating":{"level":"medium")"));
  EXPECT_TRUE(has(api::analyze(std::string(81, '.')), R"("solutions":2)"));
  const std::string conflict = api::analyze("11" + std::string(79, '.'));
  EXPECT_TRUE(has(conflict, R"("valid":false)"));
  EXPECT_TRUE(has(conflict, R"("conflicts":[0,1])"));
}

TEST(Api, HintsMistakesFirstThenTheNextDeduction) {
  std::string current = kClassic;
  current[2] = '9';  // r1c3 is 4 in the solution
  const std::string mistakes = api::hint(kClassic, current);
  EXPECT_TRUE(has(mistakes, R"("kind":"mistakes")"));
  EXPECT_TRUE(has(mistakes, R"("cells":[2])"));

  const std::string step = api::hint(kClassic, kClassic);
  EXPECT_TRUE(has(step, R"("kind":"step")"));
  EXPECT_TRUE(has(step, R"("place":[[)"));

  const std::string solved = api::hint(kClassic, std::string(testing::kClassicSolution));
  EXPECT_TRUE(has(solved, R"("kind":"solved")"));
}

TEST(Api, BenchmarksEveryAlgorithm) {
  const std::string json = api::benchmark(kClassic);
  for (const auto* algorithm : {"dlx", "smart", "logic", "naive"}) {
    EXPECT_TRUE(has(json, std::string(R"("algorithm":")") + algorithm + "\"")) << algorithm;
  }
}

}  // namespace
}  // namespace sudoku
