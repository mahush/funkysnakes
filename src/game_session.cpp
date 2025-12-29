#include "snake/game_session.hpp"

#include <algorithm>
#include <functional>
#include <iostream>
#include <random>

#include "snake/direction_command_filter.hpp"
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
 * @brief Convert consumed direction commands to snake operations (pure game logic)
 *
 * Takes preprocessed direction inputs and converts them to snake operations.
 * This function is pure: it only reads consumed directions and produces effects,
 * without modifying any state. Input preprocessing (queue consumption) happens
 * outside the game logic pipeline.
 *
 * @param consumed_directions Map of player ID to consumed direction
 * @return Game effect with snake direction operations
 */
static GameEffect provideDirectionChangeOps(const std::map<PlayerId, Direction>& consumed_directions) {
  GameEffect effect = GameEffect::empty();

  // Convert consumed directions to snake operations
  for (const auto& [player_id, direction] : consumed_directions) {
    SnakeOperation op;
    switch (direction) {
      case Direction::LEFT:
        op = SnakeOperation::left();
        break;
      case Direction::RIGHT:
        op = SnakeOperation::right();
        break;
      case Direction::UP:
        op = SnakeOperation::up();
        break;
      case Direction::DOWN:
        op = SnakeOperation::down();
        break;
    }
    effect.snakes.addOp(player_id, op);
  }

  return effect;
}

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

/**
 * @brief Apply a single operation to a snake
 *
 * Operations are composable transformations on snake state.
 *
 * @param snake Current snake
 * @param op Operation to apply
 * @param board Board dimensions (for wrapping)
 * @return Snake after applying operation
 */
