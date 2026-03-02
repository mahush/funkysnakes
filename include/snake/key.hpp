#pragma once

#include <variant>

namespace snake {

// Special keys that aren't regular characters
enum class SpecialKey { ARROW_UP, ARROW_DOWN, ARROW_LEFT, ARROW_RIGHT };

// A key is either a regular character or a special key
// Represents a logical key after parsing escape sequences
using Key = std::variant<char, SpecialKey>;

}  // namespace snake
