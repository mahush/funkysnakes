// Tests for the loopback realization of Orbital Orders: the host and phone compositions over an in-process
// transport, checked by record → replay → compare against the Domain Application.

#include <gtest/gtest.h>

#include <chrono>
#include <map>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "orbital_loopback/loopback.hpp"
#include "orbital_loopback/replay.hpp"

namespace snake {
namespace orbital {
namespace loopback {

namespace {

using std::chrono::milliseconds;

std::string joined(const std::vector<std::string>& lines) {
  std::string text;
  for (const std::string& line : lines) {
    text += line + "\n";
  }
  return text;
}

class LoopbackGames : public ::testing::TestWithParam<unsigned> {};

}  // namespace

/// @section playOverLoopback, replayInDomainApplication, compare

/// @brief Whole games over loopback replay to identical histories, views included at every step checkpoint
TEST_P(LoopbackGames, ReplayToIdenticalHistories) {
  // Given: a whole game of scripted submissions, played over loopback without delay
  HostRecord record =
      playOverLoopback(LoopbackGame{RoundId{1}, GetParam(), randomScript(GetParam(), milliseconds{3000}), {}});

  // When: the record is replayed into the Domain Application and compared
  CanonicalHistory canonical = replayInDomainApplication(record);
  Comparison comparison = compare(record, canonical);

  // Then: the histories agree, and the game was a real one
  EXPECT_TRUE(comparison.agrees()) << joined(comparison.divergences);
  EXPECT_TRUE(record.conclusion.has_value());
  EXPECT_FALSE(record.transmissions.empty());
  EXPECT_EQ(record.checkpoints.size(), canonical.checkpoints.size());
  EXPECT_GT(record.checkpoints.size(), 10u);
}

INSTANTIATE_TEST_SUITE_P(Seeds, LoopbackGames, ::testing::Values(1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u));

/// @brief Known Issues #2, #3 and #6 need a resolved order handed to the Arena again; over loopback none is
TEST(OrbitalLoopback, NeverHandsResolvedOrderToArenaAgain) {
  // Given: a whole game with frequent submissions
  HostRecord record = playOverLoopback(LoopbackGame{RoundId{1}, 11u, randomScript(11u, milliseconds{800}), {}});

  // When: the activation outcomes of all steps are collected
  std::map<std::pair<PlayerId, int>, int> outcomes;
  for (const game_boundary::StepReport& step : record.steps) {
    for (const ActivationOutcome& outcome : step.activations) {
      std::visit([&outcomes](const auto& item) { ++outcomes[{item.player, item.order.value}]; }, outcome);
    }
  }

  // Then: every order got at most one outcome
  ASSERT_FALSE(outcomes.empty());
  for (const auto& [order, count] : outcomes) {
    EXPECT_EQ(count, 1) << order.first << " #" << order.second;
  }
}

TEST(OrbitalLoopback, LogsDelayedTransmissionAsDeviationAndReportsDivergence) {
  // Given: player A's first transmission takes a second to reach the host
  bool delayed = false;
  DelayFn delay = [&delayed](const LinkMessage& message) {
    if (message.direction == LinkMessage::Direction::UPLINK && message.player == PLAYER_A &&
        std::holds_alternative<platform::ForwardedTransmission>(message.message) && !delayed) {
      delayed = true;
      return milliseconds{1000};
    }
    return milliseconds{0};
  };

  // When: a game is played and replayed
  HostRecord record = playOverLoopback(LoopbackGame{RoundId{1}, 3u, randomScript(3u, milliseconds{3000}), delay});
  Comparison comparison = compare(record, replayInDomainApplication(record));

  // Then: the host logged the late transmission, and the comparison reports it as a divergence
  ASSERT_EQ(record.deviations.size(), 1u);
  EXPECT_EQ(record.deviations[0].player, PLAYER_A);
  EXPECT_GT(record.deviations[0].first_step, record.deviations[0].scheduled_step);
  EXPECT_FALSE(comparison.agrees());
  bool reported = false;
  for (const std::string& divergence : comparison.divergences) {
    reported = reported || divergence.rfind("deviation: ", 0) == 0;
  }
  EXPECT_TRUE(reported) << joined(comparison.divergences);
}

}  // namespace loopback
}  // namespace orbital
}  // namespace snake
