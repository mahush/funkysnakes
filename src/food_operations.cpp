#include "snake/food_operations.hpp"

#include <algorithm>

namespace snake {

// ============================================================================
// Food Position Generation
// ============================================================================

Point generateRandomFoodPosition(const Board& board, const PerPlayerSnakes& snakes, RandomIntFn random_int) {
  // Try to find a position not occupied by snakes
  // Max 100 attempts to avoid infinite loop
  for (int attempt = 0; attempt < 100; ++attempt) {
    Point candidate{random_int(0, board.width - 1), random_int(0, board.height - 1)};

    // Check if position is occupied by any snake
    bool occupied = false;
    for (const auto& [player_id, snake] : snakes) {
      if (snake.head == candidate) {
        occupied = true;
        break;
      }
      for (const Point& segment : snake.tail) {
        if (segment == candidate) {
          occupied = true;
          break;
        }
      }
      if (occupied) break;
    }

    if (!occupied) {
      return candidate;
    }
  }

  // If all attempts failed, just return a random position
  return {random_int(0, board.width - 1), random_int(0, board.height - 1)};
}

// ============================================================================
// Food Transformation Operations
// ============================================================================

FoodItems dropCutTailsAsFood(FoodItems food_items, const FoodItems& cut_tails) {
  food_items.insert(food_items.end(), cut_tails.begin(), cut_tails.end());
  return food_items;
}

FoodItems dropDeadSnakesAsFood(FoodItems food_items, const PerPlayerSnakes& snakes) {
  for (const auto& [player_id, snake] : snakes) {
    if (!snake.alive) {
      // Add entire snake body as food
      auto body = snake.toBody();
      food_items.insert(food_items.end(), body.begin(), body.end());
    }
  }

  return food_items;
}

std::tuple<FoodItems, PerPlayerScores> handleFoodEating(FoodItems food_items, PerPlayerScores scores,
                                                                  const PerPlayerSnakes& snakes) {
  for (const auto& [player_id, snake] : snakes) {
    if (!snake.alive) continue;

    // Check if snake head is on food
    auto it = std::find(food_items.begin(), food_items.end(), snake.head);

    if (it != food_items.end()) {
      // Remove eaten food
      food_items.erase(it);

      // Award points
      scores[player_id] += 10;
    }
  }

  return {std::move(food_items), std::move(scores)};
}

FoodItems replenishFood(RandomIntFn random_int, int target_count, FoodItems food_items,
                                 const Board& board, const PerPlayerSnakes& snakes) {
  if (food_items.size() >= static_cast<size_t>(target_count)) {
    return food_items;
  }

  // Add food items until we reach target count
  while (food_items.size() < static_cast<size_t>(target_count)) {
    Point new_food_pos = generateRandomFoodPosition(board, snakes, random_int);
    food_items.push_back(new_food_pos);
  }

  return food_items;
}

FoodItems updateFoodPositions(RandomIntFn random_int, int tick_count, FoodItems food_items,
                                       const Board& board, const PerPlayerSnakes& snakes) {
  // Only update every 15 ticks
  if (tick_count % 15 != 0 || food_items.empty()) {
    return food_items;
  }

  // Pick random food to move
  int food_index = random_int(0, food_items.size() - 1);

  // Replace with new position
  food_items[food_index] = generateRandomFoodPosition(board, snakes, random_int);

  return food_items;
}

}  // namespace snake
