#include "snake/game_logic.hpp"

#include <algorithm>
#include <iterator>

#include "snake/control_messages.hpp"
#include "snake/snake_model_evolve.hpp"
#include "snake/snake_predicates.hpp"

namespace snake {

// ============================================================================
// Batch Utilities
// ============================================================================

PerPlayerAliveStates extractAliveStates(const PerPlayerSnakes& snakes) {
  PerPlayerAliveStates alive_states;
  std::transform(
      snakes.begin(), snakes.end(), std::inserter(alive_states, alive_states.begin()), [](const auto& entry) {
        return std::make_pair(entry.first, snake_model::alive(entry.second));
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
// Snake Game Logic
// ============================================================================

PerPlayerSnakes moveSnakes(PerPlayerSnakes snakes,
                           const Board& board,
                           const FoodItems& food_items,
                           const PerPlayerDirection& consumed_directions) {
  for (auto& [player_id, snake] : snakes) {
    if (!snake_model::alive(snake)) continue;

    auto dir_it = consumed_directions.find(player_id);
    Direction dir = (dir_it != consumed_directions.end()) ? dir_it->second : snake_model::currentDirection(snake);

    Point next_head = snake_model::nextHead(snake, dir, board);
    bool is_eating = std::find(food_items.begin(), food_items.end(), next_head) != food_items.end();

    if (is_eating) {
      snake = snake_model::grow(snake, dir, board);
    } else {
      snake = snake_model::move(snake, dir, board);
    }
  }

  return snakes;
}

namespace {

std::tuple<PerPlayerSnakes, PerPlayerScores> handleSelfBites(PerPlayerSnakes snakes, PerPlayerScores scores) {
  for (auto& [player_id, snake] : snakes) {
    if (snake_model::alive(snake) && snakeBitesItself(snake)) {
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

  if (!snake_model::alive(snake_a) || !snake_model::alive(snake_b)) {
    return {snakes, scores, cut_tails};
  }

  if (bothBiteEachOther(snake_a, snake_b)) {
    snake_a = snake_model::kill(snake_a);
    snake_b = snake_model::kill(snake_b);
    scores[PLAYER_A] -= 10;
    scores[PLAYER_B] -= 10;
  } else if (firstBitesSecond(snake_a, snake_b)) {
    scores[PLAYER_B] -= 10;
    auto [new_snake, cut] = snake_model::cutAt(snake_b, snake_model::head(snake_a));
    snake_b = new_snake;
    cut_tails.insert(cut_tails.end(), cut.begin(), cut.end());
  } else if (firstBitesSecond(snake_b, snake_a)) {
    scores[PLAYER_A] -= 10;
    auto [new_snake, cut] = snake_model::cutAt(snake_a, snake_model::head(snake_b));
    snake_a = new_snake;
    cut_tails.insert(cut_tails.end(), cut.begin(), cut.end());
  }

  return {snakes, scores, cut_tails};
}

// ============================================================================
// Food Logic
// ============================================================================

namespace {

bool containsPoint(const FoodItems& food_items, Point p) {
  return std::find(food_items.begin(), food_items.end(), p) != food_items.end();
}

// Enforces one food item per cell
FoodItems addFoodIfFree(FoodItems food_items, Point p) {
  if (!containsPoint(food_items, p)) {
    food_items.push_back(p);
  }
  return food_items;
}

}  // namespace

std::optional<Point> generateRandomFoodPosition(const Board& board,
                                                const PerPlayerSnakes& snakes,
                                                const FoodItems& food_items,
                                                RandomIntGeneratorFn random_int) {
  for (int attempt = 0; attempt < 100; ++attempt) {
    Point candidate{random_int(0, board.width - 1), random_int(0, board.height - 1)};

    bool occupied_by_snake = std::any_of(snakes.begin(), snakes.end(), [&candidate](const auto& entry) {
      const Snake& snake = entry.second;
      const std::vector<Point>& tail = snake_model::tail(snake);
      return snake_model::head(snake) == candidate || std::find(tail.begin(), tail.end(), candidate) != tail.end();
    });
    bool occupied_by_food = containsPoint(food_items, candidate);

    if (!occupied_by_snake && !occupied_by_food) {
      return candidate;
    }
  }

  return std::nullopt;
}

FoodItems dropCutTailsAsFood(FoodItems food_items, const FoodItems& cut_tails) {
  for (const Point& segment : cut_tails) {
    food_items = addFoodIfFree(std::move(food_items), segment);
  }
  return food_items;
}

FoodItems dropDeadSnakesAsFood(const PerPlayerAliveStates& alive_before_step,
                               FoodItems food_items,
                               const PerPlayerSnakes& snakes) {
  for (const auto& [player_id, snake] : snakes) {
    auto before_it = alive_before_step.find(player_id);
    bool died_this_step = before_it != alive_before_step.end() && before_it->second && !snake_model::alive(snake);
    if (died_this_step) {
      food_items = addFoodIfFree(std::move(food_items), snake_model::head(snake));
      for (const Point& segment : snake_model::tail(snake)) {
        food_items = addFoodIfFree(std::move(food_items), segment);
      }
    }
  }

  return food_items;
}

std::tuple<FoodItems, PerPlayerScores> handleFoodEating(FoodItems food_items,
                                                        PerPlayerScores scores,
                                                        const PerPlayerSnakes& snakes) {
  for (const auto& [player_id, snake] : snakes) {
    if (!snake_model::alive(snake)) continue;

    auto it = std::find(food_items.begin(), food_items.end(), snake_model::head(snake));

    if (it != food_items.end()) {
      food_items.erase(it);
      scores[player_id] += 10;
    }
  }

  return {std::move(food_items), std::move(scores)};
}

FoodItems initializeFood(RandomIntGeneratorFn random_int,
                         int count,
                         FoodItems /* food_items */,
                         const Board& board,
                         const PerPlayerSnakes& snakes) {
  return replenishFood(random_int, count, {}, board, snakes);
}

FoodItems replenishFood(RandomIntGeneratorFn random_int,
                        int target_count,
                        FoodItems food_items,
                        const Board& board,
                        const PerPlayerSnakes& snakes) {
  if (food_items.size() >= static_cast<size_t>(target_count)) {
    return food_items;
  }

  while (food_items.size() < static_cast<size_t>(target_count)) {
    std::optional<Point> new_food_pos = generateRandomFoodPosition(board, snakes, food_items, random_int);
    if (!new_food_pos) {
      break;  // Board too crowded: try again next step
    }
    food_items.push_back(*new_food_pos);
  }

  return food_items;
}

FoodItems repositionRandomFood(RandomIntGeneratorFn random_int,
                               FoodItems food_items,
                               const Board& board,
                               const PerPlayerSnakes& snakes) {
  if (food_items.empty()) {
    return food_items;
  }

  int food_index = random_int(0, food_items.size() - 1);
  std::optional<Point> new_position = generateRandomFoodPosition(board, snakes, food_items, random_int);
  if (new_position) {
    food_items[food_index] = *new_position;
  }

  return food_items;
}

}  // namespace snake
