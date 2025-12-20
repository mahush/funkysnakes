#pragma once

namespace snake {

// Message types for actor communication

struct MsgAdd {
  int a;
  int b;
};

struct MsgResult {
  int value;
};

}  // namespace snake
