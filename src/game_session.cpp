#include "snake/game_session.hpp"

#include <iostream>

#include "snake/functional_utils.hpp"
#include "snake/game_state_lenses.hpp"
#include "snake/snake_model.hpp"
#include "snake/state_with_effect.hpp"

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
// GameSession implementation
// ============================================================================

GameSession::GameSession(asio::io_context& io, TopicPtr<Tick> tick_topic, TopicPtr<DirectionChange> direction_topic,
                         TopicPtr<StateUpdate> state_topic)
    : Actor(io),
      state_pub_(create_pub(state_topic)),
      tick_sub_(create_sub(tick_topic)),
      direction_sub_(create_sub(direction_topic)) {
  state_.game_id = "game_001";
  state_.board_width = 60;
  state_.board_height = 20;

  // Initialize snakes for player1 and player2
  initializeSnake("player1");
  initializeSnake("player2");
}

void GameSession::processMessages() {
  // Process ticks
  while (auto tick = tick_sub_->tryReceive()) {
    onTick(*tick);
  }

  // Process direction changes
  while (auto dir = direction_sub_->tryReceive()) {
    onDirectionChange(*dir);
  }
}

void GameSession::onTick(const Tick&) {
  // Start with current state wrapped in StateWithEffect
  GameStateWithEffect state_with_effect = GameStateWithEffect::fromState(state_);

  // Step 1: Generate MOVE operations for all alive snakes
  for (const auto& [player_id, snake] : state_with_effect.state.snakes) {
    if (snake.alive) {
      state_with_effect.effect.snakes.addOp(player_id, SnakeOperation::move());
    }
  }

  // Step 2: Generate collision operations
  // For two-player game, check collision between player1 and player2
  // Dispatch based on collision mode
  if (state_with_effect.state.snakes.size() >= 2) {
    GameEffect collision_effect;
    if (collision_mode_ == CollisionMode::BITE_DROP_FOOD) {
      collision_effect = over_selected_snakes_combining_scores(GameStateWithEffect::fromState(state_with_effect.state),
                                                                handleSnakeCollisionWithFoodDrop, "player1", "player2")
                             .effect;
    } else {
      collision_effect =
          over_selected_snakes_combining_scores(GameStateWithEffect::fromState(state_with_effect.state),
                                                handleSnakeCollision, "player1", "player2")
              .effect;
    }
    state_with_effect.effect = combine(state_with_effect.effect, collision_effect);
  }

  // Step 3: Apply all accumulated operations to snakes
  for (const auto& [player_id, ops] : state_with_effect.effect.snakes.operations) {
    Snake snake = state_with_effect.state.snakes[player_id];
    for (const auto& op : ops) {
      snake = applyOperation(snake, op, state_.board_width, state_.board_height);
    }
    state_with_effect.state.snakes[player_id] = snake;
  }

  // Step 4: Apply score effects
  for (const auto& [player_id, delta] : state_with_effect.effect.scores.deltas) {
    state_with_effect.state.scores[player_id] += delta;
  }

  // Step 5: Apply food effects - removals first, then additions
  for (const Point& food_loc : state_with_effect.effect.food.removals) {
    auto& items = state_with_effect.state.food_items;
    auto it = std::find(items.begin(), items.end(), food_loc);
    if (it != items.end()) {
      items.erase(it);
    }
  }
  for (const Point& food_loc : state_with_effect.effect.food.additions) {
    state_with_effect.state.food_items.push_back(food_loc);
  }

  // Update internal state and publish
  state_ = state_with_effect.state;

  StateUpdate update;
  update.state = state_;
  state_pub_->publish(update);
}

void GameSession::onDirectionChange(const DirectionChange& msg) {
  // Find the snake for this player
  auto it = state_.snakes.find(msg.player_id);
  if (it == state_.snakes.end()) {
    return;  // Player not found
  }

  Snake& snake = it->second;

  // Prevent 180-degree turns (going back into yourself)
  Direction new_dir = msg.new_direction;
  Direction current = snake.current_direction;

  bool is_opposite = (current == Direction::UP && new_dir == Direction::DOWN) ||
                     (current == Direction::DOWN && new_dir == Direction::UP) ||
                     (current == Direction::LEFT && new_dir == Direction::RIGHT) ||
                     (current == Direction::RIGHT && new_dir == Direction::LEFT);

  if (!is_opposite) {
    snake.current_direction = msg.new_direction;
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

}  // namespace snake
