#include <gtest/gtest.h>

#include <chrono>
#include <optional>
#include <variant>
#include <vector>

#include "domain/orbital/pacing_policy.hpp"
#include "orbital_loopback/phone_composition.hpp"

namespace snake {
namespace orbital {
namespace loopback {

using game_boundary::SubmissionAccepted;
using game_boundary::SubmissionOutcome;
using game_boundary::SubmissionRejected;
using game_boundary::SubmissionRejectionReason;
using placement_link::GameStarted;
using placement_link::PhoneToHost;
using std::chrono::milliseconds;

namespace {

constexpr RoundId ROUND{1};
constexpr milliseconds T0{1000};  // Host game clock when the game started

using Forwarded = std::variant<OrderSubmitted, OrderTransmitted>;

// The host's side of the placement link: keeps what the device forwards, in order
class RecordingHost : public PhoneToHost {
 public:
  void forward(const OrderSubmitted& submitted) override { forwarded.emplace_back(submitted); }
  void forward(const OrderTransmitted& transmitted) override { forwarded.emplace_back(transmitted); }

  std::vector<Forwarded> forwarded;
};

// Host game clock at the time a step falls due, plus an offset into the step
milliseconds atStep(int step, milliseconds offset = milliseconds{0}) {
  return T0 + pacing_policy::timeOfStep(StepId{step}) + offset;
}

class PhoneCompositionTest : public ::testing::Test {
 protected:
  PhoneCompositionTest() : phone_{PLAYER_A, host_} {}

  // The host started the game
  void startGame() { phone_.gameStarted(GameStarted{ROUND, T0}); }

