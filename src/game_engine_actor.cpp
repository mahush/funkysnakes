#include "snake/game_engine_actor.hpp"

#include <funkyactors/effect_handler.hpp>
#include <optional>
#include <tuple>

#include "classic/classic_arena_authority.hpp"
#include "common/game_logic.hpp"
#include "funkypipes/bind_front.hpp"
#include "snake/game_state_lenses.hpp"
#include "snake/logger.hpp"
#include "snake/process_helpers.hpp"
#include "snake/utility.hpp"

namespace snake {

using funkypipes::bindFront;

namespace {

// ============================================================================
// Game Utility Functions
// ============================================================================

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
  PerPlayerAliveStates current_alive_states = extractAliveStates(state.arena.snakes);
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
 * Parameter order: bound parameters first (for bindFront), then state and event.
 *
 * @param random_int Random number source for food placement
 * @param state Current game state
 * @param event Timer elapsed event (unused, required for signature)
 * @return Tuple of (new GameState, RenderableStateMsg, optional PlayerAliveStatesMsg)
 */
std::tuple<GameState, RenderableStateMsg, std::optional<PlayerAliveStatesMsg>> handleTick(
    const RandomIntGeneratorFn& random_int, GameState state, const GameTimerElapsedEvent& /* event */) {
  // Advance the arena (owned by the Classic Arena Authority)
  state.arena = classic_arena_authority::tick(random_int, std::move(state.arena));

  // Check if alive states changed and generate message if so
  auto [state_with_updated_alive, alive_msg] = tryGeneratePlayerAliveStates(state);
  state = state_with_updated_alive;

  // Project the arena to the boundary's arena view
  RenderableStateMsg renderable{game_boundary::ArenaView{
      state.arena.board,
      state.arena.snakes,
      state.arena.food_items,
      state.arena.scores,
  }};

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
      if (msg.interval_ms) {
        state.interval_ms = *msg.interval_ms;
      }
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
 * @brief Forward a food reposition trigger to the arena
 *
 * @param state Current game state
 * @param trigger Food reposition trigger (contains game_id for validation)
 * @return Updated game state with reposition requested if game_id matches
 */
GameState handleFoodRepositionTrigger(GameState state, const FoodRepositionTriggerMsg& trigger) {
  // Only set flag if trigger is for current game (ignore stale triggers)
  if (trigger.game_id == state.game_id) {
    state.arena = classic_arena_authority::requestFoodReposition(std::move(state.arena));
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
  response.scores = state.arena.scores;
  response.alive_states = extractAliveStates(state.arena.snakes);
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
      game_loop_timer_{create_timer<GameTimer>(timer_factory)},
      random_int_{makeRandomIntGenerator()} {
  game_state_.game_id = "game_001";
  game_state_.arena = classic_arena_authority::initial(random_int_, Board{60, 20});

  Logger::log("[GameEngineActor] Initialized " + std::to_string(game_state_.arena.food_items.size()) + " food items\n");
}

void GameEngineActor::processInputs() {
  // Drain steering commands into the arena
  processMessageWithState(direction_sub_, game_state_, [](GameState state, const DirectionMsg& msg) {
    return over_arena(classic_arena_authority::steer)(std::move(state), msg.steer);
  });

  // Create effect handler for messages that produce effects
  GameEngineEffectHandler effect_handler(renderable_state_pub_, alive_states_pub_, summary_resp_pub_, game_loop_timer_);

  // Process timer events with effect handler pattern
  processEventWithState(game_loop_timer_, game_state_, bindFront(handleTick, random_int_), effect_handler);

  // Process clock commands with effect handler pattern
  processMessageWithState(clock_sub_, game_state_, handleGameClockCommand, effect_handler);

  // Process tick rate changes with effect handler pattern
  processMessageWithState(tickrate_sub_, game_state_, handleTickRateChange, effect_handler);

  // Process food reposition triggers (pure, no effects)
  processMessageWithState(reposition_sub_, game_state_, handleFoodRepositionTrigger);

  // Process game state summary requests with effect handler pattern
  processMessageWithState(summary_req_sub_, game_state_, handleSummaryRequest, effect_handler);
}

}  // namespace snake
