#include "snake/snake_operations.hpp"

#include <algorithm>

namespace snake {

// ============================================================================
// Basic Snake Utilities
// ============================================================================

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

// ============================================================================
// Snake Transformations
// ============================================================================

Snake moveSnake(Snake snake, const Board& board) {
  if (!snake.alive) return snake;

  Point new_head = getNextHeadPosition(snake);
  new_head = wrapPoint(new_head, board);

  // Build new tail: old head + old tail (minus last)
  std::vector<Point> new_tail;
  new_tail.reserve(snake.tail.size() + 1);
  new_tail.push_back(snake.head);
  new_tail.insert(new_tail.end(), snake.tail.begin(), snake.tail.end());
  if (!new_tail.empty()) {
    new_tail.pop_back();
  }

  snake.head = new_head;
  snake.tail = std::move(new_tail);
  return snake;
}

Snake growSnake(Snake snake, const Board& board) {
  if (!snake.alive) return snake;

  Point new_head = getNextHeadPosition(snake);
  new_head = wrapPoint(new_head, board);

  // Build new tail: old head + old tail (keep all)
  std::vector<Point> new_tail;
  new_tail.reserve(snake.tail.size() + 1);
  new_tail.push_back(snake.head);
  new_tail.insert(new_tail.end(), snake.tail.begin(), snake.tail.end());

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
// Game Logic Operations on Snakes
// ============================================================================

PerPlayerSnakes applyDirectionChanges(PerPlayerSnakes snakes, const PerPlayerDirection& consumed_directions) {
  for (const auto& [player_id, direction] : consumed_directions) {
    auto it = snakes.find(player_id);
    if (it != snakes.end()) {
      it->second.current_direction = direction;
    }
  }
  return snakes;
}

PerPlayerSnakes moveSnakes(PerPlayerSnakes snakes, const Board& board, const std::vector<Point>& food_items) {
  for (auto& [player_id, snake] : snakes) {
    if (!snake.alive) continue;

    // Calculate next head position to check if landing on food
    Point next_head = getNextHeadPosition(snake);
    next_head = wrapPoint(next_head, board);

    // Check if landing on food
    bool will_eat = std::find(food_items.begin(), food_items.end(), next_head) != food_items.end();

    // Grow if eating, move otherwise
    snake = will_eat ? growSnake(snake, board) : moveSnake(snake, board);
  }

  return snakes;
}

std::tuple<PerPlayerSnakes, PerPlayerScores, std::vector<Point>> handleCollisions(PerPlayerSnakes snakes,
                                                                                  PerPlayerScores scores) {
  std::vector<Point> cut_tails;  // Collect cut tail segments

  if (snakes.size() < 2) {
    return {snakes, scores, cut_tails};
  }

  // Get both snakes (hardcoded for 2-player)
  auto it1 = snakes.find("player1");
  auto it2 = snakes.find("player2");

  if (it1 == snakes.end() || it2 == snakes.end()) {
    return {snakes, scores, cut_tails};
  }

  Snake& snake_a = it1->second;
  Snake& snake_b = it2->second;

  if (!snake_a.alive || !snake_b.alive) {
    return {snakes, scores, cut_tails};
  }

  // Check collision cases
  if (bothBiteEachOther(snake_a, snake_b)) {
    // Both die
    snake_a.alive = false;
    snake_b.alive = false;
    scores["player1"] -= 10;
    scores["player2"] -= 10;
  } else if (firstBitesSecond(snake_a, snake_b)) {
    // Snake A bites B - cut B's tail
    scores["player2"] -= 10;
    auto [new_snake, cut] = cutSnakeTailAt(snake_b, snake_a.head);
    snake_b = new_snake;
    cut_tails.insert(cut_tails.end(), cut.begin(), cut.end());
  } else if (firstBitesSecond(snake_b, snake_a)) {
    // Snake B bites A - cut A's tail
    scores["player1"] -= 10;
    auto [new_snake, cut] = cutSnakeTailAt(snake_a, snake_b.head);
    snake_a = new_snake;
    cut_tails.insert(cut_tails.end(), cut.begin(), cut.end());
  }

  return {snakes, scores, cut_tails};
}

}  // namespace snake
