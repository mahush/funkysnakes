#include "snake/game_engine_actor.hpp"

#include <funkyactors/effect_handler.hpp>
#include <optional>
#include <tuple>

#include "funkypipes/bind_front.hpp"
#include "funkypipes/details/tuple/separate_tuple_elements.hpp"
#include "funkypipes/make_pipe.hpp"
#include "snake/direction_command_filter.hpp"
#include "snake/food_operations.hpp"
#include "snake/functional_utils.hpp"
#include "snake/game_state_lenses.hpp"
#include "snake/game_state_views.hpp"
#include "snake/logger.hpp"
#include "snake/process_helpers.hpp"
#include "snake/snake_model.hpp"
#include "snake/snake_operations.hpp"
#include "snake/utility.hpp"

namespace snake {

using funkypipes::bindFront;
using funkypipes::makePipe;

// Game configuration constants
constexpr int MIN_FOOD_COUNT = 5;

namespace {

// ============================================================================
// Game Utility Functions
// ============================================================================

/**
 * @brief Check if game is in bite-drop-food collision mode
 *
 * @param state Current game state
 * @return true if collision mode is BITE_DROP_FOOD
 */
bool isBiteDropFoodMode(const GameState& state) { return state.collision_mode == CollisionMode::BITE_DROP_FOOD; }

/**
 * @brief Check if food should be repositioned this tick
 *
 * @param state Current game state
 * @return true if food reposition trigger was received
 */
bool shouldRepositionFood(const GameState& state) { return state.should_reposition_food; }

/**
 * @brief Clear the food reposition flag after processing
 *
 * @param state Current game state
 * @return Updated game state with reposition flag cleared
 */
GameState clearRepositionFlag(GameState state) {
  state.should_reposition_food = false;
  return state;
}

/**
 * @brief Try to generate PlayerAliveStatesMsg message if alive states changed
 *
 * Pure function that compares current alive states with previous ones.
 * If changed, generates a PlayerAliveStatesMsg message and updates previous state.
 *
 * @param state Current game state
 * @return Tuple of (updated state, optional PlayerAliveStatesMsg message)
 */
std::tuple<GameState, std::optional<PlayerAliveStatesMsg>> tryGeneratePlayerAliveStates(GameState state) {
  PerPlayerAliveStates current_alive_states = extractAliveStates(state.snakes);
  std::optional<PlayerAliveStatesMsg> msg;

  if (current_alive_states != state.previous_alive_states) {
    PlayerAliveStatesMsg alive_msg;
    alive_msg.game_id = state.game_id;
    alive_msg.alive_states = current_alive_states;
    msg = alive_msg;
    state.previous_alive_states = current_alive_states;
  }

  return {state, msg};
}

/**
 * @brief Handle a game tick - process game logic and return effects
 *
 * Pure function that processes one game tick and returns effects:
 * - Updated GameState
 * - RenderableStateMsg message to publish
 * - Optional PlayerAliveStatesMsg message (if alive states changed)
 *
 * @param state Current game state
 * @param event Timer elapsed event (unused, required for signature)
 * @return Tuple of (new GameState, RenderableStateMsg, optional PlayerAliveStatesMsg)
 */
std::tuple<GameState, RenderableStateMsg, std::optional<PlayerAliveStatesMsg>> handleTick(
    GameState state, const GameTimerElapsedEvent& /* event */) {
  // ============================================================================
  // GAME LOGIC PIPELINE - Functional Composition with funkypipes
  // ============================================================================
  // makePipe automatically unpacks tuples between stages
  // When a function returns tuple<A, B>, the next function receives (A, B) as separate args
  // Lenses are decorators that return state transformers

  // clang-format off
  auto tick_pipeline = makePipe(
      over_direction_command_filter_state(direction_command_filter::try_consume_next),                         // → (state, next_directions)
      over_snakes(applyDirectionMsgs),                                                                      // → state
      over_snakes_viewing_board_and_food(moveSnakes),                                                             // → state
      over_snakes_and_scores(handleCollisions),                                                                // → (state, cut_tails)
      when<0>(isBiteDropFoodMode, over_food(dropCutTailsAsFood)),                                              // → state
      when(isBiteDropFoodMode, over_food_viewing_snakes(dropDeadSnakesAsFood)),                                   // → state
      over_food_and_scores_viewing_snakes(handleFoodEating),                                                      // → state
      over_food_viewing_board_and_snakes(bindFront(replenishFood, makeRandomIntGenerator(), MIN_FOOD_COUNT)),     // → state
      when(shouldRepositionFood,
           over_food_viewing_board_and_snakes(bindFront(repositionRandomFood, makeRandomIntGenerator()))),        // → state
      clearRepositionFlag);                                                                                     // → state (clear flag)
  // clang-format on

  state = tick_pipeline(state);

  // Check if alive states changed and generate message if so
  auto [state_with_updated_alive, alive_msg] = tryGeneratePlayerAliveStates(state);
  state = state_with_updated_alive;

  // Build renderable state from game state (visual elements only)
  RenderableStateMsg renderable{
      state.board,
      state.food_items,
      state.snakes,
      state.scores,
  };

  return std::make_tuple(state, renderable, alive_msg);
}

/**
 * @brief Handle game clock command (start/stop/pause/resume)
 *
 * Pure function that returns state, timer command effect, and log message effect.
 *
 * @param state Current game state
 * @param msg Clock command message
 * @return Tuple of (state, timer command effect, log message effect)
 */
std::tuple<GameState, GameTimerCommand, LogMsg> handleGameClockCommand(GameState state,
                                                                       const GameClockCommandMsg& msg) {
  GameTimerCommand timer_cmd;
  LogMsg log_msg;

  switch (msg.state) {
    case GameClockState::START:
      log_msg = {"[GameEngineActor] Starting internal timer\n"};
      timer_cmd = make_periodic_command<GameTimerTag>(std::chrono::milliseconds(state.interval_ms));
      break;

    case GameClockState::STOP:
      log_msg = {"[GameEngineActor] Stopping internal timer\n"};
      timer_cmd = make_cancel_command<GameTimerTag>();
      break;

    case GameClockState::PAUSE:
      log_msg = {"[GameEngineActor] Pausing game\n"};
      timer_cmd = make_cancel_command<GameTimerTag>();
      break;

    case GameClockState::RESUME:
      log_msg = {"[GameEngineActor] Resuming game\n"};
      timer_cmd = make_periodic_command<GameTimerTag>(std::chrono::milliseconds(state.interval_ms));
      break;
  }

  return std::make_tuple(state, timer_cmd, log_msg);
}

/**
 * @brief Handle tick rate change
 *
 * Pure function that returns state, timer command effect, and log message effect.
 *
 * @param state Current game state
 * @param msg TickMsg rate change message
 * @return Tuple of (updated state, timer command effect, log message effect)
 */
std::tuple<GameState, GameTimerCommand, LogMsg> handleTickRateChange(GameState state, const TickRateChangeMsg& msg) {
  state.interval_ms = msg.interval_ms;

  // Return periodic command to restart timer with new interval
  GameTimerCommand timer_cmd = make_periodic_command<GameTimerTag>(std::chrono::milliseconds(state.interval_ms));

  LogMsg log_msg = {"[GameEngineActor] Changing tick rate to " + std::to_string(msg.interval_ms) + "ms\n"};

  return std::make_tuple(state, timer_cmd, log_msg);
}

/**
 * @brief Set food reposition flag
 *
 * @param state Current game state
 * @param trigger Food reposition trigger (contains game_id for validation)
 * @return Updated game state with reposition flag set if game_id matches
 */
GameState setFoodRepositionFlag(GameState state, const FoodRepositionTriggerMsg& trigger) {
  // Only set flag if trigger is for current game (ignore stale triggers)
  if (trigger.game_id == state.game_id) {
    state.should_reposition_food = true;
  }
  return state;
}

/**
 * @brief Handle game state summary request
 *
 * Pure function that builds a summary response from current state.
 * Returns only data that GameEngineActor manages (scores, alive states).
 *
 * @param state Current game state
 * @param request Summary request (unused, required for signature)
 * @return Tuple of (unchanged state, summary response)
 */
std::tuple<GameState, GameStateSummaryResponseMsg> handleSummaryRequest(
    GameState state, const GameStateSummaryRequestMsg& /* request */) {
  GameStateSummaryResponseMsg response;
  response.scores = state.scores;
  response.alive_states = extractAliveStates(state.snakes);
  return {state, response};
}

// ============================================================================
// Effect Handler for GameEngineActor
// ============================================================================

/**
 * @brief Effect handler for GameEngineActor effects
 *
 * Interprets effects returned from handler functions (excluding the GameState):
 * - RenderableStateMsg: Publishes to renderer topic
 * - GameTimerCommand: Executes timer commands
 * - LogMsg: Logs messages to console
 * - std::optional<PlayerAliveStatesMsg>: Publishes if present
 * - GameStateSummaryResponseMsg: Publishes to summary response topic
 */
class GameEngineEffectHandler {
 public:
  GameEngineEffectHandler(PublisherPtr<RenderableStateMsg> renderable_pub,
                          PublisherPtr<PlayerAliveStatesMsg> alive_pub,
                          PublisherPtr<GameStateSummaryResponseMsg> summary_resp_pub,
                          GameTimerPtr timer)
      : renderable_pub_(renderable_pub), alive_pub_(alive_pub), summary_resp_pub_(summary_resp_pub), timer_(timer) {}

