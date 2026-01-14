#include "snake/game_engine.hpp"

#include <iostream>

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
#include "snake/utility.hpp"

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

/**
 * @brief Check if food should be repositioned this tick
 *
 * @param state Current game state
 * @return true if food reposition trigger was received
 */
static bool shouldRepositionFood(const GameState& state) { return state.should_reposition_food; }

/**
 * @brief Clear the food reposition flag after processing
 *
 * @param state Current game state
 * @return Updated game state with reposition flag cleared
 */
static GameState clearRepositionFlag(GameState state) {
  state.should_reposition_food = false;
  return state;
}

/**
 * @brief Handle a game tick - process game logic and publish state
 *
 * @param state_pub Publisher for state updates
 * @param state Current game state
 * @param event Timer elapsed event (unused, required for signature)
 * @return Updated game state after tick processing
 */
static GameState handleTick(PublisherPtr<StateUpdate> state_pub, GameState state, const GameTimerElapsedEvent& /* event */) {
  // ============================================================================
  // GAME LOGIC PIPELINE - Functional Composition with funkypipes
  // ============================================================================
  // makePipe automatically unpacks tuples between stages
  // When a function returns tuple<A, B>, the next function receives (A, B) as separate args
  // Lenses are decorators that return state transformers

  // clang-format off
  auto tick_pipeline = makePipe(
      over_pending_directions(direction_command_filter::try_consume_next),                                     // → (state, next_directions)
      over_snakes(applyDirectionChanges),                                                                      // → state
      over_snakes_with_board_and_food(moveSnakes),                                                             // → state
      over_snakes_and_scores(handleCollisions),                                                                // → (state, cut_tails)
      when<0>(isBiteDropFoodMode, over_food(dropCutTailsAsFood)),                                              // → state
      when(isBiteDropFoodMode, over_food_with_snakes(dropDeadSnakesAsFood)),                                   // → state
      over_food_and_scores_with_snakes(handleFoodEating),                                                      // → state
      over_food_with_board_and_snakes(bindFront(replenishFood, makeRandomIntGenerator(), MIN_FOOD_COUNT)),     // → state
      when(shouldRepositionFood,
           over_food_with_board_and_snakes(bindFront(repositionRandomFood, makeRandomIntGenerator()))),        // → state
      clearRepositionFlag);                                                                                     // → state (clear flag)
  // clang-format on

  state = tick_pipeline(state);
  state_pub->publish(StateUpdate{state});
  return state;
}

/**
 * @brief Handle game clock command (start/stop/pause/resume)
 *
 * @param timer Game timer to control
 * @param state Current game state
 * @param msg Clock command message
 * @return Updated game state (unchanged)
 */
static GameState handleGameClockCommand(GameTimerPtr timer, GameState state, const GameClockCommand& msg) {
  switch (msg.state) {
    case GameClockState::START:
      std::cout << "[GameEngine] Starting internal timer\n";
      timer->execute_command(make_periodic_command<GameTimerTag>(std::chrono::milliseconds(state.interval_ms)));
      break;

    case GameClockState::STOP:
      std::cout << "[GameEngine] Stopping internal timer\n";
      timer->execute_command(make_cancel_command<GameTimerTag>());
      break;

    case GameClockState::PAUSE:
      std::cout << "[GameEngine] Pausing game\n";
      timer->execute_command(make_cancel_command<GameTimerTag>());
      break;

    case GameClockState::RESUME:
      std::cout << "[GameEngine] Resuming game\n";
      timer->execute_command(make_periodic_command<GameTimerTag>(std::chrono::milliseconds(state.interval_ms)));
      break;
  }

  return state;
}

/**
 * @brief Handle tick rate change
 *
 * @param timer Game timer to control
 * @param state Current game state
 * @param msg Tick rate change message
 * @return Updated game state with new interval
 */
static GameState handleTickRateChange(GameTimerPtr timer, GameState state, const TickRateChange& msg) {
  std::cout << "[GameEngine] Changing tick rate to " << msg.interval_ms << "ms\n";

  state.interval_ms = msg.interval_ms;

  // If timer is currently running, restart it with new interval
  if (timer->is_scheduled()) {
    timer->execute_command(make_periodic_command<GameTimerTag>(std::chrono::milliseconds(state.interval_ms)));
  }

  return state;
}

/**
 * @brief Handle level change
 *
 * @param state Current game state
 * @param msg Level change message
 * @return Updated game state with new level
 */
static GameState handleLevelChange(GameState state, const LevelChange& msg) {
  std::cout << "[GameEngine] Level changed to " << msg.new_level << "\n";
  state.level = msg.new_level;
  return state;
}

/**
 * @brief Set food reposition flag
 *
 * @param state Current game state
 * @param trigger Food reposition trigger (contains game_id for validation)
 * @return Updated game state with reposition flag set if game_id matches
 */
static GameState setFoodRepositionFlag(GameState state, const FoodRepositionTrigger& trigger) {
  // Only set flag if trigger is for current game (ignore stale triggers)
  if (trigger.game_id == state.game_id) {
    state.should_reposition_food = true;
  }
  return state;
}

// ============================================================================
// GameEngine implementation
// ============================================================================

GameEngine::GameEngine(Actor<GameEngine>::ActorContext ctx, TopicPtr<DirectionChange> direction_topic,
                       TopicPtr<StateUpdate> state_topic, TopicPtr<GameClockCommand> clock_topic,
                       TopicPtr<TickRateChange> tickrate_topic, TopicPtr<LevelChange> levelchange_topic,
                       TopicPtr<FoodRepositionTrigger> reposition_topic, TimerFactoryPtr timer_factory)
    : Actor(ctx),
      state_pub_(create_pub(state_topic)),
      direction_sub_(create_sub(direction_topic)),
      clock_sub_(create_sub(clock_topic)),
      tickrate_sub_(create_sub(tickrate_topic)),
      levelchange_sub_(create_sub(levelchange_topic)),
      reposition_sub_(create_sub(reposition_topic)),
      timer_(create_timer<GameTimer>(timer_factory)) {
  state_.game_id = "game_001";
  state_.board.width = 60;
  state_.board.height = 20;

  // Initialize players
  apply_to_state(state_, over_snakes_scores_and_pending_directions(
                             bindFront(addPlayer, PlayerId{"player1"}, Point{5, 10}, Direction::RIGHT, 7)));
  apply_to_state(state_, over_snakes_scores_and_pending_directions(
                             bindFront(addPlayer, PlayerId{"player2"}, Point{5, 15}, Direction::RIGHT, 7)));

  // Initialize food items
  apply_to_state(state_,
                 over_food_with_board_and_snakes(bindFront(initializeFood, makeRandomIntGenerator(), MIN_FOOD_COUNT)));

  std::cout << "[GameEngine] Initialized " << state_.food_items.size() << " food items\n";
}

void GameEngine::processMessages() {
  // Drain direction commands into filtered queues
  process_message_with_state(direction_sub_, state_,
                             over_pending_directions_with_snakes(direction_command_filter::try_add));

  process_event_with_state(timer_, state_, bindFront(handleTick, state_pub_));

  process_message_with_state(clock_sub_, state_, bindFront(handleGameClockCommand, timer_));

  process_message_with_state(tickrate_sub_, state_, bindFront(handleTickRateChange, timer_));

  process_message_with_state(levelchange_sub_, state_, handleLevelChange);

  process_message_with_state(reposition_sub_, state_, setFoodRepositionFlag);
}

}  // namespace snake
