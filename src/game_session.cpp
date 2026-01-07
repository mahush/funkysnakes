#include "snake/game_session.hpp"

#include <iostream>
#include <random>

#include "funkypipes/bind_front.hpp"
#include "funkypipes/make_pipe.hpp"
#include "snake/direction_command_filter.hpp"
#include "snake/food_operations.hpp"
#include "snake/functional_utils.hpp"
#include "snake/game_state_lenses.hpp"
#include "snake/game_state_views.hpp"
#include "snake/process_helpers.hpp"
#include "snake/snake_model.hpp"
#include "snake/snake_operations.hpp"
#include "snake/timer/timer_factory.hpp"

namespace snake {

using funkypipes::bindFront;
using funkypipes::makePipe;

// Game configuration constants
constexpr int MIN_FOOD_COUNT = 5;

// ============================================================================
// Game Utility Functions
// ============================================================================

/**
 * @brief Check if game is in bite-drop-food collision mode
 *
 * @param state Current game state
 * @return true if collision mode is BITE_DROP_FOOD
 */
static bool isBiteDropFoodMode(const GameState& state) { return state.collision_mode == CollisionMode::BITE_DROP_FOOD; }

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
      over_food_with_board_and_snakes(bindFront(replenishFood, random_fn, MIN_FOOD_COUNT)),             // → state
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

  // Generate MIN_FOOD_COUNT random food items
  auto generate_food = with_board_and_snakes(generateRandomFoodPosition);
  for (int i = 0; i < MIN_FOOD_COUNT; ++i) {
    state_.food_items.push_back(generate_food(state_, random_int));
  }
  std::cout << "[GameSession] Initialized " << state_.food_items.size() << " food items\n";
}

}  // namespace snake