  // Handle RenderableStateMsg effect: publish to renderer
  void handle(const RenderableStateMsg& msg) { renderable_pub_->publish(msg); }

  // Handle GameTimerCommand effect: execute timer command
  void handle(const GameTimerCommand& cmd) { timer_->execute_command(cmd); }

  // Handle LogMsg effect: log to file
  void handle(const LogMsg& log) { Logger::log(log.message); }

  // Handle optional PlayerAliveStatesMsg effect: publish if present
  void handle(const std::optional<PlayerAliveStatesMsg>& msg) {
    if (msg) {
      alive_pub_->publish(*msg);
    }
  }

  // Handle GameStateSummaryResponseMsg effect: publish to summary response topic
  void handle(const GameStateSummaryResponseMsg& msg) { summary_resp_pub_->publish(msg); }

 private:
  PublisherPtr<RenderableStateMsg> renderable_pub_;
  PublisherPtr<PlayerAliveStatesMsg> alive_pub_;
  PublisherPtr<GameStateSummaryResponseMsg> summary_resp_pub_;
  GameTimerPtr timer_;
};

}  // namespace

// ============================================================================
// GameEngineActor implementation
// ============================================================================

GameEngineActor::GameEngineActor(ActorContext ctx,
                                 TopicPtr<DirectionMsg> direction_topic,
                                 TopicPtr<RenderableStateMsg> state_topic,
                                 TopicPtr<GameClockCommandMsg> clock_topic,
                                 TopicPtr<TickRateChangeMsg> tickrate_topic,
                                 TopicPtr<FoodRepositionTriggerMsg> reposition_topic,
                                 TopicPtr<PlayerAliveStatesMsg> alivests_topic,
                                 TopicPtr<GameStateSummaryRequestMsg> summary_req_topic,
                                 TopicPtr<GameStateSummaryResponseMsg> summary_resp_topic,
                                 TimerFactoryPtr timer_factory)
    : Actor{ctx},
      renderable_state_pub_{create_pub(state_topic)},
      alive_states_pub_{create_pub(alivests_topic)},
      summary_resp_pub_{create_pub(summary_resp_topic)},
      direction_sub_{create_sub(direction_topic)},
      clock_sub_{create_sub(clock_topic)},
      tickrate_sub_{create_sub(tickrate_topic)},
      reposition_sub_{create_sub(reposition_topic)},
      summary_req_sub_{create_sub(summary_req_topic)},
      game_loop_timer_{create_timer<GameTimer>(timer_factory)} {
  game_state_.game_id = "game_001";
  game_state_.board.width = 60;
  game_state_.board.height = 20;

  // Initialize players
  applyToState(game_state_,
               over_snakes_and_scores(bindFront(addPlayer, PlayerId{PLAYER_A}, Point{5, 10}, Direction::RIGHT, 7)));
  applyToState(game_state_,
               over_snakes_and_scores(bindFront(addPlayer, PlayerId{PLAYER_B}, Point{5, 15}, Direction::RIGHT, 7)));

  // Initialize food items
  applyToState(game_state_,
               over_food_viewing_board_and_snakes(bindFront(initializeFood, makeRandomIntGenerator(), MIN_FOOD_COUNT)));

  Logger::log("[GameEngineActor] Initialized " + std::to_string(game_state_.food_items.size()) + " food items\n");
}

void GameEngineActor::processInputs() {
  // Drain direction commands into filtered queues
  processMessageWithState(direction_sub_,
                          game_state_,
                          over_direction_command_filter_state_viewing_snakes(direction_command_filter::try_add));

  // Create effect handler for messages that produce effects
  GameEngineEffectHandler effect_handler(renderable_state_pub_, alive_states_pub_, summary_resp_pub_, game_loop_timer_);

  // Process timer events with effect handler pattern
  processEventWithState(game_loop_timer_, game_state_, handleTick, effect_handler);

  // Process clock commands with effect handler pattern
  processMessageWithState(clock_sub_, game_state_, handleGameClockCommand, effect_handler);

  // Process tick rate changes with effect handler pattern
  processMessageWithState(tickrate_sub_, game_state_, handleTickRateChange, effect_handler);

  // Process food reposition triggers (pure, no effects)
  processMessageWithState(reposition_sub_, game_state_, setFoodRepositionFlag);

  // Process game state summary requests with effect handler pattern
  processMessageWithState(summary_req_sub_, game_state_, handleSummaryRequest, effect_handler);
}

}  // namespace snake
