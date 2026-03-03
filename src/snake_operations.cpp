#include "snake/snake_operations.hpp"

#include <algorithm>
#include <iterator>

namespace snake {

// ============================================================================
// Basic Snake Utilities
// ============================================================================

PerPlayerAliveStates extractAliveStates(const PerPlayerSnakes& snakes) {
  PerPlayerAliveStates alive_states;
  std::transform(
      snakes.begin(), snakes.end(), std::inserter(alive_states, alive_states.begin()), [](const auto& entry) {
        return std::make_pair(entry.first, entry.second.alive);
      });
  return alive_states;
}

Point getNextHeadPosition(const Snake& snake) {
  Point head = snake.head;
  Point next = head;

  switch (snake.current_direction) {
    case Direction::LEFT:
      next.x = head.x - 1;
      break;
    case Direction::RIGHT:
      next.x = head.x + 1;
      break;
    case Direction::UP:
      next.y = head.y - 1;
      break;
    case Direction::DOWN:
      next.y = head.y + 1;
      break;
  }

  return next;
}

Point wrapPoint(Point p, const Board& board) {
  if (p.x < 0) {
    p.x = board.width - 1;
  } else if (p.x >= board.width) {
    p.x = 0;
  }

  if (p.y < 0) {
    p.y = board.height - 1;
  } else if (p.y >= board.height) {
    p.y = 0;
  }

  return p;
}

bool snakeBitesItself(const Snake& snake) {
  // Check if head position is anywhere in the tail
  return std::find(snake.tail.begin(), snake.tail.end(), snake.head) != snake.tail.end();
}

// ============================================================================
// Snake Transformations
// ============================================================================

Snake moveSnake(Snake snake, const Board& board, bool is_eating) {
  if (!snake.alive) return snake;

  Point new_head = getNextHeadPosition(snake);
  new_head = wrapPoint(new_head, board);

  // Build new tail: old head + old tail
  std::vector<Point> new_tail;
  new_tail.reserve(snake.tail.size() + 1);
  new_tail.push_back(snake.head);
  new_tail.insert(new_tail.end(), snake.tail.begin(), snake.tail.end());

  // Remove last segment if not eating (maintains length)
  if (!is_eating && !new_tail.empty()) {
    new_tail.pop_back();
  }

  snake.head = new_head;
  snake.tail = std::move(new_tail);
  return snake;
}

std::tuple<Snake, std::vector<Point>> cutSnakeTailAt(Snake snake, Point cut_point) {
  std::vector<Point> cut_segments;

  if (snake.head == cut_point) {
    return {snake, cut_segments};
  }

  auto it = std::find(snake.tail.begin(), snake.tail.end(), cut_point);
  if (it != snake.tail.end()) {
    // Extract cut segments
    cut_segments = std::vector<Point>(it, snake.tail.end());
    // Keep only the part before cut point
    snake.tail = std::vector<Point>(snake.tail.begin(), it);
  }

  return {snake, cut_segments};
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
  // Create snake tail extending backwards from start position
  Point head = start_position;
  std::vector<Point> tail;
  for (int i = 1; i < snake_length; ++i) {
    // Extend tail in opposite direction of initial movement
    Point tail_segment = start_position;
    switch (initial_direction) {
      case Direction::RIGHT:
        tail_segment.x = start_position.x - i;
        break;
      case Direction::LEFT:
        tail_segment.x = start_position.x + i;
        break;
      case Direction::DOWN:
        tail_segment.y = start_position.y - i;
        break;
      case Direction::UP:
        tail_segment.y = start_position.y + i;
        break;
    }
    tail.push_back(tail_segment);
  }

  // Create and add snake
  Snake snake{head, tail, initial_direction, true};
  snakes[player_id] = snake;

  // Initialize score
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
      it->second.current_direction = direction;
    }
  }
  return snakes;
}

PerPlayerSnakes moveSnakes(PerPlayerSnakes snakes, const Board& board, const FoodItems& food_items) {
  for (auto& [player_id, snake] : snakes) {
    if (!snake.alive) continue;

    // Calculate next head position to check if landing on food
    Point next_head = getNextHeadPosition(snake);
    next_head = wrapPoint(next_head, board);

    // Check if landing on food
    bool is_eating = std::find(food_items.begin(), food_items.end(), next_head) != food_items.end();

    // Move snake (grows if eating, maintains length otherwise)
    snake = moveSnake(snake, board, is_eating);
  }

  return snakes;
}

namespace {

/**
 * @brief Handle self-bite collisions for all snakes
 *
 * Pure function that checks each snake for self-collision and returns
 * updated snakes and scores.
 *
 * @param snakes Snakes to check (by value)
 * @param scores Scores (by value)
 * @return Tuple of (updated snakes, updated scores)
 */
std::tuple<PerPlayerSnakes, PerPlayerScores> handleSelfBites(PerPlayerSnakes snakes, PerPlayerScores scores) {
  for (auto& [player_id, snake] : snakes) {
    if (snake.alive && snakeBitesItself(snake)) {
      snake.alive = false;
      scores[player_id] -= 10;
    }
  }
  return {snakes, scores};
}

}  // namespace

std::tuple<PerPlayerSnakes, PerPlayerScores, std::vector<Point>> handleCollisions(PerPlayerSnakes snakes,
                                                                                  PerPlayerScores scores) {
  std::vector<Point> cut_tails;  // Collect cut tail segments

  // Handle self-bites first
  std::tie(snakes, scores) = handleSelfBites(snakes, scores);

  if (snakes.size() < 2) {
    return {snakes, scores, cut_tails};
  }

  // Get both snakes (hardcoded for 2-player)
  auto it1 = snakes.find(PLAYER_A);
  auto it2 = snakes.find(PLAYER_B);

  if (it1 == snakes.end() || it2 == snakes.end()) {
    return {snakes, scores, cut_tails};
  }

  Snake& snake_a = it1->second;
  Snake& snake_b = it2->second;

  if (!snake_a.alive || !snake_b.alive) {
    return {snakes, scores, cut_tails};
  }

  // Check inter-snake collision cases
  if (bothBiteEachOther(snake_a, snake_b)) {
    // Both die
    snake_a.alive = false;
    snake_b.alive = false;
    scores[PLAYER_A] -= 10;
    scores[PLAYER_B] -= 10;
  } else if (firstBitesSecond(snake_a, snake_b)) {
    // Snake A bites B - cut B's tail
    scores[PLAYER_B] -= 10;
    auto [new_snake, cut] = cutSnakeTailAt(snake_b, snake_a.head);
    snake_b = new_snake;
    cut_tails.insert(cut_tails.end(), cut.begin(), cut.end());
  } else if (firstBitesSecond(snake_b, snake_a)) {
    // Snake B bites A - cut A's tail
    scores[PLAYER_A] -= 10;
    auto [new_snake, cut] = cutSnakeTailAt(snake_a, snake_b.head);
    snake_a = new_snake;
    cut_tails.insert(cut_tails.end(), cut.begin(), cut.end());
  }

  return {snakes, scores, cut_tails};
}

}  // namespace snake
