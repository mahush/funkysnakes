#include "snake/game_engine.hpp"

#include <optional>
#include <tuple>

#include "actor-core/effect_handler.hpp"
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
 * @brief Try to generate PlayerAliveStates message if alive states changed
 *
 * Pure function that compares current alive states with previous ones.
 * If changed, generates a PlayerAliveStates message and updates previous state.
 *
 * @param state Current game state
 * @return Tuple of (updated state, optional PlayerAliveStates message)
 */
static std::tuple<GameState, std::optional<PlayerAliveStates>> tryGeneratePlayerAliveStates(GameState state) {
  PerPlayerAliveStates current_alive_states = extractAliveStates(state.snakes);
  std::optional<PlayerAliveStates> msg;

  if (current_alive_states != state.previous_alive_states) {
    PlayerAliveStates alive_msg;
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
 * - RenderableState message to publish
 * - Optional PlayerAliveStates message (if alive states changed)
 *
 * @param state Current game state
 * @param event Timer elapsed event (unused, required for signature)
 * @return Tuple of (new GameState, RenderableState, optional PlayerAliveStates)
 */
static std::tuple<GameState, RenderableState, std::optional<PlayerAliveStates>> handleTick(
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
      over_snakes(applyDirectionChanges),                                                                      // → state
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
  RenderableState renderable{
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
static std::tuple<GameState, GameTimerCommand, LogMessage> handleGameClockCommand(GameState state,
                                                                                  const GameClockCommand& msg) {
  GameTimerCommand timer_cmd;
  LogMessage log_msg;

  switch (msg.state) {
    case GameClockState::START:
      log_msg = {"[GameEngine] Starting internal timer\n"};
      timer_cmd = make_periodic_command<GameTimerTag>(std::chrono::milliseconds(state.interval_ms));
      break;

    case GameClockState::STOP:
      log_msg = {"[GameEngine] Stopping internal timer\n"};
      timer_cmd = make_cancel_command<GameTimerTag>();
      break;

    case GameClockState::PAUSE:
      log_msg = {"[GameEngine] Pausing game\n"};
      timer_cmd = make_cancel_command<GameTimerTag>();
      break;

    case GameClockState::RESUME:
      log_msg = {"[GameEngine] Resuming game\n"};
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
 * @param msg Tick rate change message
 * @return Tuple of (updated state, timer command effect, log message effect)
 */
static std::tuple<GameState, GameTimerCommand, LogMessage> handleTickRateChange(GameState state,
                                                                                const TickRateChange& msg) {
  state.interval_ms = msg.interval_ms;

  // Return periodic command to restart timer with new interval
  GameTimerCommand timer_cmd = make_periodic_command<GameTimerTag>(std::chrono::milliseconds(state.interval_ms));

  LogMessage log_msg = {"[GameEngine] Changing tick rate to " + std::to_string(msg.interval_ms) + "ms\n"};

  return std::make_tuple(state, timer_cmd, log_msg);
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

/**
 * @brief Handle game state summary request
 *
 * Pure function that builds a summary response from current state.
 * Returns only data that GameEngine manages (scores, alive states).
 *
 * @param state Current game state
 * @param request Summary request (unused, required for signature)
 * @return Tuple of (unchanged state, summary response)
 */
static std::tuple<GameState, GameStateSummaryResponse> handleSummaryRequest(
    GameState state, const GameStateSummaryRequest& /* request */) {
  GameStateSummaryResponse response;
  response.scores = state.scores;
  response.alive_states = extractAliveStates(state.snakes);
  return {state, response};
}

// ============================================================================
// Effect Handler for GameEngine
// ============================================================================

/**
 * @brief Effect handler for GameEngine effects
 *
 * Interprets effects returned from handler functions (excluding the GameState):
 * - RenderableState: Publishes to renderer topic
 * - GameTimerCommand: Executes timer commands
 * - LogMessage: Logs messages to console
 * - std::optional<PlayerAliveStates>: Publishes if present
 * - GameStateSummaryResponse: Publishes to summary response topic
 */
class GameEngineEffectHandler {
 public:
  GameEngineEffectHandler(PublisherPtr<RenderableState> renderable_pub, PublisherPtr<PlayerAliveStates> alive_pub,
                          PublisherPtr<GameStateSummaryResponse> summary_resp_pub, GameTimerPtr timer)
      : renderable_pub_(renderable_pub), alive_pub_(alive_pub), summary_resp_pub_(summary_resp_pub), timer_(timer) {}

  // Handle RenderableState effect: publish to renderer
  void handle(const RenderableState& msg) { renderable_pub_->publish(msg); }

  // Handle GameTimerCommand effect: execute timer command
  void handle(const GameTimerCommand& cmd) { timer_->execute_command(cmd); }

  // Handle LogMessage effect: log to file
  void handle(const LogMessage& log) { Logger::log(log.message); }

  // Handle optional PlayerAliveStates effect: publish if present
  void handle(const std::optional<PlayerAliveStates>& msg) {
    if (msg) {
      alive_pub_->publish(*msg);
    }
  }

  // Handle GameStateSummaryResponse effect: publish to summary response topic
  void handle(const GameStateSummaryResponse& msg) { summary_resp_pub_->publish(msg); }

 private:
  PublisherPtr<RenderableState> renderable_pub_;
  PublisherPtr<PlayerAliveStates> alive_pub_;
  PublisherPtr<GameStateSummaryResponse> summary_resp_pub_;
  GameTimerPtr timer_;
};

// ============================================================================
// GameEngine implementation
// ============================================================================

GameEngine::GameEngine(ActorContext ctx, TopicPtr<DirectionChange> direction_topic,
                       TopicPtr<RenderableState> state_topic, TopicPtr<GameClockCommand> clock_topic,
                       TopicPtr<TickRateChange> tickrate_topic, TopicPtr<FoodRepositionTrigger> reposition_topic,
                       TopicPtr<PlayerAliveStates> alivests_topic, TopicPtr<GameStateSummaryRequest> summary_req_topic,
                       TopicPtr<GameStateSummaryResponse> summary_resp_topic, TimerFactoryPtr timer_factory)
    : Actor(ctx),
      renderable_state_pub_(create_pub(state_topic)),
      alive_states_pub_(create_pub(alivests_topic)),
      summary_resp_pub_(create_pub(summary_resp_topic)),
      direction_sub_(create_sub(direction_topic)),
      clock_sub_(create_sub(clock_topic)),
      tickrate_sub_(create_sub(tickrate_topic)),
      reposition_sub_(create_sub(reposition_topic)),
      summary_req_sub_(create_sub(summary_req_topic)),
      timer_(create_timer<GameTimer>(timer_factory)) {
  state_.game_id = "game_001";
  state_.board.width = 60;
  state_.board.height = 20;

  // Initialize players
  apply_to_state(state_,
                 over_snakes_and_scores(bindFront(addPlayer, PlayerId{PLAYER_A}, Point{5, 10}, Direction::RIGHT, 7)));
  apply_to_state(state_,
                 over_snakes_and_scores(bindFront(addPlayer, PlayerId{PLAYER_B}, Point{5, 15}, Direction::RIGHT, 7)));

  // Initialize food items
  apply_to_state(
      state_, over_food_viewing_board_and_snakes(bindFront(initializeFood, makeRandomIntGenerator(), MIN_FOOD_COUNT)));

  Logger::log("[GameEngine] Initialized " + std::to_string(state_.food_items.size()) + " food items\n");
}

void GameEngine::processInputs() {
  // Drain direction commands into filtered queues
  process_message_with_state(direction_sub_, state_,
                             over_direction_command_filter_state_viewing_snakes(direction_command_filter::try_add));

  // Create effect handler for messages that produce effects
  GameEngineEffectHandler effect_handler(renderable_state_pub_, alive_states_pub_, summary_resp_pub_, timer_);

  // Process timer events with effect handler pattern
  process_event_with_state(timer_, state_, handleTick, effect_handler);

  // Process clock commands with effect handler pattern
  process_message_with_state(clock_sub_, state_, handleGameClockCommand, effect_handler);

  // Process tick rate changes with effect handler pattern
  process_message_with_state(tickrate_sub_, state_, handleTickRateChange, effect_handler);

  // Process food reposition triggers (pure, no effects)
  process_message_with_state(reposition_sub_, state_, setFoodRepositionFlag);

  // Process game state summary requests with effect handler pattern
  process_message_with_state(summary_req_sub_, state_, handleSummaryRequest, effect_handler);
}

}  // namespace snake
