#include "snake/snake_operations.hpp"

#include <algorithm>
#include <iterator>

#include "snake/control_messages.hpp"

namespace snake {

// ============================================================================
// Single-Snake Predicates
// ============================================================================

bool snakeBitesItself(const Snake& snake) {
  return std::find(snake.tail().begin(), snake.tail().end(), snake.head()) != snake.tail().end();
}

// ============================================================================
// Inter-Snake Predicates
// ============================================================================

bool firstBitesSecond(const Snake& first, const Snake& second) {
  const Point& attacker_head = first.head();

  if (attacker_head == second.head()) {
    return true;
  }

  return std::find(second.tail().begin(), second.tail().end(), attacker_head) != second.tail().end();
}

bool bothBiteEachOther(const Snake& a, const Snake& b) { return firstBitesSecond(a, b) && firstBitesSecond(b, a); }

// ============================================================================
// Batch Utilities
// ============================================================================

PerPlayerAliveStates extractAliveStates(const PerPlayerSnakes& snakes) {
  PerPlayerAliveStates alive_states;
  std::transform(
      snakes.begin(), snakes.end(), std::inserter(alive_states, alive_states.begin()), [](const auto& entry) {
        return std::make_pair(entry.first, entry.second.alive());
      });
  return alive_states;
}

// ============================================================================
// Player Initialization
// ============================================================================

std::tuple<PerPlayerSnakes, PerPlayerScores> addPlayer(PlayerId player_id,
                                                       Point start_position,
                                                       Direction initial_direction,
                                                       int snake_length,
                                                       PerPlayerSnakes snakes,
                                                       PerPlayerScores scores) {
  snakes[player_id] = snake_model::initial(start_position, initial_direction, snake_length);
  scores[player_id] = 0;
  return {std::move(snakes), std::move(scores)};
}

// ============================================================================
// Game Logic Operations on Snakes
// ============================================================================

PerPlayerSnakes applyDirectionMsgs(PerPlayerSnakes snakes, const PerPlayerDirection& consumed_directions) {
  for (const auto& [player_id, direction] : consumed_directions) {
    auto it = snakes.find(player_id);
    if (it != snakes.end()) {
      it->second = snake_model::setDirection(it->second, direction);
    }
  }
  return snakes;
}

PerPlayerSnakes moveSnakes(PerPlayerSnakes snakes, const Board& board, const FoodItems& food_items) {
  for (auto& [player_id, snake] : snakes) {
    if (!snake.alive()) continue;

    Point next_head = snake_model::nextHead(snake, board);
    bool is_eating = std::find(food_items.begin(), food_items.end(), next_head) != food_items.end();

    if (is_eating) {
      snake = snake_model::grow(snake, board);
    } else {
      snake = snake_model::move(snake, board);
    }
  }

  return snakes;
}

namespace {

std::tuple<PerPlayerSnakes, PerPlayerScores> handleSelfBites(PerPlayerSnakes snakes, PerPlayerScores scores) {
  for (auto& [player_id, snake] : snakes) {
    if (snake.alive() && snakeBitesItself(snake)) {
      snake = snake_model::kill(snake);
      scores[player_id] -= 10;
    }
  }
  return {snakes, scores};
}

}  // namespace

std::tuple<PerPlayerSnakes, PerPlayerScores, std::vector<Point>> handleCollisions(PerPlayerSnakes snakes,
                                                                                  PerPlayerScores scores) {
  std::vector<Point> cut_tails;

  std::tie(snakes, scores) = handleSelfBites(snakes, scores);

  if (snakes.size() < 2) {
    return {snakes, scores, cut_tails};
  }

  auto it1 = snakes.find(PLAYER_A);
  auto it2 = snakes.find(PLAYER_B);

  if (it1 == snakes.end() || it2 == snakes.end()) {
    return {snakes, scores, cut_tails};
  }

  Snake& snake_a = it1->second;
  Snake& snake_b = it2->second;

  if (!snake_a.alive() || !snake_b.alive()) {
    return {snakes, scores, cut_tails};
  }

  if (bothBiteEachOther(snake_a, snake_b)) {
    snake_a = snake_model::kill(snake_a);
    snake_b = snake_model::kill(snake_b);
    scores[PLAYER_A] -= 10;
    scores[PLAYER_B] -= 10;
  } else if (firstBitesSecond(snake_a, snake_b)) {
    scores[PLAYER_B] -= 10;
    auto [new_snake, cut] = snake_model::cutAt(snake_b, snake_a.head());
    snake_b = new_snake;
    cut_tails.insert(cut_tails.end(), cut.begin(), cut.end());
  } else if (firstBitesSecond(snake_b, snake_a)) {
    scores[PLAYER_A] -= 10;
    auto [new_snake, cut] = snake_model::cutAt(snake_a, snake_b.head());
    snake_a = new_snake;
    cut_tails.insert(cut_tails.end(), cut.begin(), cut.end());
  }

  return {snakes, scores, cut_tails};
}

}  // namespace snake
