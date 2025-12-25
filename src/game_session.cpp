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
static Point getNextHeadPosition(const SnakeState& snake_state) {
  Point head = snake_state.snake.head;
  Point next = head;

  switch (snake_state.current_direction) {
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
 * @param snake_state Current snake state
 * @param board_width Width of game board
 * @param board_height Height of game board
 * @return Snake state with empty effect
 */
static SnakeStateWithScoreEffect moveSnake(const SnakeState& snake_state, int board_width, int board_height) {
  SnakeState new_state = snake_state;

  if (!new_state.alive) {
    return {new_state, ScoreDelta::empty()};
  }

  // Get next head position
  Point new_head = getNextHeadPosition(new_state);

  // Wrap around board edges
  new_head = wrapPoint(new_head, board_width, board_height);

  // Build new tail: old head + old tail (minus last element to keep fixed length)
  std::vector<Point> new_tail;
  new_tail.reserve(new_state.snake.tail.size() + 1);
  new_tail.push_back(new_state.snake.head);
  new_tail.insert(new_tail.end(), new_state.snake.tail.begin(), new_state.snake.tail.end());

  // Remove last element to keep fixed length
  if (!new_tail.empty()) {
    new_tail.pop_back();
  }

  // Create new snake with updated head and tail
  new_state.snake = Snake{new_head, new_tail};

  return {new_state, ScoreDelta::empty()};
}

/**
 * @brief Handle collision between two snakes using bite rule
 *
 * This subsystem applies the bite rule: if one snake's head hits another's body,
 * the bitten snake gets its tail cut off. This can potentially generate score effects
 * (e.g., penalty for being bitten, bonus for biting).
 *
 * @param a First player snake pair (player ID + snake state)
 * @param b Second player snake pair (player ID + snake state)
 * @return Tuple of (updated snake_a, updated snake_b, combined effects)
 */
static std::tuple<SnakeState, SnakeState, ScoreDelta> handleSnakeCollision(std::pair<PlayerId, SnakeState> a,
                                                                           std::pair<PlayerId, SnakeState> b) {
  auto [player_a, snake_a] = a;
  auto [player_b, snake_b] = b;
  ScoreDelta effect = ScoreDelta::empty();

  // Only check collisions if both snakes are alive
  if (!snake_a.alive || !snake_b.alive) {
    return {snake_a, snake_b, effect};
  }

  // Apply bite rule directly (no conversions needed!)
  SnakePair snakes{snake_a.snake, snake_b.snake};
  SnakePair result = applyBiteRule(snakes);

  // Check if snake A got shorter (was bitten by B)
  if (result.a.length() < snakes.a.length()) {
    // Snake A was bitten - apply score penalty
    effect = combine(effect, ScoreDelta::forPlayer(player_a, -10));
  }

  // Check if snake B got shorter (was bitten by A)
  if (result.b.length() < snakes.b.length()) {
    // Snake B was bitten - apply score penalty
    effect = combine(effect, ScoreDelta::forPlayer(player_b, -10));
  }

  // Update snake states with collision results
  snake_a.snake = result.a;
  snake_b.snake = result.b;

  return {snake_a, snake_b, effect};
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
  // Uses traversal to apply moveSnake to each alive snake, accumulating effects
  state_and_score_delta = over_each_alive_snake_combining_scores(
      state_and_score_delta,
      [this](const SnakeState& snake) { return moveSnake(snake, state_.board_width, state_.board_height); });

  // Step 2: Handle collisions between selected snake pairs
  // For two-player game, check collision between player1 and player2
  if (state_and_score_delta.state.snakes.size() >= 2) {
    state_and_score_delta =
        over_selected_snakes_combining_scores(state_and_score_delta, handleSnakeCollision, "player1", "player2");
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
  auto it = state_.snakes.find(msg.player_id);
  if (it == state_.snakes.end()) {
    return;  // Player not found
  }

  SnakeState& snake_state = it->second;

  // Prevent 180-degree turns (going back into yourself)
  Direction new_dir = msg.new_direction;
  Direction current = snake_state.current_direction;

  bool is_opposite = (current == Direction::UP && new_dir == Direction::DOWN) ||
                     (current == Direction::DOWN && new_dir == Direction::UP) ||
                     (current == Direction::LEFT && new_dir == Direction::RIGHT) ||
                     (current == Direction::RIGHT && new_dir == Direction::LEFT);

  if (!is_opposite) {
    snake_state.current_direction = msg.new_direction;
  }
}

void GameSession::initializeSnake(const PlayerId& player_id) {
  SnakeState snake_state;
  snake_state.alive = true;
  snake_state.current_direction = Direction::RIGHT;

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

  snake_state.snake = Snake{head, tail};

  // Insert snake into map using player_id as key
  state_.snakes[player_id] = snake_state;

  // Initialize score in GameState
  state_.scores[player_id] = 0;

  std::cout << "[GameSession] Initialized snake for '" << player_id << "' at y=" << y_pos << "\n";
}

}  // namespace snake
