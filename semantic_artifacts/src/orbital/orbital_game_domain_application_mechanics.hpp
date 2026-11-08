#pragma once

#include <iterator>
#include <optional>
#include <utility>
#include <vector>

#include "orbital/arena_events.hpp"
#include "orbital/game_authority.hpp"
#include "orbital/game_boundary.hpp"
#include "orbital/order.hpp"

namespace snake {
namespace orbital {
namespace orbital_game_domain_application {
namespace mechanics {

/**
 * @brief Everything one interaction or execution input caused, in order of occurrence
 */
struct Observations {
  std::optional<game_boundary::StartRejected> start_rejected;  // For a refused Start
  std::optional<game_boundary::SubmissionOutcome> submission;  // For Submit
  std::optional<OrderCommitted> commitment;      // For an accepted Submit: the Order Authority's commitment record
  std::vector<game_boundary::StepReport> steps;  // Every step that ran, oldest first
  ArenaEvents end_of_round_events;               // Orders ended because the game concluded
  std::optional<game_authority::GameConcluded> concluded;
  std::optional<game_boundary::Views> views;  // Views after the interaction, once started
};

/**
 * @brief Observations produced so far, followed by a later batch
 *
 * @param observations Earlier observations
 * @param later Observations of later steps and their conclusion
 * @return Both, in order of occurrence
 */
inline Observations appendObservations(Observations observations, Observations later) {
  observations.steps.insert(observations.steps.end(),
                            std::make_move_iterator(later.steps.begin()),
                            std::make_move_iterator(later.steps.end()));
  observations.end_of_round_events.insert(observations.end_of_round_events.end(),
                                          std::make_move_iterator(later.end_of_round_events.begin()),
                                          std::make_move_iterator(later.end_of_round_events.end()));
  if (later.concluded) {
    observations.concluded = later.concluded;
  }
  return observations;
}

/**
 * @brief Hand the observations to the players' outbound interface, in order of occurrence
 *
 * @param observations What one interaction or execution input caused
 * @param players The players' side
 */
inline void deliver(const Observations& observations, game_boundary::PlayersEndOutbound& players) {
  if (observations.start_rejected) {
    players.startRejected(*observations.start_rejected);
  }
  if (observations.submission) {
    players.submissionOutcome(*observations.submission);
  }
  if (observations.commitment) {
    players.orderCommitted(*observations.commitment);
  }
  for (const game_boundary::StepReport& step : observations.steps) {
    players.stepReported(step);
  }
  if (!observations.end_of_round_events.empty()) {
    players.ordersEndedWithRound(observations.end_of_round_events);
  }
  if (observations.concluded) {
    players.concluded(*observations.concluded);
  }
  if (observations.views) {
    players.viewsUpdated(*observations.views);
  }
}

}  // namespace mechanics
}  // namespace orbital_game_domain_application
}  // namespace orbital
}  // namespace snake
