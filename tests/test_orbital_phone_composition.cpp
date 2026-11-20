// Tests for the phone composition of the Orbital loopback realization: one player's device with its Order Authority.

#include <gtest/gtest.h>

#include <chrono>
#include <variant>
#include <vector>

#include "orbital_loopback/phone_composition.hpp"

namespace snake {
namespace orbital {
namespace loopback {

namespace {

using std::chrono::milliseconds;

// A device's uplink that keeps what it forwards
struct ForwardedFacts : PhoneUplink {
  void forward(const platform::ForwardedSubmission& submission) override { submissions.push_back(submission); }
  void forward(const platform::ForwardedTransmission& transmission) override { transmissions.push_back(transmission); }

  std::vector<platform::ForwardedSubmission> submissions;
  std::vector<platform::ForwardedTransmission> transmissions;
};

RoundStarted roundStart() { return RoundStarted{RoundId{1}, milliseconds{1000}, milliseconds{200}, {StepId{1}}}; }

}  // namespace

/// @section PhoneComposition

TEST(OrbitalPhoneComposition, RejectsSubmissionBeforeRoundStarted) {
  // Given: a device that has not heard of a round
  ForwardedFacts uplink;
  PhoneComposition phone{PLAYER_A, uplink};

  // When: the player submits
  game_boundary::SubmissionOutcome outcome = phone.submit(Point{3, 4}, milliseconds{0});

  // Then: the Admission Policy's rejection is the outcome and nothing is forwarded
  ASSERT_TRUE(std::holds_alternative<game_boundary::SubmissionRejected>(outcome));
  EXPECT_EQ(std::get<game_boundary::SubmissionRejected>(outcome).reason,
            game_boundary::SubmissionRejectionReason::NOT_STARTED);
  EXPECT_TRUE(uplink.submissions.empty());
}

TEST(OrbitalPhoneComposition, ClassifiesSubmissionIntoStepThatRanLast) {
  // Given: a round that started at host time 1000 ms with 200 ms steps
  ForwardedFacts uplink;
  PhoneComposition phone{PLAYER_A, uplink};
  phone.roundStarted(roundStart());

  // When: the player submits on the boundary of step 2 and just before step 3
  phone.submit(Point{3, 4}, milliseconds{1400});
  phone.submit(Point{5, 6}, milliseconds{1599});

  // Then: both belong to step 2, and the second replaces the first
  ASSERT_EQ(uplink.submissions.size(), 2u);
  EXPECT_EQ(uplink.submissions[0].submitted.submitted_at, StepId{2});
  EXPECT_EQ(uplink.submissions[1].submitted.submitted_at, StepId{2});
  EXPECT_EQ(uplink.submissions[1].submitted.replaced, OrderNumber{1});
  EXPECT_EQ(uplink.submissions[1].order, (Order{RoundId{1}, OrderNumber{2}, PLAYER_A, Point{5, 6}}));
}

TEST(OrbitalPhoneComposition, ForwardsTransmissionAtOpportunity) {
  // Given: a selected order submitted in step 0
  ForwardedFacts uplink;
  PhoneComposition phone{PLAYER_A, uplink};
  phone.roundStarted(roundStart());
  phone.submit(Point{3, 4}, milliseconds{1100});

  // When: the device's timer fires the opportunity of step 1
  phone.fireOpportunity(StepId{1});

  // Then: the Order Authority's transmission is forwarded with its order
  ASSERT_EQ(uplink.transmissions.size(), 1u);
  EXPECT_EQ(uplink.transmissions[0].order, (Order{RoundId{1}, OrderNumber{1}, PLAYER_A, Point{3, 4}}));
  EXPECT_EQ(uplink.transmissions[0].transmitted.transmitted_at, StepId{1});
}

TEST(OrbitalPhoneComposition, TransmitsNothingAfterConclusion) {
  // Given: a selected order, then the host reported the conclusion
  ForwardedFacts uplink;
  PhoneComposition phone{PLAYER_A, uplink};
  phone.roundStarted(roundStart());
  phone.submit(Point{3, 4}, milliseconds{1100});
  phone.concluded(game_authority::GameConcluded{RoundId{1}, {}});

  // When: the timer fires an opportunity
  phone.fireOpportunity(StepId{1});

  // Then: nothing is transmitted and the order is cancelled
  EXPECT_TRUE(uplink.transmissions.empty());
  EXPECT_EQ(phone.orders()->orders.at(0).status, order_authority::Status::CANCELLED);
}

}  // namespace loopback
}  // namespace orbital
}  // namespace snake
