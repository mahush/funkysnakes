#include "orbital/game_authority.hpp"

#include <algorithm>

namespace snake {
namespace orbital {
namespace game_authority {

namespace detail {

// The Authority's own access to the private representation of its state
struct Access {
  static auto& rep(State& state) { return state.rep_; }
  static const auto& rep(const State& state) { return state.rep_; }
};

}  // namespace detail

namespace {

auto& rep(State& state) { return detail::Access::rep(state); }
const auto& rep(const State& state) { return detail::Access::rep(state); }

// Highest credited score wins; equal highest scores draw
Result decideResult(const PerPlayerScores& credited) {
  std::optional<PlayerId> leader;
  bool tied = false;
  for (const auto& [player, score] : credited) {
    if (!leader || score > credited.at(*leader)) {
      leader = player;
      tied = false;
    } else if (score == credited.at(*leader)) {
      tied = true;
    }
  }
  return Result{tied ? std::nullopt : leader};
}

}  // namespace

State startRound(RoundId round, const std::vector<PlayerId>& players) {
  State state;
  rep(state).round = round;
  for (const PlayerId& player : players) {
    rep(state).scores.credited[player] = 0;
  }
  return state;
}

std::tuple<State, std::optional<GameConcluded>> observeStep(State state,
                                                            GameTime now,
                                                            const ArenaEvents& events,
                                                            bool any_snake_alive) {
  if (!isRunning(state)) {
    return {std::move(state), std::nullopt};
  }

  rep(state).now = now;
  rep(state).scores = scoring_policy::applyScoring(std::move(rep(state).scores), events);

  if (now < rep(state).duration && any_snake_alive) {
    return {std::move(state), std::nullopt};
  }

  rep(state).scores = scoring_policy::loseUnbanked(std::move(rep(state).scores));
  rep(state).result = decideResult(rep(state).scores.credited);
  return {state, GameConcluded{rep(state).round, *rep(state).result}};
}

bool isRunning(const State& state) { return !rep(state).result.has_value(); }

View view(const State& state) {
  View result;
  for (const auto& [player, credited] : rep(state).scores.credited) {
    result.players[player] = PlayerScore{credited, 0};
  }
  for (const auto& [order, unbanked] : rep(state).scores.unbanked) {
    result.players[unbanked.player].unbanked += unbanked.points;
  }
  result.remaining = std::max(GameTime{0}, rep(state).duration - rep(state).now);
  result.result = rep(state).result;
  return result;
}

}  // namespace game_authority
}  // namespace orbital
}  // namespace snake