  RecordingHost host_;
  PhoneComposition phone_;
};

}  // namespace

/// @section PhoneComposition::submit

TEST_F(PhoneCompositionTest, RejectsSubmissionBeforeGameStarted) {
  // When: the player submits before the host started the game
  SubmissionOutcome outcome = phone_.submit(Point{3, 4}, atStep(0));

  // Then: it is rejected, and nothing is forwarded
  EXPECT_EQ(std::get<SubmissionRejected>(outcome).reason, SubmissionRejectionReason::NOT_STARTED);
  EXPECT_TRUE(host_.forwarded.empty());
}

/// @brief The time a submission happened decides its step, not when the host learns of it
TEST_F(PhoneCompositionTest, SelectsSubmissionInStepOfItsTime) {
  // Given: the game started
  startGame();

  // When: the player submits halfway through step 22
  SubmissionOutcome outcome = phone_.submit(Point{3, 4}, atStep(22, milliseconds{100}));

  // Then: it is accepted in step 22, and its submission record is forwarded
  const SubmissionAccepted& accepted = std::get<SubmissionAccepted>(outcome);
  EXPECT_EQ(accepted.order, OrderNumber{1});
  EXPECT_EQ(accepted.submitted_at, StepId{22});
  EXPECT_EQ(
      host_.forwarded,
      (std::vector<Forwarded>{OrderSubmitted{ROUND, OrderNumber{1}, PLAYER_A, Point{3, 4}, StepId{22}, std::nullopt}}));
}

/// @brief A device clock running behind the host's may read a time before the game started
TEST_F(PhoneCompositionTest, SelectsSubmissionBeforeGameStartTimeInStepZero) {
  // Given: the game started
  startGame();

  // When: the player submits at a host time before the game started
  SubmissionOutcome outcome = phone_.submit(Point{3, 4}, T0 - milliseconds{50});

  // Then: it is accepted in step 0
  EXPECT_EQ(std::get<SubmissionAccepted>(outcome).submitted_at, StepId{0});
}

/// @brief The timer may fire late; an opportunity whose time came before the submission still takes place first
TEST_F(PhoneCompositionTest, TransmitsAtDueOpportunityBeforeLaterSubmission) {
  // Given: the game started, and the player submitted in step 0
  startGame();
  phone_.submit(Point{3, 4}, atStep(0, milliseconds{100}));

  // When: the player submits again just after the opportunity of step 1, before its timer fired
  phone_.submit(Point{8, 1}, atStep(1, milliseconds{1}));

  // Then: the first order is transmitted at step 1 before the second one is selected, which replaces nothing
  EXPECT_EQ(host_.forwarded,
            (std::vector<Forwarded>{
                OrderSubmitted{ROUND, OrderNumber{1}, PLAYER_A, Point{3, 4}, StepId{0}, std::nullopt},
                OrderTransmitted{ROUND, OrderNumber{1}, PLAYER_A, Point{3, 4}, StepId{0}, StepId{1}, std::nullopt},
                OrderSubmitted{ROUND, OrderNumber{2}, PLAYER_A, Point{8, 1}, StepId{1}, std::nullopt}}));
}

TEST_F(PhoneCompositionTest, RejectsSubmissionAfterConclusion) {
  // Given: the game started and concluded
  startGame();
  phone_.concluded(game_authority::GameConcluded{ROUND, game_authority::Result{std::nullopt}});

  // When: the player submits
  SubmissionOutcome outcome = phone_.submit(Point{3, 4}, atStep(5));

  // Then: it is rejected, and nothing is forwarded
  EXPECT_EQ(std::get<SubmissionRejected>(outcome).reason, SubmissionRejectionReason::CONCLUDED);
  EXPECT_TRUE(host_.forwarded.empty());
}

/// @section PhoneComposition::transmitAtDueOpportunities

TEST_F(PhoneCompositionTest, TransmitsSelectionAtItsOpportunity) {
  // Given: the game started, and the player submitted in step 0
  startGame();
  phone_.submit(Point{3, 4}, atStep(0, milliseconds{100}));

  // When: the time of the opportunity of step 1 comes
  phone_.transmitAtDueOpportunities(atStep(1));

  // Then: the order is transmitted at step 1
  EXPECT_EQ(
      host_.forwarded.back(),
      (Forwarded{OrderTransmitted{ROUND, OrderNumber{1}, PLAYER_A, Point{3, 4}, StepId{0}, StepId{1}, std::nullopt}}));
}

TEST_F(PhoneCompositionTest, TransmitsNothingBeforeOpportunity) {
  // Given: the game started, and the player submitted in step 0
  startGame();
  phone_.submit(Point{3, 4}, atStep(0, milliseconds{100}));

  // When: the time just before the opportunity of step 1 comes
  phone_.transmitAtDueOpportunities(atStep(1, milliseconds{-1}));

  // Then: only the submission was forwarded
  EXPECT_EQ(host_.forwarded.size(), 1u);
}

/// @brief A submission in an opportunity's step comes after that opportunity
TEST_F(PhoneCompositionTest, KeepsSubmissionAtOpportunityTimeForLaterOpportunity) {
  // Given: the game started, and the player submitted exactly at the time of the opportunity of step 1
  startGame();
  phone_.submit(Point{3, 4}, atStep(1));

  // When: the timer of that opportunity fires
  phone_.transmitAtDueOpportunities(atStep(1));

  // Then: nothing is transmitted, the selection waits for a later opportunity
  EXPECT_EQ(host_.forwarded.size(), 1u);
}

/// @brief A held-up device must not give its order a later transmission step than the opportunity that sent it
TEST_F(PhoneCompositionTest, TransmitsAtEachMissedOpportunityWithItsOwnStep) {
  // Given: the game started, and the player submitted in step 0
  startGame();
  phone_.submit(Point{3, 4}, atStep(0, milliseconds{100}));

  // When: the device is held up until after the opportunity of step 21
  phone_.transmitAtDueOpportunities(atStep(30));

  // Then: the order is transmitted once, at the opportunity of step 1
  EXPECT_EQ(host_.forwarded,
            (std::vector<Forwarded>{
                OrderSubmitted{ROUND, OrderNumber{1}, PLAYER_A, Point{3, 4}, StepId{0}, std::nullopt},
                OrderTransmitted{ROUND, OrderNumber{1}, PLAYER_A, Point{3, 4}, StepId{0}, StepId{1}, std::nullopt}}));
}

/// @brief Catching up after a stall still lets a selection wait for the opportunity of its submission
TEST_F(PhoneCompositionTest, TransmitsHeldUpSelectionAtOpportunityOfItsSubmission) {
  // Given: the game started, and the player submitted exactly at the time of the opportunity of step 1
  startGame();
  phone_.submit(Point{3, 4}, atStep(1));

  // When: the device is held up until after the opportunity of step 21
  phone_.transmitAtDueOpportunities(atStep(30));

  // Then: the order is transmitted once, at the opportunity of step 21
  EXPECT_EQ(host_.forwarded,
            (std::vector<Forwarded>{
                OrderSubmitted{ROUND, OrderNumber{1}, PLAYER_A, Point{3, 4}, StepId{1}, std::nullopt},
                OrderTransmitted{ROUND, OrderNumber{1}, PLAYER_A, Point{3, 4}, StepId{1}, StepId{21}, std::nullopt}}));
}

TEST_F(PhoneCompositionTest, TakesPlaceAtEachOpportunityOnce) {
  // Given: the game started, the opportunity of step 1 took place, and the player then submitted in step 1
  startGame();
  phone_.transmitAtDueOpportunities(atStep(1));
  phone_.submit(Point{3, 4}, atStep(1, milliseconds{50}));

  // When: the timer of that opportunity fires again
  phone_.transmitAtDueOpportunities(atStep(1, milliseconds{100}));

  // Then: nothing is transmitted, the selection waits for the next opportunity
  EXPECT_EQ(host_.forwarded.size(), 1u);
}

TEST_F(PhoneCompositionTest, TransmitsNothingAfterConclusion) {
  // Given: the game started, the player submitted in step 0, and the game concluded
  startGame();
  phone_.submit(Point{3, 4}, atStep(0, milliseconds{100}));
  phone_.concluded(game_authority::GameConcluded{ROUND, game_authority::Result{std::nullopt}});

  // When: the time of the opportunity of step 1 comes
  phone_.transmitAtDueOpportunities(atStep(1));

  // Then: only the submission was forwarded
  EXPECT_EQ(host_.forwarded.size(), 1u);
}

/// @section PhoneComposition::gameStarted

TEST_F(PhoneCompositionTest, PlaysOnlyFirstGameStarted) {
  // Given: the game started
  startGame();

  // When: the host reports another game start, and the player submits
  phone_.gameStarted(GameStarted{RoundId{2}, T0 + milliseconds{5000}});
  phone_.submit(Point{3, 4}, atStep(0, milliseconds{100}));

  // Then: the submission belongs to the first round and its timeline
  EXPECT_EQ(
      host_.forwarded,
      (std::vector<Forwarded>{OrderSubmitted{ROUND, OrderNumber{1}, PLAYER_A, Point{3, 4}, StepId{0}, std::nullopt}}));
}

}  // namespace loopback
}  // namespace orbital
}  // namespace snake
