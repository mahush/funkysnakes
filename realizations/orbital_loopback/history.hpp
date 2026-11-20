#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "common/random_source.hpp"
#include "orbital/arena_events.hpp"
#include "orbital/forwarded_order_facts.hpp"
#include "orbital/game_authority.hpp"
#include "orbital/game_boundary.hpp"
#include "orbital/order_replica_authority.hpp"

namespace snake {
namespace orbital {
namespace loopback {

/**
 * Recorded histories of the Orbital realizations and their text form
 *
 * Platform code without meaning: a record collects what a realization reported, and the text form spells it out
 * line by line, so two histories are compared, and their difference shown, as text.
 */

/**
 * @brief Random source drawn from a seeded engine, so a realization and its replay draw the same numbers
 */
RandomIntGeneratorFn seededRandom(unsigned seed);

/**
 * @brief What the host recorded of one game: enough to replay it into the Domain Application
 */
struct HostRecord {
  RoundId round{0};
  unsigned seed{0};
  std::vector<platform::ForwardedSubmission> submissions;      // Taken in while running, in order of receipt
  std::vector<platform::ForwardedTransmission> transmissions;  // Taken in while running, in order of receipt
  std::vector<std::vector<game_authority::HandoverGranted>> transmission_handovers;  // Per transmission
  std::vector<game_boundary::StepReport> steps;
  ArenaEvents end_events;
  std::optional<game_authority::GameConcluded> conclusion;
  std::vector<platform::order_replica_authority::LateTransmission> deviations;
  std::map<StepId, game_boundary::Views> checkpoints;  // Views between step k and step k+1, by k
  std::optional<game_boundary::Views> final_views;
};

/**
 * @brief What the Domain Application reported when the record was replayed into it
 */
struct CanonicalHistory {
  std::vector<OrderSubmitted> submissions;
  std::vector<game_boundary::StepReport> steps;
  ArenaEvents end_events;
  std::optional<game_authority::GameConcluded> conclusion;
  std::map<StepId, game_boundary::Views> checkpoints;  // Views between step k and step k+1, by k
  std::optional<game_boundary::Views> final_views;
};

// Text form, one item per line
std::string describe(const Order& order);
std::string describe(const OrderSubmitted& submitted);
std::string describe(const OrderTransmitted& transmitted);
std::string describe(const ActivationOutcome& outcome);
std::string describe(const ArenaEvent& event);
std::string describe(const game_authority::HandoverGranted& handover);
std::string describe(const game_authority::GameConcluded& concluded);
std::string describe(const platform::order_replica_authority::LateTransmission& late);
std::vector<std::string> describe(const game_boundary::Views& views);

/**
 * @brief The host's record as a readable timeline
 */
std::vector<std::string> describeTimeline(const HostRecord& record);

}  // namespace loopback
}  // namespace orbital
}  // namespace snake
