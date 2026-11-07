// Tests for the Admission Policy: which players' interactions a game admits in each of its phases.

#include <gtest/gtest.h>

#include "orbital/admission_policy.hpp"

namespace snake {
namespace orbital {

using admission_policy::GamePhase;
using admission_policy::StartRejectionReason;
using admission_policy::SubmissionRejectionReason;

/// @section admission_policy::startRejectionIn

TEST(OrbitalAdmissionPolicy, AdmitsStartBeforeAGameStarted) {
  // When: a start is asked for before a game has started
  std::optional<StartRejectionReason> rejection = admission_policy::startRejectionIn(GamePhase::NOT_STARTED);

  // Then: it is admitted
  EXPECT_FALSE(rejection.has_value());
}

class OrbitalAdmissionPolicyStarted : public ::testing::TestWithParam<GamePhase> {};

/// @brief One Domain Application plays one game; a new game needs a new one
TEST_P(OrbitalAdmissionPolicyStarted, RejectsStartOnceAGameStarted) {
  // When: a start is asked for once a game has started
  std::optional<StartRejectionReason> rejection = admission_policy::startRejectionIn(GetParam());

  // Then: it is refused because a game already started
  EXPECT_EQ(rejection, StartRejectionReason::ALREADY_STARTED);
}

INSTANTIATE_TEST_SUITE_P(StartedPhases,
                         OrbitalAdmissionPolicyStarted,
                         ::testing::Values(GamePhase::RUNNING, GamePhase::CONCLUDED));

/// @section admission_policy::submissionRejectionIn

TEST(OrbitalAdmissionPolicy, AdmitsSubmissionWhileGameIsRunning) {
  // When: a submission is asked for while the game is running
  std::optional<SubmissionRejectionReason> rejection = admission_policy::submissionRejectionIn(GamePhase::RUNNING);

  // Then: it is admitted
  EXPECT_FALSE(rejection.has_value());
}

TEST(OrbitalAdmissionPolicy, RejectsSubmissionBeforeStartAsNotStarted) {
  // When: a submission is asked for before a game has started
  std::optional<SubmissionRejectionReason> rejection = admission_policy::submissionRejectionIn(GamePhase::NOT_STARTED);

  // Then: it is refused because no game has started
  EXPECT_EQ(rejection, SubmissionRejectionReason::NOT_STARTED);
}

TEST(OrbitalAdmissionPolicy, RejectsSubmissionAfterConclusionAsRoundOver) {
  // When: a submission is asked for after the game concluded
  std::optional<SubmissionRejectionReason> rejection = admission_policy::submissionRejectionIn(GamePhase::CONCLUDED);

  // Then: it is refused because the round is over
  EXPECT_EQ(rejection, SubmissionRejectionReason::ROUND_OVER);
}

}  // namespace orbital
}  // namespace snake
