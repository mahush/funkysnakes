#include "snake/snake_predicates.hpp"

#include <algorithm>

#include "snake/snake_model.hpp"

namespace snake {

bool snakeBitesItself(const Snake& snake) {
  return std::find(snake_model::tail(snake).begin(), snake_model::tail(snake).end(), snake_model::head(snake)) !=
         snake_model::tail(snake).end();
}

bool firstBitesSecond(const Snake& first, const Snake& second) {
  const Point& attacker_head = snake_model::head(first);

  if (attacker_head == snake_model::head(second)) {
    return true;
  }

  return std::find(snake_model::tail(second).begin(), snake_model::tail(second).end(), attacker_head) !=
         snake_model::tail(second).end();
}

bool bothBiteEachOther(const Snake& a, const Snake& b) { return firstBitesSecond(a, b) && firstBitesSecond(b, a); }

}  // namespace snake
