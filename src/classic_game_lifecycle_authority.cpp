#include "snake/classic_game_lifecycle_authority.hpp"

#include <algorithm>

#include "snake/difficulty_policy.hpp"

namespace snake {
namespace classic_game_lifecycle_authority {

std::tuple<State, ClockIntent, CadenceIntent> start(GameId game_id, int starting_level, State state) {
  state.game_id = std::move(game_id);
  state.level = starting_level;
  state.over = false;
  state.paused = false;
  return {std::move(state), GameClockState::START, CadenceIntent::START};
}

std::tuple<State, ClockIntent> togglePause(State state) {
  state.paused = !state.paused;
  ClockIntent clock = state.paused ? GameClockState::PAUSE : GameClockState::RESUME;
  return {std::move(state), clock};
}

std::tuple<State, std::optional<StepIntervalIntent>> levelPeriodElapsed(State state) {
  if (state.paused) {
    return {std::move(state), std::nullopt};
  }
  state.level++;
  StepIntervalIntent interval{difficulty_policy::stepIntervalMs(state.level)};
  return {std::move(state), interval};
}

std::optional<RepositionIntent> repositionPeriodElapsed(const State& state) {
  if (state.paused) {
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
  return {GameClockState::STOP, CadenceIntent::STOP};
}

}  // namespace classic_game_lifecycle_authority
}  // namespace snake
