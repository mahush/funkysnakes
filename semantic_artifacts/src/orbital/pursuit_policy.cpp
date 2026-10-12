#include "orbital/pursuit_policy.hpp"

namespace snake {
namespace orbital {
namespace pursuit_policy {

namespace {

bool isHorizontal(Direction dir) { return dir == Direction::LEFT || dir == Direction::RIGHT; }

// Clockwise neighbour as seen on screen (y grows downwards)
Direction clockwise(Direction dir) {
  switch (dir) {
    case Direction::UP:
      return Direction::RIGHT;
    case Direction::RIGHT:
      return Direction::DOWN;
    case Direction::DOWN:
      return Direction::LEFT;
    case Direction::LEFT:
      return Direction::UP;
  }
  return dir;
}

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

// Shorter wrapped way across the heading's axis; equal ways turn clockwise
FinalLeg finalLeg(Point head, Direction heading, Point target, const Board& board) {
  const bool horizontal = isHorizontal(heading);
  const int size = horizontal ? board.height : board.width;
  const int positive_steps =
      horizontal ? forwardDistance(head.y, target.y, size) : forwardDistance(head.x, target.x, size);
  const int negative_steps = positive_steps == 0 ? 0 : size - positive_steps;
  const Direction positive = horizontal ? Direction::DOWN : Direction::RIGHT;
  const Direction negative = horizontal ? Direction::UP : Direction::LEFT;

  if (positive_steps < negative_steps) {
    return {positive, positive_steps};
  }
  if (negative_steps < positive_steps) {
    return {negative, negative_steps};
  }
  return {clockwise(heading), positive_steps};
}

}  // namespace

std::vector<Direction> planRoute(Point head, Direction heading, Point target, const Board& board) {
  const int first_steps = stepsAlongHeading(head, heading, target, board);
  const FinalLeg final_leg = finalLeg(head, heading, target, board);

  std::vector<Direction> route(first_steps, heading);
  route.insert(route.end(), final_leg.steps, final_leg.direction);
  return route;
}

}  // namespace pursuit_policy
}  // namespace orbital
}  // namespace snake