static Snake applyOperation(Snake snake, const SnakeOperation& op, const Board& board) {
  switch (op.op) {
    case SnakeOp::MOVE: {
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
      snake.tail = new_tail;
      break;
    }

    case SnakeOp::GROW: {
      if (!snake.alive) return snake;

      Point new_head = getNextHeadPosition(snake);
      new_head = wrapPoint(new_head, board);

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
 * @brief Pipeline stage 1: Generate movement operations for all alive snakes
 *
 * Checks if snake will land on food. If yes, generates GROW (keeps tail).
 * If no, generates MOVE (shortens tail).
 *
 * @param state_with_effect Current game state with accumulated effects
 * @return Updated state with movement operations added to effect
 */
static GameStateWithEffect generateSnakeMovementOps(GameStateWithEffect state_with_effect) {
  for (const auto& [player_id, snake] : state_with_effect.state.snakes) {
    if (!snake.alive) continue;

    // Calculate where snake will land
    Point next_head = getNextHeadPosition(snake);
    next_head = wrapPoint(next_head, state_with_effect.state.board);

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
 * @return Updated state with collision operations added to effect
 */
static GameStateWithEffect generateCollisionOps(GameStateWithEffect state_with_effect) {
  if (state_with_effect.state.snakes.size() >= 2) {
    GameEffect collision_effect;
    if (state_with_effect.state.collision_mode == CollisionMode::BITE_DROP_FOOD) {
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
 * @return Updated state with snakes modified and operations consumed
 */
static GameStateWithEffect applySnakeOps(GameStateWithEffect state_with_effect) {
  // Create new state with modified snakes
  GameState new_state = state_with_effect.state;

  for (const auto& [player_id, ops] : state_with_effect.effect.snakes.operations) {
    Snake snake = new_state.snakes.at(player_id);
    for (const auto& op : ops) {
      snake = applyOperation(snake, op, new_state.board);
    }
    new_state.snakes[player_id] = snake;
  }

  // Consume the operations after applying them
  GameEffect new_effect = state_with_effect.effect;
  new_effect.snakes.operations.clear();

  return {new_state, new_effect};
}

/**
 * @brief Pipeline stage 4: Apply score deltas to player scores
 *
 * @param state_with_effect Current game state with accumulated effects
 * @return Updated state with scores modified and deltas consumed
 */
static GameStateWithEffect applyScoreEffects(GameStateWithEffect state_with_effect) {
  // Create new state with modified scores
  GameState new_state = state_with_effect.state;

  for (const auto& [player_id, delta] : state_with_effect.effect.scores.deltas) {
    new_state.scores[player_id] += delta;
  }

  // Consume the deltas after applying them
  GameEffect new_effect = state_with_effect.effect;
  new_effect.scores.deltas.clear();

  return {new_state, new_effect};
}

/**
 * @brief Pipeline stage 5: Apply food additions and removals
 *
 * @param state_with_effect Current game state with accumulated effects
 * @return Updated state with food items modified and effects consumed
 */
static GameStateWithEffect applyFoodEffects(GameStateWithEffect state_with_effect) {
  // Create new state with modified food items
  GameState new_state = state_with_effect.state;

  // Apply removals first
  for (const Point& food_loc : state_with_effect.effect.food.removals) {
    auto it = std::find(new_state.food_items.begin(), new_state.food_items.end(), food_loc);
    if (it != new_state.food_items.end()) {
      new_state.food_items.erase(it);
    }
  }

  // Then apply additions
  for (const Point& food_loc : state_with_effect.effect.food.additions) {
    new_state.food_items.push_back(food_loc);
  }

  // Consume the food effects after applying them
  GameEffect new_effect = state_with_effect.effect;
  new_effect.food.additions.clear();
  new_effect.food.removals.clear();

  return {new_state, new_effect};
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
static GameStateWithEffect generateFoodEatingOps(GameStateWithEffect state_with_effect,
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
 * @brief Generate a random food position not occupied by snakes
 *
 * @param board Board dimensions
 * @param snakes Map of player snakes to check for collisions
 * @return Random unoccupied position (or random position if all attempts fail)
 */
static Point generateRandomFoodPosition(const Board& board, const std::map<PlayerId, Snake>& snakes) {
  static std::random_device rd;
  static std::mt19937 gen(rd());

  std::uniform_int_distribution<> dist_x(0, board.width - 1);
  std::uniform_int_distribution<> dist_y(0, board.height - 1);

  // Try to find a position not occupied by snakes
  // Max 100 attempts to avoid infinite loop
  for (int attempt = 0; attempt < 100; ++attempt) {
    Point candidate{dist_x(gen), dist_y(gen)};

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
  return {dist_x(gen), dist_y(gen)};
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
static GameStateWithEffect generateFoodUpdateOps(GameStateWithEffect state_with_effect, int tick_count,
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
  while (auto dir = direction_sub_->tryReceive()) {
    state_ = over_direction_command_and_snakes(state_, direction_command_filter::try_add, *dir);
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

  // ============================================================================
  // INPUT PREPROCESSING LAYER
  // ============================================================================
  // Process buffered player inputs before game logic runs.
  // This modifies direction_command filter state (consumes queue entries).
  auto consume_result = direction_command_filter::try_consume_next(state_.direction_command);
  state_.direction_command = consume_result.filters;  // Update filter queues (consumed entries removed)

  // ============================================================================
  // GAME LOGIC PIPELINE
  // ============================================================================
  // All stages below are pure game logic: read state → produce effects.
  // No direct state mutation happens here.

  // Wrap direction ops generator to capture consumed inputs from preprocessing layer
  auto provideDirectionOps = [&consumed = consume_result.consumed_directions](GameStateWithEffect swe) {
    return GameStateWithEffect{swe.state, combine(swe.effect, provideDirectionChangeOps(consumed))};
  };

  // Create lambdas that extract state from pipeline
  auto generateFoodEatingOps = [](GameStateWithEffect swe) {
    return snake::generateFoodEatingOps(
        swe, [&swe]() { return with_board_and_snakes(swe.state, generateRandomFoodPosition); });
  };

  auto generateFoodUpdateOps = [tick_count = tick_count_](GameStateWithEffect swe) {
    return snake::generateFoodUpdateOps(
        swe, tick_count, [&swe]() { return with_board_and_snakes(swe.state, generateRandomFoodPosition); });
  };

  // Compose pipeline stages using nested function calls
  // Order: provide directions -> apply ops -> movement -> apply ops -> collision -> apply ops -> food eating -> scores
  // -> food updates -> apply food
  auto final_state = applyFoodEffects(generateFoodUpdateOps(
      applyScoreEffects(generateFoodEatingOps(applySnakeOps(generateCollisionOps(applySnakeOps(generateSnakeMovementOps(
          applySnakeOps(provideDirectionOps(GameStateWithEffect::fromState(state_)))))))))));

  // Update internal state and publish
  state_ = final_state.state;

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

  // Initialize direction command filter for this player
  state_.direction_command[player_id] = DirectionCommandFilterState{};

  std::cout << "[GameSession] Initialized snake for '" << player_id << "' at y=" << y_pos << "\n";
}

void GameSession::initializeFood() {
  // Generate 5 random food items
  for (int i = 0; i < 5; ++i) {
    state_.food_items.push_back(with_board_and_snakes(state_, generateRandomFoodPosition));
  }
  std::cout << "[GameSession] Initialized " << state_.food_items.size() << " food items\n";
}

}  // namespace snake
