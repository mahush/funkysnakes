#include "classic/classic_game_lifecycle_authority.hpp"

#include <algorithm>

#include "classic/difficulty_policy.hpp"

namespace snake {
namespace classic_game_lifecycle_authority {

namespace {

// Cadence periods only count while the game runs: not while paused, and not once it is over
bool cadencesCount(const State& state) { return !state.paused && !state.over; }

}  // namespace

std::tuple<State, ClockIntent, StepIntervalIntent, CadenceIntent> start(GameId game_id,
                                                                        int starting_level,
                                                                        State state) {
  state.game_id = std::move(game_id);
  state.level = starting_level;
  state.over = false;
  state.paused = false;
  StepIntervalIntent interval{difficulty_policy::stepIntervalMs(state.level)};
  return {std::move(state), ClockIntent::START, interval, CadenceIntent::START};
}

std::tuple<State, ClockIntent, std::optional<CadenceIntent>> togglePause(State state) {
  state.paused = !state.paused;
  ClockIntent clock = state.paused ? ClockIntent::PAUSE : ClockIntent::RESUME;
  if (state.over) {
    return {std::move(state), clock, std::nullopt};
  }
  CadenceIntent cadence = state.paused ? CadenceIntent::FREEZE : CadenceIntent::RESUME;
  return {std::move(state), clock, cadence};
}

std::tuple<State, std::optional<StepIntervalIntent>> levelPeriodElapsed(State state) {
  if (!cadencesCount(state)) {
    return {std::move(state), std::nullopt};
  }
  state.level++;
  StepIntervalIntent interval{difficulty_policy::stepIntervalMs(state.level)};
  return {std::move(state), interval};
}

std::optional<RepositionIntent> repositionPeriodElapsed(const State& state) {
  if (!cadencesCount(state)) {
    return std::nullopt;
  }
  return RepositionIntent{};
}

std::tuple<State, std::optional<ConcludeIntent>> observeAliveStates(State state,
                                                                    const PerPlayerAliveStates& alive_states) {
  if (state.over) {
    return {std::move(state), std::nullopt};
  }

  // Conclusion: the game is over when no snake is alive
  bool any_alive =
      std::any_of(alive_states.begin(), alive_states.end(), [](const auto& entry) { return entry.second; });
  if (any_alive) {
    return {std::move(state), std::nullopt};
  }

  state.over = true;
  return {std::move(state), ConcludeIntent{}};
}

std::tuple<ClockIntent, CadenceIntent> concluded(const State& /* state */) {
  return {ClockIntent::STOP, CadenceIntent::STOP};
}

}  // namespace classic_game_lifecycle_authority
}  // namespace snake
