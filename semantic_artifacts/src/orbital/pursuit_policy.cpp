#include "orbital/pursuit_policy.hpp"

#include <cstdlib>

namespace snake {
namespace orbital {
namespace pursuit_policy {

namespace {

bool isHorizontal(Direction dir) { return dir == Direction::LEFT || dir == Direction::RIGHT; }

// Steps needed when moving in the positive direction of an axis, wrapping at its size
int forwardDistance(int from, int to, int size) { return ((to - from) % size + size) % size; }

// Steps needed to align with the target along the heading, moving only forwards
int stepsAlongHeading(Point head, Direction heading, Point target, const Board& board) {
  switch (heading) {
    case Direction::RIGHT:
      return forwardDistance(head.x, target.x, board.width);
    case Direction::LEFT:
      return forwardDistance(target.x, head.x, board.width);
    case Direction::DOWN:
      return forwardDistance(head.y, target.y, board.height);
    case Direction::UP:
      return forwardDistance(target.y, head.y, board.height);
  }
  return 0;
}

struct FinalLeg {
  Direction direction;
  int steps;
};

// Across the heading's axis towards the target as seen on screen, without wrapping
FinalLeg finalLeg(Point head, Direction heading, Point target) {
  if (isHorizontal(heading)) {
    const int offset = target.y - head.y;
    return {offset < 0 ? Direction::UP : Direction::DOWN, std::abs(offset)};
  }
  const int offset = target.x - head.x;
  return {offset < 0 ? Direction::LEFT : Direction::RIGHT, std::abs(offset)};
}

}  // namespace

std::vector<Direction> planRoute(Point head, Direction heading, Point target, const Board& board) {
  const int first_steps = stepsAlongHeading(head, heading, target, board);
  const FinalLeg final_leg = finalLeg(head, heading, target);

  std::vector<Direction> route(first_steps, heading);
  route.insert(route.end(), final_leg.steps, final_leg.direction);
  return route;
}

}  // namespace pursuit_policy
}  // namespace orbital
}  // namespace snake
