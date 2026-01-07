#include "snake/game_session.hpp"

#include <algorithm>
#include <functional>
#include <iostream>
#include <random>

#include "funkypipes/bind_front.hpp"
#include "funkypipes/make_pipe.hpp"
#include "snake/direction_command_filter.hpp"
#include "snake/functional_utils.hpp"
#include "snake/game_state_lenses.hpp"
#include "snake/game_state_views.hpp"
#include "snake/process_helpers.hpp"
#include "snake/snake_model.hpp"
#include "snake/timer/timer_factory.hpp"

namespace snake {

using funkypipes::bindFront;
using funkypipes::makePipe;

// Random integer generator function type
using RandomIntFn = std::function<int(int, int)>;

// ============================================================================
// Basic Game Functions
// ============================================================================

/**
 * @brief Check if game is in bite-drop-food collision mode
 *
 * @param state Current game state
 * @return true if collision mode is BITE_DROP_FOOD
 */
static bool isBiteDropFoodMode(const GameState& state) { return state.collision_mode == CollisionMode::BITE_DROP_FOOD; }

/**
 * @brief Get next head position based on current direction
 */
static Point getNextHeadPosition(const Snake& snake) {
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

/**
 * @brief Wrap a point around board boundaries
 */
static Point wrapPoint(Point p, const Board& board) {
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
// Snake Helper Functions
// ============================================================================

/**
 * @brief Move snake one step in current direction (shortens tail)
 *
 * Calculates new head position, wraps it around board boundaries,
 * and removes the last tail segment.
 *
 * @param snake Snake to move
 * @param board Board dimensions for wrapping
 * @return Snake after moving
 */
static Snake moveSnake(Snake snake, const Board& board) {
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

/**
 * @brief Grow snake one step in current direction (keeps tail)
 *
 * Calculates new head position, wraps it around board boundaries,
 * and keeps all tail segments (snake grows by one).
 *
 * @param snake Snake to grow
 * @param board Board dimensions for wrapping
 * @return Snake after growing
 */
static Snake growSnake(Snake snake, const Board& board) {
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

/**
 * @brief Cut snake tail at specified point
 *
 * Removes all tail segments from the cut point onwards and returns them separately.
 * If cut point is the head or not in tail, snake is unchanged and cut is empty.
 *
 * @param snake Snake to cut
 * @param cut_point Point where to cut the tail
 * @return Tuple of (snake with tail cut, cut tail segments)
 */
static std::tuple<Snake, std::vector<Point>> cutSnakeTailAt(Snake snake, Point cut_point) {
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

/**
 * @brief Generate a random food position not occupied by snakes
 *
 * @param board Board dimensions
 * @param snakes Map of player snakes to check for collisions
 * @param random_int Function that generates random int in range [min, max]
 * @return Random unoccupied position (or random position if all attempts fail)
 */
static Point generateRandomFoodPosition(const Board& board, const std::map<PlayerId, Snake>& snakes,
                                        std::function<int(int, int)> random_int) {
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
// Game Logic Functions - Pure sub-state transformations
// ============================================================================
// Each function operates only on its required sub-state, accessed via lenses.
// Function signatures reveal exact dependencies.
// All functions are pure: input (by value) → output.
// Pass by value enables move semantics and RVO for efficiency.

/**
 * @brief Apply direction changes to snakes
 *
 * @param snakes Snake map (passed by value)
 * @param consumed_directions Directions consumed from input queues
 * @return Updated snakes with new directions
 */
static std::map<PlayerId, Snake> applyDirectionChanges(std::map<PlayerId, Snake> snakes,
                                                       const std::map<PlayerId, Direction>& consumed_directions) {
  for (const auto& [player_id, direction] : consumed_directions) {
    auto it = snakes.find(player_id);
    if (it != snakes.end()) {
      it->second.current_direction = direction;
    }
  }
  return snakes;
}

/**
 * @brief Move all alive snakes one step
 *
 * If snake head will land on food, grow (keep tail).
 * Otherwise, move (shorten tail).
 *
 * @param snakes Snakes to move (by value)
 * @param board Board dimensions (for wrapping)
 * @param food_items Food positions (to check if eating)
 * @return Updated snakes after movement
 */
static std::map<PlayerId, Snake> moveSnakes(std::map<PlayerId, Snake> snakes, const Board& board,
                                            const std::vector<Point>& food_items) {
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

/**
 * @brief Handle snake-to-snake collisions
 *
 * Updates snakes (kill/cut) and scores based on collision detection.
 * Returns cut tail segments for potential food conversion.
 *
 * @param snakes Snakes (by value)
 * @param scores Scores (by value)
 * @return Tuple of (updated snakes, updated scores, cut tail segments)
 */
static std::tuple<std::map<PlayerId, Snake>, std::map<PlayerId, int>, std::vector<Point>> handleCollisions(
    std::map<PlayerId, Snake> snakes, std::map<PlayerId, int> scores) {
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

/**
 * @brief Add cut tail segments as food
 *
 * Takes cut tail segments and adds them as food items.
 * Separate stage for modular composition.
 *
 * @param food_items Food (by value)
 * @param cut_tails Cut tail segments to add
 * @return Updated food with cut tails added
 */
static std::vector<Point> dropCutTailsAsFood(std::vector<Point> food_items, const std::vector<Point>& cut_tails) {
  food_items.insert(food_items.end(), cut_tails.begin(), cut_tails.end());
  return food_items;
}

/**
 * @brief Add dead snake bodies to food (for BITE_DROP_FOOD mode)
 *
 * Checks for dead snakes and adds their body segments as food.
 * Separate from collision detection to maintain sub-state separation.
 *
 * @param food_items Food (by value)
 * @param snakes Snakes (to check for dead ones)
 * @return Updated food with dead snake bodies added
 */
static std::vector<Point> dropDeadSnakesAsFood(std::vector<Point> food_items, const std::map<PlayerId, Snake>& snakes) {
  for (const auto& [player_id, snake] : snakes) {
    if (!snake.alive) {
      // Add entire snake body as food
      auto body = snake.toBody();
      food_items.insert(food_items.end(), body.begin(), body.end());
    }
  }

  return food_items;
}

/**
 * @brief Handle snakes eating food
 *
 * If snake head is on food:
 * - Remove eaten food
 * - Award points (+10)
 *
 * @param food_items Food (by value)
 * @param scores Scores (by value)
 * @param snakes Snakes (to check head positions)
 * @return Tuple of (updated food, updated scores)
 */
static std::tuple<std::vector<Point>, std::map<PlayerId, int>> handleFoodEating(
    std::vector<Point> food_items, std::map<PlayerId, int> scores, const std::map<PlayerId, Snake>& snakes) {
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

/**
 * @brief Replenish food to maintain target count
 *
 * Adds new food items until the target count is reached.
 *
 * @param random_int Random number generator function
 * @param target_count Desired number of food items
 * @param food_items Food (by value)
 * @param board Board dimensions
 * @param snakes Snakes (for position generation)
 * @return Updated food with new items added
 */
static std::vector<Point> replenishFood(RandomIntFn random_int, int target_count, std::vector<Point> food_items,
                                        const Board& board, const std::map<PlayerId, Snake>& snakes) {
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

/**
 * @brief Update food positions periodically
 *
 * Every 15 ticks, move one random food item to a new position.
 *
 * @param random_int Random number generator function
 * @param tick_count Current tick number
 * @param food_items Food (by value)
 * @param board Board dimensions
 * @param snakes Snakes (for position generation)
 * @return Updated food with repositioned item
 */
static std::vector<Point> updateFoodPositions(RandomIntFn random_int, int tick_count, std::vector<Point> food_items,
                                              const Board& board, const std::map<PlayerId, Snake>& snakes) {
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

// ============================================================================
// Random Number Generation
// ============================================================================

/**
 * @brief Create a random integer generator function
 *
 * Returns a function that generates random integers in a given range [min, max].
 * Uses static random_device and mt19937 for thread-safe randomness.
 *
 * @return Function that takes (min, max) and returns random int in that range
 */
static RandomIntFn makeRandomIntGenerator() {
  static std::random_device rd;
  static std::mt19937 gen(rd());

  return [](int min, int max) mutable {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(min, max);
    return dist(gen);
  };
}

// ============================================================================
// GameSession implementation
// ============================================================================

GameSession::GameSession(asio::io_context& io, TopicPtr<DirectionChange> direction_topic,
                         TopicPtr<StateUpdate> state_topic, TopicPtr<GameClockCommand> clock_topic,
                         TopicPtr<TickRateChange> tickrate_topic, TopicPtr<LevelChange> levelchange_topic,
                         TimerFactoryPtr timer_factory)
    : Actor(io),
      state_pub_(create_pub(state_topic)),
      direction_sub_(create_sub(direction_topic)),
      clock_sub_(create_sub(clock_topic)),
      tickrate_sub_(create_sub(tickrate_topic)),
      levelchange_sub_(create_sub(levelchange_topic)),
      timer_(create_timer<GameTimer>(timer_factory)) {
  state_.game_id = "game_001";
  state_.board.width = 60;
  state_.board.height = 20;

  // Initialize snakes for player1 and player2
  initializeSnake("player1");
  initializeSnake("player2");

  // Initialize food items
  initializeFood();
}

void GameSession::processMessages() {
  // Drain direction commands into filtered queues
  process_message_with_state(direction_sub_, state_,
                             over_pending_directions_with_snakes(direction_command_filter::try_add));

  process_event(timer_, [&](const GameTimerElapsedEvent&) { onTick(); });

  process_message(clock_sub_, [&](const GameClockCommand& msg) { onGameClockCommand(msg); });

  process_message(tickrate_sub_, [&](const TickRateChange& msg) { onTickRateChange(msg); });

  process_message(levelchange_sub_, [&](const LevelChange& msg) { onLevelChange(msg); });
}

void GameSession::onTick() {
  ++tick_count_;

  // Create random number generator for this tick
  auto random_fn = makeRandomIntGenerator();

  // ============================================================================
  // GAME LOGIC PIPELINE - Functional Composition with funkypipes
  // ============================================================================
  // makePipe automatically unpacks tuples between stages
  // When a function returns tuple<A, B>, the next function receives (A, B) as separate args
  // Lenses are decorators that return state transformers

  // clang-format off
  auto tick_pipeline = makePipe(
      over_pending_directions(direction_command_filter::try_consume_next),                 // → (state, next_directions)
      over_snakes(applyDirectionChanges),                                                  // → state
      over_snakes_with_board_and_food(moveSnakes),                                         // → state
      over_snakes_and_scores(handleCollisions),                                            // → (state, cut_tails)
      when<0>(isBiteDropFoodMode, over_food(dropCutTailsAsFood)),                          // → state
      when(isBiteDropFoodMode, over_food_with_snakes(dropDeadSnakesAsFood)),               // → state
      over_food_and_scores_with_snakes(handleFoodEating),                                  // → state
      over_food_with_board_and_snakes(bindFront(replenishFood, random_fn, 5)),             // → state
      over_food_with_board_and_snakes(bindFront(updateFoodPositions, random_fn, tick_count_))); // → state
  // clang-format on

  state_ = tick_pipeline(state_);
  state_pub_->publish(StateUpdate{state_});
}

void GameSession::onGameClockCommand(const GameClockCommand& msg) {
  switch (msg.state) {
    case GameClockState::START:
      std::cout << "[GameSession] Starting internal timer\n";
      timer_->execute_command(make_periodic_command<GameTimerTag>(std::chrono::milliseconds(interval_ms_)));
      break;

    case GameClockState::STOP:
      std::cout << "[GameSession] Stopping internal timer\n";
      timer_->execute_command(make_cancel_command<GameTimerTag>());
      break;

    case GameClockState::PAUSE:
      std::cout << "[GameSession] Pausing game\n";
      timer_->execute_command(make_cancel_command<GameTimerTag>());
      break;

    case GameClockState::RESUME:
      std::cout << "[GameSession] Resuming game\n";
      timer_->execute_command(make_periodic_command<GameTimerTag>(std::chrono::milliseconds(interval_ms_)));
      break;
  }
}

void GameSession::onTickRateChange(const TickRateChange& msg) {
  std::cout << "[GameSession] Changing tick rate to " << msg.interval_ms << "ms\n";
  interval_ms_ = msg.interval_ms;

  // If timer is currently running, restart it with new interval
  if (timer_->is_scheduled()) {
    timer_->execute_command(make_periodic_command<GameTimerTag>(std::chrono::milliseconds(interval_ms_)));
  }
}

void GameSession::onLevelChange(const LevelChange& msg) {
  std::cout << "[GameSession] Level changed to " << msg.new_level << "\n";
  state_.level = msg.new_level;
}

void GameSession::initializeSnake(const PlayerId& player_id) {
  // Position snakes at different y-levels
  int y_pos = (player_id == "player1") ? 10 : 15;
  int start_x = 5;

  // Create a snake of length 7, moving right
  // Head at front, tail behind it
  Point head = {start_x, y_pos};
  std::vector<Point> tail;
  for (int i = 1; i < 7; ++i) {
    tail.push_back({start_x - i, y_pos});
  }

  Snake snake{head, tail, Direction::RIGHT, true};

  // Insert snake into map using player_id as key
  state_.snakes[player_id] = snake;

  // Initialize score in GameState
  state_.scores[player_id] = 0;

  // Initialize pending direction queue for this player
  state_.pending_directions[player_id] = std::deque<Direction>{};

  std::cout << "[GameSession] Initialized snake for '" << player_id << "' at y=" << y_pos << "\n";
}

void GameSession::initializeFood() {
  auto random_int = makeRandomIntGenerator();

  // Generate 5 random food items
  auto generate_food = with_board_and_snakes(generateRandomFoodPosition);
  for (int i = 0; i < 5; ++i) {
    state_.food_items.push_back(generate_food(state_, random_int));
  }
  std::cout << "[GameSession] Initialized " << state_.food_items.size() << " food items\n";
}

}  // namespace snake
