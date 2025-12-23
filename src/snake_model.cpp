#include "snake/snake_model.hpp"

#include <algorithm>

#include "snake/functional_utils.hpp"

namespace snake {

Point getHead(const Snake& snake) { return snake.head; }

Snake cutTailAt(const Snake& snake, const Point& hitPoint) {
  // If hit point is the head, can't cut (would have no snake left)
  if (snake.head == hitPoint) {
    return snake;
  }

  // Search in tail
  auto it = std::find(snake.tail.begin(), snake.tail.end(), hitPoint);
  if (it == snake.tail.end()) {
    return snake;  // Point not found in tail, return unchanged
  }

  // Cut tail at this point (everything before it)
  return Snake{snake.head, std::vector<Point>(snake.tail.begin(), it)};
}

bool firstBitesSecond(const Snake& first, const Snake& second) {
  const Point& attackerHead = first.head;

  // Check if attacker's head hits victim's head
  if (attackerHead == second.head) {
    return true;
  }

  // Check if attacker's head hits any point in victim's tail
  return std::find(second.tail.begin(), second.tail.end(), attackerHead) != second.tail.end();
}

SnakePair applyBiteRule(SnakePair s) {
  auto rule = cond(
      // Otherwise / default case: return unchanged
      [](SnakePair snakes) { return snakes; },

      // Case 1: Snake A bites Snake B → cut B at A's head position
      Case<SnakePair>{[](const SnakePair& snakes) { return firstBitesSecond(snakes.a, snakes.b); },
                      [](SnakePair snakes) {
                        snakes.b = cutTailAt(snakes.b, getHead(snakes.a));
                        return snakes;
                      }},

      // Case 2: Snake B bites Snake A → cut A at B's head position
      Case<SnakePair>{[](const SnakePair& snakes) { return firstBitesSecond(snakes.b, snakes.a); },
                      [](SnakePair snakes) {
                        snakes.a = cutTailAt(snakes.a, getHead(snakes.b));
                        return snakes;
                      }});

  return rule(s);
}

}  // namespace snake
