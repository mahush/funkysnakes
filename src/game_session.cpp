#include "snake/game_session.hpp"

#include <algorithm>
#include <functional>
#include <iostream>
#include <random>

#include "snake/functional_utils.hpp"
#include "snake/game_state_lenses.hpp"
#include "snake/snake_model.hpp"
#include "snake/state_with_effect.hpp"
#include "snake/timer/timer_factory.hpp"

namespace snake {

// ============================================================================
// Pure subsystem functions - operate on sub-states and return StateWithEffect
// ============================================================================

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
static Point wrapPoint(Point p, int board_width, int board_height) {
  if (p.x < 0) {
    p.x = board_width - 1;
  } else if (p.x >= board_width) {
    p.x = 0;
  }

  if (p.y < 0) {
    p.y = board_height - 1;
  } else if (p.y >= board_height) {
    p.y = 0;
  }

  return p;
}

/**
 * @brief Apply a single operation to a snake
 *
 * Operations are composable transformations on snake state.
 *
 * @param snake Current snake
 * @param op Operation to apply
 * @param board_width Width of game board (for wrapping)
 * @param board_height Height of game board (for wrapping)
 * @return Snake after applying operation
 */
static Snake applyOperation(Snake snake, const SnakeOperation& op, int board_width, int board_height) {
  switch (op.op) {
    case SnakeOp::MOVE: {
      if (!snake.alive) return snake;

      Point new_head = getNextHeadPosition(snake);
      new_head = wrapPoint(new_head, board_width, board_height);

      // Build new tail: old head + old tail (minus last)
      std::vector<Point> new_tail;
      new_tail.reserve(snake.tail.size() + 1);
      new_tail.push_back(snake.head);
      new_tail.insert(new_tail.end(), snake.tail.begin(), snake.tail.end());
      if (!new_tail.empty()) {
        new_tail.pop_back();
      }

      snake.head = new_head;
      snake.tail = new_tail;
      break;
    }

    case SnakeOp::GROW: {
      if (!snake.alive) return snake;

      Point new_head = getNextHeadPosition(snake);
      new_head = wrapPoint(new_head, board_width, board_height);

      // Build new tail: old head + old tail (keep last)
      std::vector<Point> new_tail;
      new_tail.reserve(snake.tail.size() + 1);
      new_tail.push_back(snake.head);
      new_tail.insert(new_tail.end(), snake.tail.begin(), snake.tail.end());

      snake.head = new_head;
      snake.tail = new_tail;
      break;
    }

    case SnakeOp::CUT_TAIL_AT: {
      if (op.point && !(snake.head == *op.point)) {
        auto it = std::find(snake.tail.begin(), snake.tail.end(), *op.point);
        if (it != snake.tail.end()) {
          snake.tail = std::vector<Point>(snake.tail.begin(), it);
        }
      }
      break;
    }

    case SnakeOp::KILL:
      snake.alive = false;
      break;

    case SnakeOp::LEFT:
      snake.current_direction = Direction::LEFT;
      break;

    case SnakeOp::RIGHT:
      snake.current_direction = Direction::RIGHT;
      break;

    case SnakeOp::UP:
      snake.current_direction = Direction::UP;
      break;

    case SnakeOp::DOWN:
      snake.current_direction = Direction::DOWN;
      break;
  }

  return snake;
}

/**
 * @brief Handle collision between two snakes - drop tail mode
 *
 * Uses pattern matching on collision cases. Cut tail segments are simply
 * dropped (not transformed to food). Returns operations as effects.
 *
 * @param a First player snake pair (player ID + snake)
 * @param b Second player snake pair (player ID + snake)
 * @return Game effects (scores, operations)
 */
static GameEffect handleSnakeCollision(std::pair<PlayerId, Snake> a, std::pair<PlayerId, Snake> b) {
  auto [player_a, snake_a] = a;
  auto [player_b, snake_b] = b;

  // Only check collisions if both snakes are alive
  if (!snake_a.alive || !snake_b.alive) {
    return GameEffect::empty();
  }

  GameEffect effect = GameEffect::empty();

  // Case 1: Both snakes bite each other → both die completely
  if (bothBiteEachOther(snake_a, snake_b)) {
    effect.scores.addForPlayer(player_a, -10);
    effect.scores.addForPlayer(player_b, -10);
    // Generate KILL operations
    effect.snakes.addOp(player_a, SnakeOperation::kill());
    effect.snakes.addOp(player_b, SnakeOperation::kill());
  }
  // Case 2: Snake A bites Snake B
  else if (firstBitesSecond(snake_a, snake_b)) {
    effect.scores.addForPlayer(player_b, -10);
    // Generate CUT_TAIL_AT operation
    effect.snakes.addOp(player_b, SnakeOperation::cutTailAt(snake_a.head));
    // cut segments are dropped
  }
  // Case 3: Snake B bites Snake A
  else if (firstBitesSecond(snake_b, snake_a)) {
    effect.scores.addForPlayer(player_a, -10);
    // Generate CUT_TAIL_AT operation
    effect.snakes.addOp(player_a, SnakeOperation::cutTailAt(snake_b.head));
    // cut segments are dropped
  }
  // Case 4: No collision - effect already empty

  return effect;
}

/**
 * @brief Handle collision between two snakes - food drop mode
 *
 * Uses pattern matching on collision cases. Cut tail segments are
 * transformed into food items. Returns operations as effects.
 *
 * @param a First player snake pair (player ID + snake)
 * @param b Second player snake pair (player ID + snake)
 * @return Game effects (scores, food, operations)
 */
static GameEffect handleSnakeCollisionWithFoodDrop(std::pair<PlayerId, Snake> a, std::pair<PlayerId, Snake> b) {
  auto [player_a, snake_a] = a;
  auto [player_b, snake_b] = b;

  // Only check collisions if both snakes are alive
  if (!snake_a.alive || !snake_b.alive) {
    return GameEffect::empty();
  }

  GameEffect effect = GameEffect::empty();

  // Case 1: Both snakes bite each other → both die completely and turn to food
  if (bothBiteEachOther(snake_a, snake_b)) {
    effect.scores.addForPlayer(player_a, -10);
    effect.scores.addForPlayer(player_b, -10);
    // Transform entire snakes to food
    effect.food.add(snake_a.toBody());
    effect.food.add(snake_b.toBody());
    // Generate KILL operations
    effect.snakes.addOp(player_a, SnakeOperation::kill());
    effect.snakes.addOp(player_b, SnakeOperation::kill());
  }
  // Case 2: Snake A bites Snake B
  else if (firstBitesSecond(snake_a, snake_b)) {
    auto cut = cutTailAt(snake_b, snake_a.head);

    effect.scores.addForPlayer(player_b, -10);
    // Transform cut segments to food
    effect.food.add(cut.cut_segments);
    // Generate CUT_TAIL_AT operation
    effect.snakes.addOp(player_b, SnakeOperation::cutTailAt(snake_a.head));
  }
  // Case 3: Snake B bites Snake A
  else if (firstBitesSecond(snake_b, snake_a)) {
    auto cut = cutTailAt(snake_a, snake_b.head);

    effect.scores.addForPlayer(player_a, -10);
    // Transform cut segments to food
    effect.food.add(cut.cut_segments);
    // Generate CUT_TAIL_AT operation
    effect.snakes.addOp(player_a, SnakeOperation::cutTailAt(snake_b.head));
  }
  // Case 4: No collision - effect already empty

  return effect;
}

// ============================================================================
// Pipeline stage functions for onTick
// ============================================================================

/**
 * @brief Pipeline stage 0: Apply accumulated direction change effects
 *
 * Applies direction operations accumulated from onDirectionChange calls between ticks.
 * These are applied BEFORE movement generation so snakes move in the updated direction.
 *
 * @param state_with_effect Current game state with accumulated effects
 * @param pending_direction_effects Direction ops accumulated between ticks
 * @return Updated state with directions changed
 */
static GameStateWithEffect applyPendingDirectionOps(GameStateWithEffect state_with_effect,
                                                    const SnakeUpdateEffect& pending_direction_effects) {
  // Apply direction operations from pending effects
  for (const auto& [player_id, ops] : pending_direction_effects.operations) {
    if (state_with_effect.state.snakes.find(player_id) == state_with_effect.state.snakes.end()) {
      continue;  // Player doesn't exist
    }

    Snake& snake = state_with_effect.state.snakes[player_id];

    // Apply all direction ops for this player (should be 0 or 1 due to overwrite semantics)
    for (const auto& op : ops) {
      if (op.op == SnakeOp::LEFT || op.op == SnakeOp::RIGHT || op.op == SnakeOp::UP || op.op == SnakeOp::DOWN) {
        // Use existing applyOperation logic
        snake = applyOperation(snake, op, state_with_effect.state.board_width, state_with_effect.state.board_height);
      }
    }
  }

  return state_with_effect;
}

/**
 * @brief Pipeline stage 1: Generate movement operations for all alive snakes
 *
 * Checks if snake will land on food. If yes, generates GROW (keeps tail).
 * If no, generates MOVE (shortens tail).
 *
 * @param state_with_effect Current game state with accumulated effects
 * @param board_width Board width for position wrapping
 * @param board_height Board height for position wrapping
 * @return Updated state with movement operations added to effect
 */
static GameStateWithEffect generateSnakeMovementOps(GameStateWithEffect state_with_effect, int board_width,
                                                    int board_height) {
  for (const auto& [player_id, snake] : state_with_effect.state.snakes) {
    if (!snake.alive) continue;

    // Calculate where snake will land
    Point next_head = getNextHeadPosition(snake);
    next_head = wrapPoint(next_head, board_width, board_height);

    // Check if landing on food
    bool will_eat_food = std::find(state_with_effect.state.food_items.begin(), state_with_effect.state.food_items.end(),
                                   next_head) != state_with_effect.state.food_items.end();

    // Generate GROW if landing on food, MOVE otherwise
    if (will_eat_food) {
      state_with_effect.effect.snakes.addOp(player_id, SnakeOperation::grow());
    } else {
      state_with_effect.effect.snakes.addOp(player_id, SnakeOperation::move());
    }
  }
  return state_with_effect;
}

/**
 * @brief Pipeline stage 2: Generate collision operations based on collision mode
 *
 * @param state_with_effect Current game state with accumulated effects
 * @param collision_mode Collision mode (drop tail or drop as food)
 * @return Updated state with collision operations added to effect
 */
static GameStateWithEffect generateCollisionOps(GameStateWithEffect state_with_effect, CollisionMode collision_mode) {
  if (state_with_effect.state.snakes.size() >= 2) {
    GameEffect collision_effect;
    if (collision_mode == CollisionMode::BITE_DROP_FOOD) {
      collision_effect = over_selected_snakes_combining_scores(GameStateWithEffect::fromState(state_with_effect.state),
                                                               handleSnakeCollisionWithFoodDrop, "player1", "player2")
                             .effect;
    } else {
      collision_effect = over_selected_snakes_combining_scores(GameStateWithEffect::fromState(state_with_effect.state),
                                                               handleSnakeCollision, "player1", "player2")
                             .effect;
    }
    state_with_effect.effect = combine(state_with_effect.effect, collision_effect);
  }
  return state_with_effect;
}

/**
 * @brief Pipeline stage 3: Apply all accumulated snake operations to snake state
 *
 * @param state_with_effect Current game state with accumulated effects
 * @param board_width Width of game board for wrapping
 * @param board_height Height of game board for wrapping
 * @return Updated state with snakes modified and operations consumed
 */
static GameStateWithEffect applySnakeOps(GameStateWithEffect state_with_effect, int board_width, int board_height) {
  for (const auto& [player_id, ops] : state_with_effect.effect.snakes.operations) {
    Snake snake = state_with_effect.state.snakes[player_id];
    for (const auto& op : ops) {
      snake = applyOperation(snake, op, board_width, board_height);
    }
    state_with_effect.state.snakes[player_id] = snake;
  }
  // Consume the operations after applying them
  state_with_effect.effect.snakes.operations.clear();
  return state_with_effect;
}

/**
 * @brief Pipeline stage 4: Apply score deltas to player scores
 *
 * @param state_with_effect Current game state with accumulated effects
 * @return Updated state with scores modified and deltas consumed
 */
static GameStateWithEffect applyScoreEffects(GameStateWithEffect state_with_effect) {
  for (const auto& [player_id, delta] : state_with_effect.effect.scores.deltas) {
    state_with_effect.state.scores[player_id] += delta;
  }
  // Consume the deltas after applying them
  state_with_effect.effect.scores.deltas.clear();
  return state_with_effect;
}

/**
 * @brief Pipeline stage 5: Apply food additions and removals
 *
 * @param state_with_effect Current game state with accumulated effects
 * @return Updated state with food items modified and effects consumed
 */
static GameStateWithEffect applyFoodEffects(GameStateWithEffect state_with_effect) {
  // Apply removals first
  for (const Point& food_loc : state_with_effect.effect.food.removals) {
    auto& items = state_with_effect.state.food_items;
    auto it = std::find(items.begin(), items.end(), food_loc);
    if (it != items.end()) {
      items.erase(it);
    }
  }

  // Then apply additions
  for (const Point& food_loc : state_with_effect.effect.food.additions) {
    state_with_effect.state.food_items.push_back(food_loc);
  }

  // Consume the food effects after applying them
  state_with_effect.effect.food.additions.clear();
  state_with_effect.effect.food.removals.clear();

  return state_with_effect;
}

/**
 * @brief Pipeline stage 6: Generate food eating effects
 *
 * Checks if any snake head is on a food position. If yes:
 * - Generates score effect (+10 points)
 * - Generates food removal effect for eaten food
 * - Generates food addition effect for replacement food (keeps count constant)
 *
 * Note: Snake growth is handled by GROW operation in generateSnakeMovementOps.
 *
 * @param state_with_effect Current game state with accumulated effects
 * @param generate_position Function to generate random food position for replacement
 * @return Updated state with food eating effects added
 */
static GameStateWithEffect generateFoodEatingEffects(GameStateWithEffect state_with_effect,
                                                     std::function<Point()> generate_position) {
  // Check each snake for food consumption
  for (const auto& [player_id, snake] : state_with_effect.state.snakes) {
    if (!snake.alive) continue;

    // Check if snake head is on food
    auto it =
        std::find(state_with_effect.state.food_items.begin(), state_with_effect.state.food_items.end(), snake.head);

    if (it != state_with_effect.state.food_items.end()) {
      // Snake ate food!
      // Generate replacement food position
      Point new_food_pos = generate_position();

      // Generate effects: remove eaten food, add replacement, award points
      state_with_effect.effect.food.remove(snake.head);
      state_with_effect.effect.food.add(new_food_pos);
      state_with_effect.effect.scores.addForPlayer(player_id, 10);
    }
  }

  return state_with_effect;
}

/**
 * @brief Pipeline stage 7: Generate food position update effects periodically
 *
 * Every 10 ticks, generate effects to move one random food item to a new random position.
 * This stage generates effects (removal + addition) that will be applied by applyFoodEffects.
 *
 * @param state_with_effect Current game state with accumulated effects
 * @param tick_count Current tick counter
 * @param generate_position Function to generate random food position
 * @return Updated state with food repositioning effects added
 */
static GameStateWithEffect updateFoodPositions(GameStateWithEffect state_with_effect, int tick_count,
                                               std::function<Point()> generate_position) {
  // Move one food every 10 ticks
  if (tick_count % 15 == 0 && !state_with_effect.state.food_items.empty()) {
    static std::random_device rd;
    static std::mt19937 gen(rd());

    // Pick a random food item to move
    std::uniform_int_distribution<> dist(0, state_with_effect.state.food_items.size() - 1);
    int food_index = dist(gen);

    // Get old position and generate new position
    Point old_pos = state_with_effect.state.food_items[food_index];
    Point new_pos = generate_position();

    // Generate effects: remove old position, add new position
    state_with_effect.effect.food.remove(old_pos);
    state_with_effect.effect.food.add(new_pos);
  }

  return state_with_effect;
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
  state_.board_width = 60;
  state_.board_height = 20;

  // Initialize snakes for player1 and player2
  initializeSnake("player1");
  initializeSnake("player2");

  // Initialize food items
  initializeFood();
}

void GameSession::processMessages() {
  while (auto dir = direction_sub_->tryReceive()) {
    onDirectionChange(*dir);
  }

  auto timer_events = timer_->take_all_elapsed_events();
  for (const auto& event : timer_events) {
    (void)event;  // Unused for now
    onTick();
  }

  while (auto msg = clock_sub_->tryReceive()) {
    onGameClockCommand(*msg);
  }

  while (auto msg = tickrate_sub_->tryReceive()) {
    onTickRateChange(*msg);
  }

  while (auto msg = levelchange_sub_->tryReceive()) {
    onLevelChange(*msg);
  }
}

void GameSession::onTick() {
  // Increment tick counter
  ++tick_count_;

  // Create lambdas that capture context from this
  auto applyPendingDirectionOps = [this](GameStateWithEffect swe) {
    return snake::applyPendingDirectionOps(swe, pending_effects_.snakes);
  };

  auto generateSnakeMovementOps = [this](GameStateWithEffect swe) {
    return snake::generateSnakeMovementOps(swe, state_.board_width, state_.board_height);
  };

  auto generateSnakeCollisionOps = [this](GameStateWithEffect swe) {
    return snake::generateCollisionOps(swe, collision_mode_);
  };

  auto applySnakeOps = [this](GameStateWithEffect swe) {
    return snake::applySnakeOps(swe, state_.board_width, state_.board_height);
  };

  auto generateFoodEatingEffects = [this](GameStateWithEffect swe) {
    return snake::generateFoodEatingEffects(swe, [this]() { return generateRandomFoodPosition(); });
  };

  auto updateFoodPositions = [this](GameStateWithEffect swe) {
    return snake::updateFoodPositions(swe, tick_count_, [this]() { return generateRandomFoodPosition(); });
  };

  // Compose pipeline stages using nested function calls
  // Order: pending directions -> movement -> collision -> food eating -> scores -> food updates -> apply food
  auto final_state = applyFoodEffects(updateFoodPositions(
      applyScoreEffects(generateFoodEatingEffects(applySnakeOps(generateSnakeCollisionOps(applySnakeOps(
          generateSnakeMovementOps(applyPendingDirectionOps(GameStateWithEffect::fromState(state_))))))))));

  // Update internal state and publish
  state_ = final_state.state;

  // Clear pending effects for next tick
  pending_effects_ = GameEffect::empty();

  StateUpdate update;
  update.state = state_;
  state_pub_->publish(update);
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

void GameSession::onDirectionChange(const DirectionChange& msg) {
  // Find the snake for this player
  auto it = state_.snakes.find(msg.player_id);
  if (it == state_.snakes.end()) {
    return;  // Player not found
  }

  const Snake& snake = it->second;  // const - no direct mutation!

  // Prevent 180-degree turns (going back into yourself)
  // Validate against CURRENT state direction (not pending effects)
  Direction new_dir = msg.new_direction;
  Direction current = snake.current_direction;

  bool is_opposite = (current == Direction::UP && new_dir == Direction::DOWN) ||
                     (current == Direction::DOWN && new_dir == Direction::UP) ||
                     (current == Direction::LEFT && new_dir == Direction::RIGHT) ||
                     (current == Direction::RIGHT && new_dir == Direction::LEFT);

  if (!is_opposite) {
    // Generate direction operation
    SnakeOperation dir_op;
    switch (new_dir) {
      case Direction::LEFT:
        dir_op = SnakeOperation::left();
        break;
      case Direction::RIGHT:
        dir_op = SnakeOperation::right();
        break;
      case Direction::UP:
        dir_op = SnakeOperation::up();
        break;
      case Direction::DOWN:
        dir_op = SnakeOperation::down();
        break;
    }

    // Overwrite semantics: clear previous direction ops for this player
    auto& ops = pending_effects_.snakes.operations[msg.player_id];
    ops.erase(std::remove_if(ops.begin(), ops.end(),
                             [](const SnakeOperation& op) {
                               return op.op == SnakeOp::LEFT || op.op == SnakeOp::RIGHT || op.op == SnakeOp::UP ||
                                      op.op == SnakeOp::DOWN;
                             }),
              ops.end());

    // Add new direction op (last valid direction wins)
    pending_effects_.snakes.addOp(msg.player_id, dir_op);
  }
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

  std::cout << "[GameSession] Initialized snake for '" << player_id << "' at y=" << y_pos << "\n";
}

Point GameSession::generateRandomFoodPosition() const {
  static std::random_device rd;
  static std::mt19937 gen(rd());

  std::uniform_int_distribution<> dist_x(0, state_.board_width - 1);
  std::uniform_int_distribution<> dist_y(0, state_.board_height - 1);

  // Try to find a position not occupied by snakes
  // Max 100 attempts to avoid infinite loop
  for (int attempt = 0; attempt < 100; ++attempt) {
    Point candidate{dist_x(gen), dist_y(gen)};

    // Check if position is occupied by any snake
    bool occupied = false;
    for (const auto& [player_id, snake] : state_.snakes) {
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
  return {dist_x(gen), dist_y(gen)};
}

void GameSession::initializeFood() {
  // Generate 5 random food items
  for (int i = 0; i < 5; ++i) {
    state_.food_items.push_back(generateRandomFoodPosition());
  }
  std::cout << "[GameSession] Initialized " << state_.food_items.size() << " food items\n";
}

}  // namespace snake
