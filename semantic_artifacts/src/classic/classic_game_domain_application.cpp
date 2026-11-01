#include "classic/classic_game_domain_application.hpp"

#include <algorithm>

#include "classic/difficulty_policy.hpp"
#include "common/game_logic.hpp"

namespace snake {
namespace classic_game_domain_application {

namespace lifecycle = classic_game_lifecycle_authority;
using std::chrono::milliseconds;

namespace {

// ============================================================================
// Carrying out lifecycle intents
// ============================================================================
// The lifecycle Authority announces clock and cadence intents together, so one running flag
// covers both; the cadence intents decide only whether the period countdowns restart.

VirtualClock executeClockIntent(VirtualClock clock, lifecycle::ClockIntent intent) {
  switch (intent) {
    case lifecycle::ClockIntent::START:
    case lifecycle::ClockIntent::RESUME:
      clock.running = true;
      clock.until_step = clock.step_interval;
      break;
    case lifecycle::ClockIntent::STOP:
    case lifecycle::ClockIntent::PAUSE:
      clock.running = false;
      break;
  }
  return clock;
}

VirtualClock executeStepIntervalIntent(VirtualClock clock, lifecycle::StepIntervalIntent intent) {
  clock.step_interval = milliseconds{intent.interval_ms};
  clock.until_step = clock.step_interval;
  return clock;
}

VirtualClock executeCadenceIntent(VirtualClock clock, lifecycle::CadenceIntent intent) {
  switch (intent) {
    case lifecycle::CadenceIntent::START:
      clock.until_level_period = lifecycle::LEVEL_PERIOD;
      clock.until_reposition_period = lifecycle::REPOSITION_PERIOD;
      break;
    case lifecycle::CadenceIntent::RESUME:  // Continue the remaining periods
    case lifecycle::CadenceIntent::STOP:
    case lifecycle::CadenceIntent::FREEZE:  // Keep the remaining periods
      break;
  }
  return clock;
}

// ============================================================================
// Projections to the boundary
// ============================================================================

ArenaView viewArena(const classic_arena_authority::State& arena) {
  return ArenaView{arena.board, arena.snakes, arena.food_items, arena.scores};
}

Status viewStatus(const lifecycle::State& state) { return Status{state.level, state.paused}; }

// ============================================================================
// Due work
// ============================================================================

std::tuple<State, Observations> runStep(const RandomIntGeneratorFn& random_int_fn,
                                        State state,
                                        Observations observations) {
  state.arena = classic_arena_authority::tick(random_int_fn, std::move(state.arena));
  observations.arena_views.push_back(viewArena(state.arena));

  // Conclusion is decided by the lifecycle Authority from the arena's alive states
  auto [lifecycle_state, conclude] =
      lifecycle::observeAliveStates(state.lifecycle, extractAliveStates(state.arena.snakes));
  state.lifecycle = lifecycle_state;

  if (conclude) {
    observations.game_over = GameOver{state.arena.scores, state.lifecycle.level};
    state.clock = executeCadenceIntent(executeClockIntent(state.clock, conclude->clock), conclude->cadence);
  }

  state.clock.until_step = state.clock.step_interval;
  return {std::move(state), std::move(observations)};
}

std::tuple<State, Observations> runLevelPeriod(State state, Observations observations) {
  auto [lifecycle_state, interval] = lifecycle::levelPeriodElapsed(state.lifecycle);
  state.lifecycle = lifecycle_state;
  if (interval) {
    state.clock = executeStepIntervalIntent(state.clock, *interval);
    observations.statuses.push_back(viewStatus(state.lifecycle));
  }
  state.clock.until_level_period = lifecycle::LEVEL_PERIOD;
  return {std::move(state), std::move(observations)};
}

State runRepositionPeriod(State state) {
  if (lifecycle::repositionPeriodElapsed(state.lifecycle)) {
    state.arena = classic_arena_authority::requestFoodReposition(std::move(state.arena));
  }
  state.clock.until_reposition_period = lifecycle::REPOSITION_PERIOD;
  return state;
}

// Time until the next due work, or nullopt if nothing is running
std::optional<milliseconds> untilNextDue(const State& state) {
  if (!state.clock.running) {
    return std::nullopt;
  }
  return std::min({state.clock.until_step, state.clock.until_level_period, state.clock.until_reposition_period});
}

State advanceClock(State state, milliseconds duration) {
  if (state.clock.running) {
    state.clock.until_step -= duration;
    state.clock.until_level_period -= duration;
    state.clock.until_reposition_period -= duration;
  }
  return state;
}

}  // namespace

State initial(const RandomIntGeneratorFn& random_int_fn) {
  State state{lifecycle::State{},
              classic_arena_authority::initial(random_int_fn, classic_arena_authority::makeClassicSetup()),
              VirtualClock{}};
  state.clock.step_interval = milliseconds{difficulty_policy::stepIntervalMs(1)};
  return state;
}

std::tuple<State, Observations> apply(State state, const Start& start) {
  auto [lifecycle_state, clock_intent, interval, cadence_intent] =
      lifecycle::start(start.starting_level, state.lifecycle);
  state.lifecycle = lifecycle_state;
  state.clock = executeStepIntervalIntent(state.clock, interval);
  state.clock = executeClockIntent(state.clock, clock_intent);
  state.clock = executeCadenceIntent(state.clock, cadence_intent);

  Observations observations;
  observations.statuses.push_back(viewStatus(state.lifecycle));
  return {std::move(state), std::move(observations)};
}

std::tuple<State, Observations> apply(State state, const Steer& steer) {
  state.arena = classic_arena_authority::steer(std::move(state.arena), steer);
  return {std::move(state), Observations{}};
}

std::tuple<State, Observations> apply(State state, const TogglePause& /* toggle */) {
  auto [lifecycle_state, clock_intent, cadence_intent] = lifecycle::togglePause(state.lifecycle);
  state.lifecycle = lifecycle_state;
  if (clock_intent) {
    state.clock = executeClockIntent(state.clock, *clock_intent);
  }
  if (cadence_intent) {
    state.clock = executeCadenceIntent(state.clock, *cadence_intent);
  }

  Observations observations;
  observations.statuses.push_back(viewStatus(state.lifecycle));
  return {std::move(state), std::move(observations)};
}

std::tuple<State, Observations> apply(const RandomIntGeneratorFn& random_int_fn,
                                      State state,
                                      const TimeElapsed& elapsed) {
  Observations observations;
  milliseconds remaining = elapsed.duration;

  while (true) {
    std::optional<milliseconds> next = untilNextDue(state);
    if (!next || *next > remaining) {
      state = advanceClock(std::move(state), remaining);
      break;
    }
    state = advanceClock(std::move(state), *next);
    remaining -= *next;

    // Work due at the same instant runs in the order the lifecycle Authority defines
    for (lifecycle::DueWork work : lifecycle::SAME_INSTANT_ORDER) {
      switch (work) {
        case lifecycle::DueWork::STEP:
          if (state.clock.running && state.clock.until_step <= milliseconds{0}) {
            std::tie(state, observations) = runStep(random_int_fn, std::move(state), std::move(observations));
          }
          break;
        case lifecycle::DueWork::LEVEL_PERIOD:
          if (state.clock.running && state.clock.until_level_period <= milliseconds{0}) {
            std::tie(state, observations) = runLevelPeriod(std::move(state), std::move(observations));
          }
          break;
        case lifecycle::DueWork::REPOSITION_PERIOD:
          if (state.clock.running && state.clock.until_reposition_period <= milliseconds{0}) {
            state = runRepositionPeriod(std::move(state));
          }
          break;
      }
    }
  }

  return {std::move(state), std::move(observations)};
}

}  // namespace classic_game_domain_application
}  // namespace snake
