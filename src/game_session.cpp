#include "snake/game_session.hpp"

#include <iostream>

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
static Point getNextHeadPosition(const SnakeState& snake) {
  if (snake.body.empty()) {
    return {0, 0};
  }

  Point head = snake.body[0];
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
 * @brief Move a snake forward one step (pure, no effects)
 *
 * This subsystem handles snake movement, wrapping around board edges.
 * Returns the updated snake state with empty effects.
 *
 * @param snake Current snake state
 * @param board_width Width of game board
 * @param board_height Height of game board
 * @return Snake state with empty effect
 */
static SnakeStateWithScoreEffect moveSnake(const SnakeState& snake, int board_width, int board_height) {
  SnakeState new_snake = snake;

  if (new_snake.body.empty() || !new_snake.alive) {
    return {new_snake, ScoreDelta::empty()};
  }

  // Get next head position
  Point new_head = getNextHeadPosition(new_snake);

  // Wrap around board edges
  new_head = wrapPoint(new_head, board_width, board_height);

  // Add new head at front
  new_snake.body.insert(new_snake.body.begin(), new_head);

  // Remove tail to keep fixed length
  new_snake.body.pop_back();

  return {new_snake, ScoreDelta::empty()};
}

/**
 * @brief Handle collision between two snakes using bite rule
 *
 * This subsystem applies the bite rule: if one snake's head hits another's body,
 * the bitten snake gets its tail cut off. This can potentially generate score effects
 * (e.g., penalty for being bitten, bonus for biting).
 *
 * @param snake_a First snake state
 * @param snake_b Second snake state
 * @return Tuple of (updated snake_a, updated snake_b, combined effects)
 */
static std::tuple<SnakeState, SnakeState, ScoreDelta> handleSnakeCollision(const SnakeState& snake_a,
                                                                           const SnakeState& snake_b) {
  SnakeState new_a = snake_a;
  SnakeState new_b = snake_b;
  ScoreDelta effect = ScoreDelta::empty();

  // Only check collisions if both snakes are alive
  if (!new_a.alive || !new_b.alive) {
    return {new_a, new_b, effect};
  }

  // Convert from vector<Point> to Snake type for collision detection
  auto snake_a_model = Snake::fromBody(new_a.body);
  auto snake_b_model = Snake::fromBody(new_b.body);

  // Both snakes must be valid (non-empty)
  if (!snake_a_model || !snake_b_model) {
    return {new_a, new_b, effect};
  }

  // Apply bite rule
  SnakePair snakes{*snake_a_model, *snake_b_model};
  SnakePair result = applyBiteRule(snakes);

  // Check if snake A got shorter (was bitten by B)
  if (result.a.length() < snakes.a.length()) {
    // Snake A was bitten - could add score penalty here
    effect = combine(effect, ScoreDelta::forPlayer(new_a.player_id, -10));
  }

  // Check if snake B got shorter (was bitten by A)
  if (result.b.length() < snakes.b.length()) {
    // Snake B was bitten - could add score penalty here
    effect = combine(effect, ScoreDelta::forPlayer(new_b.player_id, -10));
  }

  // Convert back to vector<Point>
  new_a.body = result.a.toBody();
  new_b.body = result.b.toBody();

  return {new_a, new_b, effect};
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
  GameStateAndScoreDelta state_and_score_delta = GameStateAndScoreDelta::fromState(state_);

  // Step 1: Move all alive snakes
  // Uses lens to apply moveSnake to each alive snake, accumulating effects
  state_and_score_delta = over_alive_snakes_with_combining_scores(
      state_and_score_delta,
      [this](const SnakeState& snake) { return moveSnake(snake, state_.board_width, state_.board_height); });

  // Step 2: Handle collisions between snake pairs
  // For two-player game, check collision between snakes 0 and 1
  if (state_and_score_delta.state.snakes.size() >= 2) {
    state_and_score_delta = over_snake_pair_with_effects(state_and_score_delta, 0, 1, handleSnakeCollision);
  }

  // Step 3: Apply accumulated score effects to the final state
  for (const auto& [player_id, delta] : state_and_score_delta.effect.deltas) {
    state_and_score_delta.state.scores[player_id] += delta;
  }

  // Update internal state and publish
  state_ = state_and_score_delta.state;

  StateUpdate update;
  update.state = state_;
  state_pub_->publish(update);
}

void GameSession::onDirectionChange(const DirectionChange& msg) {
  // Find the snake for this player
  for (auto& snake : state_.snakes) {
    if (snake.player_id == msg.player_id) {
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
      break;
    }
  }
}

void GameSession::initializeSnake(const PlayerId& player_id) {
  SnakeState snake;
  snake.player_id = player_id;
  snake.alive = true;
  snake.current_direction = Direction::RIGHT;

  // Position snakes at different y-levels
  int y_pos = (player_id == "player1") ? 10 : 15;
  int start_x = 5;

  // Create a snake of length 5, moving right
  // Body is stored with head at index 0
  for (int i = 0; i < 7; ++i) {
    snake.body.push_back({start_x - i, y_pos});
  }

  state_.snakes.push_back(snake);

  // Initialize score in GameState
  state_.scores[player_id] = 0;

  std::cout << "[GameSession] Initialized snake for '" << player_id << "' at y=" << y_pos << "\n";
}

}  // namespace snake
