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
 * @brief Move a snake forward one step (pure, no effects)
 *
 * This subsystem handles snake movement, wrapping around board edges.
 * Returns the updated snake with empty effects.
 *
 * @param snake Current snake
 * @param board_width Width of game board
 * @param board_height Height of game board
 * @return Snake with empty effect
 */
static SnakeWithScoreEffect moveSnake(const Snake& snake, int board_width, int board_height) {
  Snake new_snake = snake;

  if (!new_snake.alive) {
    return {new_snake, ScoreDelta::empty()};
  }

  // Get next head position
  Point new_head = getNextHeadPosition(new_snake);

  // Wrap around board edges
  new_head = wrapPoint(new_head, board_width, board_height);

  // Build new tail: old head + old tail (minus last element to keep fixed length)
  std::vector<Point> new_tail;
  new_tail.reserve(new_snake.tail.size() + 1);
  new_tail.push_back(new_snake.head);
  new_tail.insert(new_tail.end(), new_snake.tail.begin(), new_snake.tail.end());

  // Remove last element to keep fixed length
  if (!new_tail.empty()) {
    new_tail.pop_back();
  }

  // Update snake with new head and tail
  new_snake.head = new_head;
  new_snake.tail = new_tail;

  return {new_snake, ScoreDelta::empty()};
}

/**
 * @brief Handle collision between two snakes - drop tail mode
 *
 * Uses pattern matching on collision cases. Cut tail segments are simply
 * dropped (not transformed to food). Returns all state changes as effects.
 *
 * @param a First player snake pair (player ID + snake)
 * @param b Second player snake pair (player ID + snake)
 * @return Game effects (scores, snake position updates)
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
    snake_a.alive = false;
    snake_b.alive = false;

    effect.scores = combine(effect.scores, ScoreDelta::forPlayer(player_a, -10));
    effect.scores = combine(effect.scores, ScoreDelta::forPlayer(player_b, -10));
    // Update both snakes (now dead)
    effect.snakes = combine(effect.snakes, SnakeUpdateEffect::updateSnake(player_a, snake_a));
    effect.snakes = combine(effect.snakes, SnakeUpdateEffect::updateSnake(player_b, snake_b));
  }
  // Case 2: Snake A bites Snake B
  else if (firstBitesSecond(snake_a, snake_b)) {
    auto cut = cutTailAt(snake_b, snake_a.head);
    snake_b = cut.snake;

    effect.scores = ScoreDelta::forPlayer(player_b, -10);
    // Update snake B (tail cut)
    effect.snakes = SnakeUpdateEffect::updateSnake(player_b, snake_b);
    // cut.cut_segments are ignored (dropped)
  }
  // Case 3: Snake B bites Snake A
  else if (firstBitesSecond(snake_b, snake_a)) {
    auto cut = cutTailAt(snake_a, snake_b.head);
    snake_a = cut.snake;

    effect.scores = ScoreDelta::forPlayer(player_a, -10);
    // Update snake A (tail cut)
    effect.snakes = SnakeUpdateEffect::updateSnake(player_a, snake_a);
    // cut.cut_segments are ignored (dropped)
  }
  // Case 4: No collision - effect already empty

  return effect;
}

/**
 * @brief Handle collision between two snakes - food drop mode
 *
 * Uses pattern matching on collision cases. Cut tail segments are
 * transformed into food items. Returns all state changes as effects.
 *
 * @param a First player snake pair (player ID + snake)
 * @param b Second player snake pair (player ID + snake)
 * @return Game effects (scores, food, snake position updates)
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
    snake_a.alive = false;
    snake_b.alive = false;

    effect.scores = combine(effect.scores, ScoreDelta::forPlayer(player_a, -10));
    effect.scores = combine(effect.scores, ScoreDelta::forPlayer(player_b, -10));
    // Transform entire snakes to food
    effect.food = combine(effect.food, FoodEffect::add(snake_a.toBody()));
    effect.food = combine(effect.food, FoodEffect::add(snake_b.toBody()));
    // Update both snakes (now dead)
    effect.snakes = combine(effect.snakes, SnakeUpdateEffect::updateSnake(player_a, snake_a));
    effect.snakes = combine(effect.snakes, SnakeUpdateEffect::updateSnake(player_b, snake_b));
  }
  // Case 2: Snake A bites Snake B
  else if (firstBitesSecond(snake_a, snake_b)) {
    auto cut = cutTailAt(snake_b, snake_a.head);
    snake_b = cut.snake;

    effect.scores = ScoreDelta::forPlayer(player_b, -10);
    // Transform cut segments to food
    effect.food = FoodEffect::add(cut.cut_segments);
    // Update snake B (tail cut)
    effect.snakes = SnakeUpdateEffect::updateSnake(player_b, snake_b);
  }
  // Case 3: Snake B bites Snake A
  else if (firstBitesSecond(snake_b, snake_a)) {
    auto cut = cutTailAt(snake_a, snake_b.head);
    snake_a = cut.snake;

    effect.scores = ScoreDelta::forPlayer(player_a, -10);
    // Transform cut segments to food
    effect.food = FoodEffect::add(cut.cut_segments);
    // Update snake A (tail cut)
    effect.snakes = SnakeUpdateEffect::updateSnake(player_a, snake_a);
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

  // Step 1: Move all alive snakes
  // Uses traversal to apply moveSnake to each alive snake, accumulating effects
  state_with_effect = over_each_alive_snake_combining_scores(
      state_with_effect,
      [this](const Snake& snake) { return moveSnake(snake, state_.board_width, state_.board_height); });

  // Apply movement effects to state before collision detection
  // (Collision needs to see the moved positions)
  for (const auto& [player_id, snake] : state_with_effect.effect.snakes.snake_updates) {
    state_with_effect.state.snakes[player_id] = snake;
  }
  // Clear snake effects after applying them
  state_with_effect.effect.snakes = SnakeUpdateEffect::empty();

  // Step 2: Handle collisions between selected snake pairs
  // For two-player game, check collision between player1 and player2
  // Dispatch based on collision mode
  if (state_with_effect.state.snakes.size() >= 2) {
    if (collision_mode_ == CollisionMode::BITE_DROP_FOOD) {
      state_with_effect = over_selected_snakes_combining_scores(state_with_effect, handleSnakeCollisionWithFoodDrop,
                                                                 "player1", "player2");
    } else {
      state_with_effect =
          over_selected_snakes_combining_scores(state_with_effect, handleSnakeCollision, "player1", "player2");
    }
  }

  // Step 3: Apply accumulated effects to the final state

  // Apply snake update effects - update all snakes that have changed
  for (const auto& [player_id, snake] : state_with_effect.effect.snakes.snake_updates) {
    state_with_effect.state.snakes[player_id] = snake;
  }

  // Apply score effects
  for (const auto& [player_id, delta] : state_with_effect.effect.scores.deltas) {
    state_with_effect.state.scores[player_id] += delta;
  }

  // Apply food effects - removals first, then additions
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
