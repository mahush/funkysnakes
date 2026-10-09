#include "snake/game_logic.hpp"

#include <algorithm>
#include <iterator>

#include "classic/snake_model_evolve.hpp"
#include "snake/control_messages.hpp"
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

// Appends a snake's complete body (head first) to the dropped segments
std::vector<Point> appendBody(std::vector<Point> segments, const Snake& snake) {
  segments.push_back(snake_model::head(snake));
  segments.insert(segments.end(), snake_model::tail(snake).begin(), snake_model::tail(snake).end());
  return segments;
}

std::tuple<PerPlayerSnakes, ArenaEvents, std::vector<Point>> handleSelfBites(PerPlayerSnakes snakes,
                                                                             ArenaEvents events) {
  std::vector<Point> dropped_segments;
  for (auto& [player_id, snake] : snakes) {
    if (snake_model::alive(snake) && snakeBitesItself(snake)) {
      snake = snake_model::kill(snake);
      events.push_back(SelfBitten{player_id});
      dropped_segments = appendBody(std::move(dropped_segments), snake);
    }
  }
  return {snakes, events, dropped_segments};
}

}  // namespace

std::tuple<PerPlayerSnakes, ArenaEvents, std::vector<Point>> handleCollisions(PerPlayerSnakes snakes,
                                                                              ArenaEvents events) {
  std::vector<Point> dropped_segments;

  std::tie(snakes, events, dropped_segments) = handleSelfBites(snakes, events);

  if (snakes.size() < 2) {
    return {snakes, events, dropped_segments};
  }

  auto it1 = snakes.find(PLAYER_A);
  auto it2 = snakes.find(PLAYER_B);

  if (it1 == snakes.end() || it2 == snakes.end()) {
    return {snakes, events, dropped_segments};
  }

  Snake& snake_a = it1->second;
  Snake& snake_b = it2->second;

  if (!snake_model::alive(snake_a) || !snake_model::alive(snake_b)) {
    return {snakes, events, dropped_segments};
  }

  if (bothBiteEachOther(snake_a, snake_b)) {
    snake_a = snake_model::kill(snake_a);
    snake_b = snake_model::kill(snake_b);
    events.push_back(MutualBite{PLAYER_A, PLAYER_B});
    dropped_segments = appendBody(std::move(dropped_segments), snake_a);
    dropped_segments = appendBody(std::move(dropped_segments), snake_b);
  } else if (firstBitesSecond(snake_a, snake_b)) {
    events.push_back(Bitten{PLAYER_B, PLAYER_A});
    auto [new_snake, cut] = snake_model::cutAt(snake_b, snake_model::head(snake_a));
    snake_b = new_snake;
    dropped_segments.insert(dropped_segments.end(), cut.begin(), cut.end());
  } else if (firstBitesSecond(snake_b, snake_a)) {
    events.push_back(Bitten{PLAYER_A, PLAYER_B});
    auto [new_snake, cut] = snake_model::cutAt(snake_a, snake_model::head(snake_b));
    snake_a = new_snake;
    dropped_segments.insert(dropped_segments.end(), cut.begin(), cut.end());
  }

  return {snakes, events, dropped_segments};
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

FoodItems dropSegmentsAsFood(FoodItems food_items, const std::vector<Point>& dropped_segments) {
  for (const Point& segment : dropped_segments) {
    food_items = addFoodIfFree(std::move(food_items), segment);
  }
  return food_items;
}

std::tuple<FoodItems, ArenaEvents> handleFoodEating(FoodItems food_items,
                                                    ArenaEvents events,
                                                    const PerPlayerSnakes& snakes) {
  for (const auto& [player_id, snake] : snakes) {
    if (!snake_model::alive(snake)) continue;

    auto it = std::find(food_items.begin(), food_items.end(), snake_model::head(snake));

    if (it != food_items.end()) {
      food_items.erase(it);
      events.push_back(FoodEaten{player_id});
    }
  }

  return {std::move(food_items), std::move(events)};
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
