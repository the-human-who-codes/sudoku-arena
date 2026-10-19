#pragma once

#include <cmath>
#include <concepts>
#include <cstdio>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sudoku::detail {

/// Minimal streaming JSON writer, enough for the engine's API responses.
class JsonWriter {
 public:
  JsonWriter& beginObject() { return open('{'); }
  JsonWriter& endObject() { return close('}'); }
  JsonWriter& beginArray() { return open('['); }
  JsonWriter& endArray() { return close(']'); }

  JsonWriter& key(std::string_view name) {
    separate();
    writeString(name);
    out_ += ':';
    afterKey_ = true;
    return *this;
  }

  JsonWriter& value(std::string_view text) {
    separate();
    writeString(text);
    return *this;
  }
  JsonWriter& value(const char* text) { return value(std::string_view(text)); }
  JsonWriter& value(const std::string& text) { return value(std::string_view(text)); }

  JsonWriter& value(bool flag) {
    separate();
    out_ += flag ? "true" : "false";
    return *this;
  }

  template <std::integral T>
    requires(!std::same_as<T, bool>)
  JsonWriter& value(T number) {
    separate();
    out_ += std::to_string(number);
    return *this;
  }

  JsonWriter& value(double number) {
    separate();
    if (!std::isfinite(number)) {
      out_ += "null";
    } else {
      char buffer[32];
      std::snprintf(buffer, sizeof buffer, "%.3f", number);
      out_ += buffer;
    }
    return *this;
  }

  JsonWriter& null() {
    separate();
    out_ += "null";
    return *this;
  }

  template <typename T>
  JsonWriter& field(std::string_view name, const T& v) {
    key(name);
    return value(v);
  }

  std::string take() { return std::move(out_); }

 private:
  JsonWriter& open(char bracket) {
    separate();
    out_ += bracket;
    first_.push_back(true);
    return *this;
  }

  JsonWriter& close(char bracket) {
    out_ += bracket;
    first_.pop_back();
    return *this;
  }

  // Emits the comma between siblings; a value directly after its key needs none.
  void separate() {
    if (afterKey_) {
      afterKey_ = false;
      return;
    }
    if (first_.empty()) return;
    if (!first_.back()) out_ += ',';
    first_.back() = false;
  }

  void writeString(std::string_view text) {
    out_ += '"';
    for (const char ch : text) {
      switch (ch) {
        case '"': out_ += "\\\""; break;
        case '\\': out_ += "\\\\"; break;
        case '\n': out_ += "\\n"; break;
        case '\r': out_ += "\\r"; break;
        case '\t': out_ += "\\t"; break;
        default:
          if (static_cast<unsigned char>(ch) < 0x20) {
            char buffer[8];
            std::snprintf(buffer, sizeof buffer, "\\u%04x", static_cast<unsigned>(ch));
            out_ += buffer;
          } else {
            out_ += ch;
          }
      }
    }
    out_ += '"';
  }

  std::string out_;
  std::vector<bool> first_;
  bool afterKey_ = false;
};

}  // namespace sudoku::detail
