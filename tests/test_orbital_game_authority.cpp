// Tests for the Orbital Game Authority: scoring of steps, conclusion and result.

#include <gtest/gtest.h>

#include <set>
#include <tuple>
#include <vector>

#include "domain/orbital/game_authority.hpp"
#include "domain/orbital/pacing_policy.hpp"

namespace snake {
namespace orbital {

namespace {

using game_authority::Result;
using game_authority::State;

constexpr RoundId ROUND{1};
const std::set<PlayerId> BOTH_ALIVE{PLAYER_A, PLAYER_B};

State runningGame() { return game_authority::startRound(ROUND, {PLAYER_A, PLAYER_B}); }

// Transmission of an order of player A that replaced no earlier selection
OrderTransmitted transmittedByA(int order, int submitted_in, int transmitted_in) {
  return OrderTransmitted{
      ROUND, OrderNumber{order}, PLAYER_A, Point{0, 0}, StepId{submitted_in}, StepId{transmitted_in}, std::nullopt};
}

// Completion of an order of player A
OrderEnded completed(int order) { return OrderEnded{OrderNumber{order}, PLAYER_A, EndReason::COMPLETED, std::nullopt}; }

// Activation of an order of player A that interrupted nothing
OrderActivated activated(int order) { return OrderActivated{OrderNumber{order}, PLAYER_A, std::nullopt, {}}; }

// A step of a running game that concludes nothing
State step(State state, int at, const std::vector<ActivationOutcome>& activations, const ArenaEvents& events) {
  return std::get<0>(game_authority::observeStep(std::move(state), StepId{at}, activations, events, BOTH_ALIVE));
}

// Game in which A's order 1 executed from step 2 and completed in step 15, and successor 2, submitted in step 10
// while 1 executed and transmitted at the first opportunity after, step 21, activated in step 22
State withSuccessorExecuting() {
  State state = std::get<0>(game_authority::observeTransmission(runningGame(), transmittedByA(1, 0, 1)));
  state = step(std::move(state), 2, {activated(1)}, {});
  state = std::get<0>(game_authority::observeTransmission(std::move(state), transmittedByA(2, 10, 21)));
  state = step(std::move(state), 15, {}, {completed(1)});
  return step(std::move(state), 22, {activated(2)}, {});
}

// Game in which 1 handed over to 2, which completed in step 25: the chain has one handover
State withOneHandover() { return step(withSuccessorExecuting(), 25, {}, {completed(2)}); }

}  // namespace

/// @section game_authority::startRound

TEST(OrbitalGameAuthority, StartsRunningWithZeroPointsAndFullDuration) {
  // When: a game starts
  game_authority::View view = game_authority::view(runningGame());

  // Then: both players have no points and all 600 steps remain
  EXPECT_EQ(view.players.at(PLAYER_A).banked, 0);
  EXPECT_EQ(view.players.at(PLAYER_B).banked, 0);
  EXPECT_EQ(view.remaining_steps, 600);
  EXPECT_TRUE(game_authority::isRunning(runningGame()));
}

/// @section game_authority::observeStep

/// @subsection scoring

TEST(OrbitalGameAuthority, ShowsBankedAndHeldPointsSeparately) {
  // When: a step with food during an order and food without an order is observed
  auto [state, state_handovers, concluded] = game_authority::observeStep(
      runningGame(),
      StepId{1},
      {},
      {FoodCollected{PLAYER_A, Point{1, 1}, OrderNumber{1}}, FoodCollected{PLAYER_B, Point{2, 2}, std::nullopt}},
      BOTH_ALIVE);

  // Then: A has 10 held points, B 5 banked points, and the game goes on
  game_authority::View view = game_authority::view(state);
  EXPECT_EQ(view.players.at(PLAYER_A).banked, 0);
  EXPECT_EQ(view.players.at(PLAYER_A).held, 10);
  EXPECT_EQ(view.players.at(PLAYER_B).banked, 5);
  EXPECT_FALSE(concluded.has_value());
}

/// @brief F's +20 count once F actually completes
TEST(OrbitalGameAuthority, BanksHeldPointsOnCompletion) {
  // Given: order F of A holds 20 points
  State state = step(
      runningGame(),
      1,
      {},
      {FoodCollected{PLAYER_A, Point{1, 1}, OrderNumber{1}}, FoodCollected{PLAYER_A, Point{2, 1}, OrderNumber{1}}});

  // When: F completes
  State banked = step(state, 2, {}, {completed(1)});

  // Then: the 20 points are banked and nothing is held any more
  game_authority::View view = game_authority::view(banked);
  EXPECT_EQ(view.players.at(PLAYER_A).banked, 20);
  EXPECT_EQ(view.players.at(PLAYER_A).held, 0);
}

TEST(OrbitalGameAuthority, DropsHeldPointsOfInterruptedOrder) {
  // Given: order F of A holds 10 points
  State state = step(runningGame(), 1, {}, {FoodCollected{PLAYER_A, Point{1, 1}, OrderNumber{1}}});

  // When: F is interrupted
  State interrupted =
      step(state, 2, {}, {OrderEnded{OrderNumber{1}, PLAYER_A, EndReason::INTERRUPTED, OrderNumber{2}}});

  // Then: the points are gone and the banked score is unchanged
  game_authority::View view = game_authority::view(interrupted);
  EXPECT_EQ(view.players.at(PLAYER_A).banked, 0);
  EXPECT_EQ(view.players.at(PLAYER_A).held, 0);
}

/// @brief Food collected during an order and its completion in the same step count in order
TEST(OrbitalGameAuthority, BanksFoodCollectedOnArrivalStep) {
  // When: a step with collection during order 1 followed by its completion is observed
  State state = step(runningGame(), 1, {}, {FoodCollected{PLAYER_A, Point{1, 1}, OrderNumber{1}}, completed(1)});

  // Then: the points are banked
  EXPECT_EQ(game_authority::view(state).players.at(PLAYER_A).banked, 10);
}

TEST(OrbitalGameAuthority, AwardsNoPointsForCompletionAlone) {
  // When: the completion of an order without held points is observed
  State state = step(runningGame(), 1, {}, {completed(1)});

  // Then: A has no points
  EXPECT_EQ(game_authority::view(state).players.at(PLAYER_A).banked, 0);
}

/// @brief A replacement's own points start fresh; the interrupted order's points do not move over
TEST(OrbitalGameAuthority, KeepsHeldPointsPerOrder) {
  // Given: order F of A holds 10 points
  State state = step(runningGame(), 1, {}, {FoodCollected{PLAYER_A, Point{1, 1}, OrderNumber{1}}});

  // When: G interrupts F and collects food
  State next = step(state,
                    2,
                    {},
                    {OrderEnded{OrderNumber{1}, PLAYER_A, EndReason::INTERRUPTED, OrderNumber{2}},
                     FoodCollected{PLAYER_A, Point{2, 1}, OrderNumber{2}}});

  // Then: only G's 10 points are held
  game_authority::View view = game_authority::view(next);
  EXPECT_EQ(view.players.at(PLAYER_A).held, 10);
  EXPECT_EQ(view.players.at(PLAYER_A).banked, 0);
}

/// @brief A penalty counts at once and does not touch an executing order's held points
TEST(OrbitalGameAuthority, KeepsHeldPointsOnCollisionPenalty) {
  // Given: order F of B holds 10 points
  State state = step(runningGame(), 1, {}, {FoodCollected{PLAYER_B, Point{1, 1}, OrderNumber{1}}});

  // When: B is bitten
  State bitten = step(state, 2, {}, {Bitten{PLAYER_B, PLAYER_A}});

  // Then: B lost ten banked points and F still holds its 10 points
  game_authority::View view = game_authority::view(bitten);
  EXPECT_EQ(view.players.at(PLAYER_B).banked, -10);
  EXPECT_EQ(view.players.at(PLAYER_B).held, 10);
}

/// @brief Players number their orders independently, so equal numbers of two players hold their points apart
TEST(OrbitalGameAuthority, SettlesHeldPointsOfEachPlayersOrderApart) {
  // Given: A's order 1 and B's order 1 each hold 10 points
  State state = step(
      runningGame(),
      1,
      {},
      {FoodCollected{PLAYER_A, Point{1, 1}, OrderNumber{1}}, FoodCollected{PLAYER_B, Point{2, 2}, OrderNumber{1}}});

  // When: A's order 1 completes
  State banked = step(state, 2, {}, {completed(1)});

  // Then: A banked its 10 points and B's order 1 still holds its own
  game_authority::View view = game_authority::view(banked);
  EXPECT_EQ(view.players.at(PLAYER_A).banked, 10);
  EXPECT_EQ(view.players.at(PLAYER_A).held, 0);
  EXPECT_EQ(view.players.at(PLAYER_B).banked, 0);
  EXPECT_EQ(view.players.at(PLAYER_B).held, 10);
}

TEST(OrbitalGameAuthority, CountsDownRemainingSteps) {
  // When: step 150 is observed
  auto [state, state_handovers, concluded] =
      game_authority::observeStep(runningGame(), StepId{150}, {}, {}, BOTH_ALIVE);

  // Then: 450 steps remain
  EXPECT_EQ(game_authority::view(state).remaining_steps, 450);
}

/// @subsection conclusion

TEST(OrbitalGameAuthority, KeepsRunningBeforeLastStep) {
  // When: step 599 is observed
  auto [state, state_handovers, concluded] =
      game_authority::observeStep(runningGame(), StepId{599}, {}, {}, BOTH_ALIVE);

  // Then: the game still runs
  EXPECT_FALSE(concluded.has_value());
  EXPECT_TRUE(game_authority::isRunning(state));
}

TEST(OrbitalGameAuthority, ConcludesWithLastStep) {
  // When: the last step, 600, is observed
  auto [state, state_handovers, concluded] =
      game_authority::observeStep(runningGame(), pacing_policy::LAST_STEP, {}, {}, BOTH_ALIVE);

  // Then: the game concluded
  EXPECT_TRUE(concluded.has_value());
  EXPECT_FALSE(game_authority::isRunning(state));
}

TEST(OrbitalGameAuthority, ConcludesWhenNoSnakeIsAlive) {
  // When: a step after which no snake is alive is observed
  auto [state, state_handovers, concluded] =
      game_authority::observeStep(runningGame(), StepId{25}, {}, {MutualBite{PLAYER_A, PLAYER_B}}, {});

  // Then: the game concluded early
  EXPECT_TRUE(concluded.has_value());
}

/// @brief The last step of the game still counts, including a completion it brings
TEST(OrbitalGameAuthority, BanksCompletionOnFinalStepBeforeConclusion) {
  // Given: order F of A holds 20 points
  auto [state, state_handovers, first] = game_authority::observeStep(
      runningGame(),
      StepId{5},
      {},
      {FoodCollected{PLAYER_A, Point{1, 1}, OrderNumber{1}}, FoodCollected{PLAYER_A, Point{2, 1}, OrderNumber{1}}},
      BOTH_ALIVE);

  // When: F completes in the last step
  auto [final_state, final_state_handovers, concluded] =
      game_authority::observeStep(state,
                                  pacing_policy::LAST_STEP,
                                  {},
                                  {OrderEnded{OrderNumber{1}, PLAYER_A, EndReason::COMPLETED, std::nullopt}},
                                  BOTH_ALIVE);

  // Then: A's 20 points count and A wins
  EXPECT_EQ(game_authority::view(final_state).players.at(PLAYER_A).banked, 20);
  EXPECT_EQ(concluded->result, (Result{PlayerId{PLAYER_A}}));
}

/// @brief Conclusion ends the round for an order still executing; the Scoring Policy drops its points then
TEST(OrbitalGameAuthority, SettlesExecutingOrderAsEndedByRoundAtConclusion) {
  // Given: B has 10 banked points and A's executing order holds 20 points
  auto [state, state_handovers, first] =
      game_authority::observeStep(runningGame(),
                                  StepId{5},
                                  {},
                                  {FoodCollected{PLAYER_B, Point{3, 3}, std::nullopt},
                                   FoodCollected{PLAYER_A, Point{1, 1}, OrderNumber{1}},
                                   FoodCollected{PLAYER_A, Point{2, 1}, OrderNumber{1}}},
                                  BOTH_ALIVE);

  // When: the game concludes while A's order still executes
  auto [final_state, final_state_handovers, concluded] =
      game_authority::observeStep(state, pacing_policy::LAST_STEP, {}, {}, BOTH_ALIVE);

  // Then: A's held points are dropped and B wins
  EXPECT_EQ(game_authority::view(final_state).players.at(PLAYER_A).held, 0);
  EXPECT_EQ(concluded->result, (Result{PlayerId{PLAYER_B}}));
}

TEST(OrbitalGameAuthority, DrawsOnEqualBankedScores) {
  // When: the game concludes with equal banked scores
  auto [state, state_handovers, concluded] =
      game_authority::observeStep(runningGame(), pacing_policy::LAST_STEP, {}, {}, BOTH_ALIVE);

  // Then: the result is a draw
  EXPECT_EQ(concluded->result, (Result{std::nullopt}));
}

/// @brief The Arena Authority's ROUND_ENDED events after conclusion must not change the final scores
TEST(OrbitalGameAuthority, IgnoresStepsAfterConclusion) {
  // Given: a concluded game in which B leads
  auto [state, state_handovers, concluded] = game_authority::observeStep(
      runningGame(), pacing_policy::LAST_STEP, {}, {FoodCollected{PLAYER_B, Point{3, 3}, std::nullopt}}, BOTH_ALIVE);

  // When: further events are observed
  auto [later, later_handovers, again] =
      game_authority::observeStep(state,
                                  pacing_policy::LAST_STEP,
                                  {},
                                  {OrderEnded{OrderNumber{1}, PLAYER_A, EndReason::ROUND_ENDED, std::nullopt},
                                   FoodCollected{PLAYER_A, Point{1, 1}, std::nullopt}},
                                  BOTH_ALIVE);

  // Then: nothing changes and no second conclusion is announced
  EXPECT_FALSE(again.has_value());
  EXPECT_EQ(game_authority::view(later).players.at(PLAYER_A).banked, 0);
  EXPECT_EQ(game_authority::view(later).result, (Result{PlayerId{PLAYER_B}}));
}

/// @subsection handover chain
///
/// Opportunities come in steps 1, 21, 40, 59 and 77. A successor is sent at the first opportunity at or after its
/// predecessor's completion.

/// @brief The successor earns the bonus with its own completion, when it took over from a predecessor that completed
TEST(OrbitalGameAuthority, AwardsBaseBonusWhenSuccessorCompletes) {
  // Given: 1 completed in step 15 and successor 2, submitted while 1 executed, executes
  State state = withSuccessorExecuting();

  // When: 2 completes
  State observed = step(std::move(state), 25, {}, {completed(2)});

  // Then: A banks the base handover bonus and the chain has one handover
  game_authority::PlayerScore score = game_authority::view(observed).players.at(PLAYER_A);
  EXPECT_EQ(score.banked, 10);
  EXPECT_EQ(score.chain, 1);
  EXPECT_EQ(score.next_handover, 20);
}

/// @brief The handover is decided when the successor takes over; its bonus is held until the successor completes
TEST(OrbitalGameAuthority, HoldsBonusWhenSuccessorTakesOver) {
  // When: 1 completed and successor 2 has activated
  State state = withSuccessorExecuting();

  // Then: the chain has one handover, and its bonus is held, not yet banked
  game_authority::PlayerScore score = game_authority::view(state).players.at(PLAYER_A);
  EXPECT_EQ(score.chain, 1);
  EXPECT_EQ(score.held, 10);
  EXPECT_EQ(score.banked, 0);
}

/// @brief A held handover bonus shares the fate of the successor's held food points
TEST(OrbitalGameAuthority, DropsHeldBonusWhenSuccessorFails) {
  // Given: successor 2 took over and holds the handover bonus
  State state = withSuccessorExecuting();

  // When: A's snake dies while executing 2
  State observed =
      step(std::move(state),
           23,
           {},
           {SelfBitten{PLAYER_A}, OrderEnded{OrderNumber{2}, PLAYER_A, EndReason::ELIMINATED, std::nullopt}});

  // Then: the bonus is dropped, only the bite penalty counts, and the chain is broken
  game_authority::PlayerScore score = game_authority::view(observed).players.at(PLAYER_A);
  EXPECT_EQ(score.held, 0);
  EXPECT_EQ(score.banked, -10);
  EXPECT_EQ(score.chain, 0);
}

/// @brief A takeover that is no handover breaks the chain at once, not only when that order ends
TEST(OrbitalGameAuthority, BreaksChainWhenOrderTakesOverWithoutHandover) {
  // Given: the chain has one handover; 3 was submitted in step 40, after that step's opportunity that followed 2's
  //        completion, and transmitted in step 59
  State state = std::get<0>(game_authority::observeTransmission(withOneHandover(), transmittedByA(3, 40, 59)));

  // When: 3 activates
  State observed = step(std::move(state), 60, {activated(3)}, {});

  // Then: the chain is broken while the earned bonus stays banked
  game_authority::PlayerScore score = game_authority::view(observed).players.at(PLAYER_A);
  EXPECT_EQ(score.chain, 0);
  EXPECT_EQ(score.banked, 10);
}

TEST(OrbitalGameAuthority, DoublesBonusForConsecutiveHandover) {
  // Given: 1 handed over to 2, which executed from step 22 to step 25; successor 3 was submitted in step 23,
  //        transmitted in step 40 and activated in step 41
  State state = std::get<0>(game_authority::observeTransmission(withSuccessorExecuting(), transmittedByA(3, 23, 40)));
  state = step(std::move(state), 25, {}, {completed(2)});
  state = step(std::move(state), 41, {activated(3)}, {});

  // When: 3 completes
  State observed = step(std::move(state), 45, {}, {completed(3)});

  // Then: the second handover earns twice the base bonus
  game_authority::PlayerScore score = game_authority::view(observed).players.at(PLAYER_A);
  EXPECT_EQ(score.banked, 10 + 20);
  EXPECT_EQ(score.chain, 2);
}

/// @brief The successor need not take over right away; driving straight in between is no reason to lose the handover
TEST(OrbitalGameAuthority, AwardsBonusAfterIdleGapBetweenPredecessorAndSuccessor) {
  // Given: 1 completed in step 5; successor 2, submitted in step 3, was transmitted in step 21 and activated at
  //        step 22
  State state = std::get<0>(game_authority::observeTransmission(runningGame(), transmittedByA(1, 0, 1)));
  state = step(std::move(state), 2, {activated(1)}, {});
  state = step(std::move(state), 5, {}, {completed(1)});
  state = std::get<0>(game_authority::observeTransmission(std::move(state), transmittedByA(2, 3, 21)));
  state = step(std::move(state), 22, {activated(2)}, {});

  // When: 2 completes
  State observed = step(std::move(state), 25, {}, {completed(2)});

  // Then: the handover counts
  EXPECT_EQ(game_authority::view(observed).players.at(PLAYER_A).banked, 10);
}

/// @brief The window reaches past the predecessor's arrival up to the opportunity that sends the successor
TEST(OrbitalGameAuthority, AwardsBonusToSuccessorSubmittedAfterCompletionBeforeOpportunity) {
  // Given: 1 completed in step 15; 2 was submitted in step 17, transmitted in step 21 and activated in step 22
  State state = std::get<0>(game_authority::observeTransmission(runningGame(), transmittedByA(1, 0, 1)));
  state = step(std::move(state), 2, {activated(1)}, {});
  state = step(std::move(state), 15, {}, {completed(1)});
  state = std::get<0>(game_authority::observeTransmission(std::move(state), transmittedByA(2, 17, 21)));
  state = step(std::move(state), 22, {activated(2)}, {});

  // When: 2 completes
  State observed = step(std::move(state), 25, {}, {completed(2)});

  // Then: the handover counts
  EXPECT_EQ(game_authority::view(observed).players.at(PLAYER_A).banked, 10);
}

/// @brief Submitting too late: the first opportunity after the predecessor's completion has passed
TEST(OrbitalGameAuthority, DeniesHandoverToSuccessorSentAfterFirstOpportunityFromCompletion) {
  // Given: 1 completed in step 15; 2 was submitted in step 21, after that step's opportunity, transmitted in step 40
  //        and activated in step 41
  State state = std::get<0>(game_authority::observeTransmission(runningGame(), transmittedByA(1, 0, 1)));
  state = step(std::move(state), 2, {activated(1)}, {});
  state = step(std::move(state), 15, {}, {completed(1)});
  state = std::get<0>(game_authority::observeTransmission(std::move(state), transmittedByA(2, 21, 40)));
  state = step(std::move(state), 41, {activated(2)}, {});

  // When: 2 completes
  State observed = step(std::move(state), 45, {}, {completed(2)});

  // Then: the completion is no handover
  game_authority::PlayerScore score = game_authority::view(observed).players.at(PLAYER_A);
  EXPECT_EQ(score.banked, 0);
  EXPECT_EQ(score.chain, 0);
}

/// @brief The opportunity in a step comes after that step's completions, so it sends the successor in time
TEST(OrbitalGameAuthority, AwardsBonusToSuccessorSentAtOpportunityOfCompletionStep) {
  // Given: 1 completed in step 21; 2, submitted in step 15, was transmitted at that step's opportunity and activated
  //        in step 22
  State state = std::get<0>(game_authority::observeTransmission(runningGame(), transmittedByA(1, 0, 1)));
  state = step(std::move(state), 2, {activated(1)}, {});
  state = step(std::move(state), 21, {}, {completed(1)});
  state = std::get<0>(game_authority::observeTransmission(std::move(state), transmittedByA(2, 15, 21)));
  state = step(std::move(state), 22, {activated(2)}, {});

  // When: 2 completes
  State observed = step(std::move(state), 25, {}, {completed(2)});

  // Then: the handover counts
  EXPECT_EQ(game_authority::view(observed).players.at(PLAYER_A).banked, 10);
}

/// @brief A submission in the step of the activation was made after the step's movement, so the predecessor executed
/// then
TEST(OrbitalGameAuthority, AwardsBonusToSuccessorSubmittedInStepOfPredecessorActivation) {
  // Given: 1 activated in step 2 and completed in step 15; 2 was submitted in step 2, transmitted in step 21 and
  //        activated in step 22
  State state = std::get<0>(game_authority::observeTransmission(runningGame(), transmittedByA(1, 0, 1)));
  state = step(std::move(state), 2, {activated(1)}, {});
  state = std::get<0>(game_authority::observeTransmission(std::move(state), transmittedByA(2, 2, 21)));
  state = step(std::move(state), 15, {}, {completed(1)});
  state = step(std::move(state), 22, {activated(2)}, {});

  // When: 2 completes
  State observed = step(std::move(state), 25, {}, {completed(2)});

  // Then: the handover counts
  EXPECT_EQ(game_authority::view(observed).players.at(PLAYER_A).banked, 10);
}

/// @brief An order chosen after its predecessor was sent but before it activated follows it without interrupting;
/// still, it was not chosen while the predecessor executed
TEST(OrbitalGameAuthority, DeniesHandoverToSuccessorSubmittedBeforePredecessorActivated) {
  // Given: 1 was transmitted in step 1, activated in step 2 and completed in step 15; 2 was submitted in step 1,
  //        transmitted in step 21 and activated in step 22
  State state = std::get<0>(game_authority::observeTransmission(runningGame(), transmittedByA(1, 0, 1)));
  state = std::get<0>(game_authority::observeTransmission(std::move(state), transmittedByA(2, 1, 21)));
  state = step(std::move(state), 2, {activated(1)}, {});
  state = step(std::move(state), 15, {}, {completed(1)});
  state = step(std::move(state), 22, {activated(2)}, {});

  // When: 2 completes
  State observed = step(std::move(state), 25, {}, {completed(2)});

  // Then: the completion is no handover
  EXPECT_EQ(game_authority::view(observed).players.at(PLAYER_A).banked, 0);
}

/// @brief Changing one's mind about the successor costs its handover, even though only the last choice was sent
TEST(OrbitalGameAuthority, DeniesHandoverToSuccessorThatReplacedSelection) {
  // Given: 1 completed in step 15; successor 2 replaced selection 9 before it was transmitted, and executes
  State state = std::get<0>(game_authority::observeTransmission(runningGame(), transmittedByA(1, 0, 1)));
  state = step(std::move(state), 2, {activated(1)}, {});
  state = std::get<0>(game_authority::observeTransmission(
      std::move(state),
      OrderTransmitted{ROUND, OrderNumber{2}, PLAYER_A, Point{0, 0}, StepId{10}, StepId{21}, OrderNumber{9}}));
  state = step(std::move(state), 15, {}, {completed(1)});
  state = step(std::move(state), 22, {activated(2)}, {});

  // When: 2 completes
  State observed = step(std::move(state), 25, {}, {completed(2)});

  // Then: the completion is no handover
  game_authority::PlayerScore score = game_authority::view(observed).players.at(PLAYER_A);
  EXPECT_EQ(score.banked, 0);
  EXPECT_EQ(score.chain, 0);
}

/// @brief The chain follows what executed: a second order submitted during the same predecessor follows the first
/// successor, during which it was not submitted
TEST(OrbitalGameAuthority, DeniesHandoverToSecondOrderSubmittedDuringSamePredecessor) {
  // Given: 1 handed over to 2, which completed in step 25; 3, submitted in step 12 while 1 executed, was transmitted
  //        at step 40 and activated in step 41
  State state = std::get<0>(game_authority::observeTransmission(withOneHandover(), transmittedByA(3, 12, 40)));
  state = step(std::move(state), 41, {activated(3)}, {});

  // When: 3 completes
  State observed = step(std::move(state), 45, {}, {completed(3)});

  // Then: only 2's handover counts and the chain is broken
  game_authority::PlayerScore score = game_authority::view(observed).players.at(PLAYER_A);
  EXPECT_EQ(score.banked, 10);
  EXPECT_EQ(score.chain, 0);
}

/// @brief A successor that activates too early interrupts instead of taking over, and cannot hand over later
TEST(OrbitalGameAuthority, BreaksChainWhenSuccessorInterruptsExecutingOrder) {
  // Given: the chain has one handover; 3 executes from step 41; 4, submitted in step 41 and transmitted in step 59,
  //        interrupted 3 at step 60
  State state = std::get<0>(game_authority::observeTransmission(withOneHandover(), transmittedByA(3, 25, 40)));
  state = step(std::move(state), 41, {activated(3)}, {});
  state = std::get<0>(game_authority::observeTransmission(std::move(state), transmittedByA(4, 41, 59)));
  state = step(std::move(state),
               60,
               {OrderActivated{OrderNumber{4}, PLAYER_A, OrderNumber{3}, {}}},
               {OrderEnded{OrderNumber{3}, PLAYER_A, EndReason::INTERRUPTED, OrderNumber{4}}});

  // When: 4 completes
  State observed = step(std::move(state), 64, {}, {completed(4)});

  // Then: the chain is broken and 4 earned nothing
  game_authority::PlayerScore score = game_authority::view(observed).players.at(PLAYER_A);
  EXPECT_EQ(score.chain, 0);
  EXPECT_EQ(score.banked, 10);
}

TEST(OrbitalGameAuthority, BreaksChainOnCompletionWithoutHandover) {
  // Given: the chain has one handover; 3 was submitted in step 40, after that step's opportunity that followed 2's
  //        completion, transmitted in step 59 and executes
  State state = std::get<0>(game_authority::observeTransmission(withOneHandover(), transmittedByA(3, 40, 59)));
  state = step(std::move(state), 60, {activated(3)}, {});

  // When: 3 completes
  State observed = step(std::move(state), 63, {}, {completed(3)});

  // Then: no further bonus is banked and the chain is broken
  game_authority::PlayerScore score = game_authority::view(observed).players.at(PLAYER_A);
  EXPECT_EQ(score.banked, 10);
  EXPECT_EQ(score.chain, 0);
  EXPECT_EQ(score.next_handover, 10);
}

TEST(OrbitalGameAuthority, BreaksChainWhenExecutingOrderIsEliminated) {
  // Given: the chain has one handover and 3 executes
  State state = std::get<0>(game_authority::observeTransmission(withOneHandover(), transmittedByA(3, 25, 40)));
  state = step(std::move(state), 41, {activated(3)}, {});

  // When: A's snake dies while executing 3
  State observed =
      step(std::move(state),
           42,
           {},
           {SelfBitten{PLAYER_A}, OrderEnded{OrderNumber{3}, PLAYER_A, EndReason::ELIMINATED, std::nullopt}});

  // Then: the chain is broken
  EXPECT_EQ(game_authority::view(observed).players.at(PLAYER_A).chain, 0);
}

/// @brief A snake can die between orders, right after a handover; no order ending is reported then
TEST(OrbitalGameAuthority, BreaksChainWhenSnakeDiesWithoutExecutingOrder) {
  // Given: the chain has one handover and no order executes
  State state = withOneHandover();

  // When: A's snake dies in the next step
  auto [observed, observed_handovers, concluded] =
      game_authority::observeStep(std::move(state), StepId{26}, {}, {SelfBitten{PLAYER_A}}, {PLAYER_B});

  // Then: the chain is broken, while the earned bonus stays banked
  game_authority::PlayerScore score = game_authority::view(observed).players.at(PLAYER_A);
  EXPECT_EQ(score.chain, 0);
  EXPECT_EQ(score.banked, 10 - 10);
}

/// @brief A rejected order never executed: it neither breaks the chain nor becomes anyone's predecessor
TEST(OrbitalGameAuthority, PassesOverRejectedOrderInChain) {
  // Given: the chain has one handover; 3 executes from step 41 to step 63; 4 was rejected in step 60; 5, submitted at
  //        step 59 while 3 executed, was transmitted in step 77 and activated in step 78
  State state = std::get<0>(game_authority::observeTransmission(withOneHandover(), transmittedByA(3, 23, 40)));
  state = step(std::move(state), 41, {activated(3)}, {});
  state = std::get<0>(game_authority::observeTransmission(std::move(state), transmittedByA(4, 41, 59)));
  state = step(std::move(state), 60, {OrderRejected{OrderNumber{4}, PLAYER_A, RejectionReason::TARGET_OFF_BOARD}}, {});
  state = std::get<0>(game_authority::observeTransmission(std::move(state), transmittedByA(5, 59, 77)));
  state = step(std::move(state), 63, {}, {completed(3)});
  state = step(std::move(state), 78, {activated(5)}, {});

  // When: 5 completes
  State observed = step(std::move(state), 80, {}, {completed(5)});

  // Then: 3 and 5 both handed over, so the chain has three handovers
  game_authority::PlayerScore score = game_authority::view(observed).players.at(PLAYER_A);
  EXPECT_EQ(score.banked, 10 + 20 + 40);
  EXPECT_EQ(score.chain, 3);
}

/// @brief The successor's held points are its own: its handover bonus comes in addition to them
TEST(OrbitalGameAuthority, BanksSuccessorPointsWithItsHandoverBonus) {
  // Given: successor 2 executes and collected food
  State state = step(withSuccessorExecuting(), 23, {}, {FoodCollected{PLAYER_A, Point{1, 1}, OrderNumber{2}}});

  // When: 2 completes
  State observed = step(std::move(state), 25, {}, {completed(2)});

  // Then: its food points and the handover bonus are banked
  EXPECT_EQ(game_authority::view(observed).players.at(PLAYER_A).banked, 10 + 10);
}

TEST(OrbitalGameAuthority, BreaksChainAtConclusion) {
  // Given: the chain has one handover
  State state = withOneHandover();

  // When: the game concludes
  auto [observed, observed_handovers, concluded] =
      game_authority::observeStep(std::move(state), pacing_policy::LAST_STEP, {}, {}, BOTH_ALIVE);

  // Then: the earned bonus counts, and no chain is left
  game_authority::PlayerScore score = game_authority::view(observed).players.at(PLAYER_A);
  EXPECT_EQ(score.banked, 10);
  EXPECT_EQ(score.chain, 0);
}

/// @brief A handover bonus counts only when its order completes, which the end of the game prevents
TEST(OrbitalGameAuthority, DropsHeldHandoverBonusAtConclusion) {
  // Given: successor 2 took over as a handover and still executes, holding its bonus
  State state = withSuccessorExecuting();

  // When: the game concludes
  auto [observed, observed_handovers, concluded] =
      game_authority::observeStep(std::move(state), pacing_policy::LAST_STEP, {}, {}, BOTH_ALIVE);

  // Then: the bonus is dropped and no chain is left
  game_authority::PlayerScore score = game_authority::view(observed).players.at(PLAYER_A);
  EXPECT_EQ(score.held, 0);
  EXPECT_EQ(score.banked, 0);
  EXPECT_EQ(score.chain, 0);
}

/// @subsection handovers reported

TEST(OrbitalGameAuthority, ReportsHandoverWhenSuccessorTakesOver) {
  // Given: 1 executed from step 2 and completed in step 15; successor 2, submitted in step 10, was sent in step 21
  State state = std::get<0>(game_authority::observeTransmission(runningGame(), transmittedByA(1, 0, 1)));
  state = step(std::move(state), 2, {activated(1)}, {});
  state = std::get<0>(game_authority::observeTransmission(std::move(state), transmittedByA(2, 10, 21)));
  state = step(std::move(state), 15, {}, {completed(1)});

  // When: 2 activates in step 22
  auto [observed, handovers, concluded] =
      game_authority::observeStep(std::move(state), StepId{22}, {activated(2)}, {}, BOTH_ALIVE);

  // Then: the first handover of the chain is reported with its bonus
  EXPECT_EQ(handovers, (std::vector<game_authority::HandoverGranted>{{PLAYER_A, OrderNumber{2}, 1, 10}}));
}

/// @brief A player's chain follows only that player's orders, also where another player's order has the same number
TEST(OrbitalGameAuthority, ReportsNoHandoverForOtherPlayersOrderWithSameNumber) {
  // Given: 1 executed from step 2 and completed in step 15; successor 2, submitted in step 10, was sent in step 21
  State state = std::get<0>(game_authority::observeTransmission(runningGame(), transmittedByA(1, 0, 1)));
  state = step(std::move(state), 2, {activated(1)}, {});
  state = std::get<0>(game_authority::observeTransmission(std::move(state), transmittedByA(2, 10, 21)));
  state = step(std::move(state), 15, {}, {completed(1)});

  // When: B's order 2 activates in step 22
  auto [observed, handovers, concluded] = game_authority::observeStep(
      std::move(state), StepId{22}, {OrderActivated{OrderNumber{2}, PLAYER_B, std::nullopt, {}}}, {}, BOTH_ALIVE);

  // Then: no handover is reported
  EXPECT_TRUE(handovers.empty());
}

TEST(OrbitalGameAuthority, ReportsNoHandoverForInterruption) {
  // Given: A's order 1 executes and successor 2 was sent before 1 arrived
  State state = std::get<0>(game_authority::observeTransmission(runningGame(), transmittedByA(1, 0, 1)));
  state = step(std::move(state), 2, {activated(1)}, {});
  state = std::get<0>(game_authority::observeTransmission(std::move(state), transmittedByA(2, 1, 21)));

  // When: 2 activates over 1 at step 22
  auto [observed, handovers, concluded] = game_authority::observeStep(
      std::move(state), StepId{22}, {OrderActivated{OrderNumber{2}, PLAYER_A, OrderNumber{1}, {}}}, {}, BOTH_ALIVE);

  // Then: no handover is reported
  EXPECT_TRUE(handovers.empty());
}

/// @section game_authority::handoverProspect

namespace {

// Game in which A's order 1 executes from step 2
State withOrderExecuting() {
  State state = std::get<0>(game_authority::observeTransmission(runningGame(), transmittedByA(1, 0, 1)));
  return step(std::move(state), 2, {activated(1)}, {});
}

// Game in which A's order 1 executed from step 2 and completed in step 15; the next opportunity comes at step 21
State withOrderCompleted() { return step(withOrderExecuting(), 15, {}, {completed(1)}); }

}  // namespace

/// @brief A successor chosen now is sent at the first opportunity at or after the expected completion
TEST(OrbitalGameAuthority, PromptsHandoverBeforeExpectedCompletion) {
  // When: the prospect is queried in step 5 while 1 is expected to complete at step 15
  game_authority::HandoverProspect prospect = game_authority::handoverProspect(
      withOrderExecuting(), PLAYER_A, {StepId{5}, StepId{15}, std::nullopt, false, false});

  // Then: A is prompted to choose now
  EXPECT_EQ(prospect, game_authority::HandoverProspect::CHOOSE_NOW);
}

/// @brief After the arrival a successor still hands over until the opportunity that would send it
TEST(OrbitalGameAuthority, KeepsHandoverPromptAfterCompletionUntilOpportunity) {
  // When: the prospect is queried in step 17, after 1 completed and before the opportunity at step 21
  game_authority::HandoverProspect prospect = game_authority::handoverProspect(
      withOrderCompleted(), PLAYER_A, {StepId{17}, std::nullopt, std::nullopt, false, false});

  // Then: A is still prompted
  EXPECT_EQ(prospect, game_authority::HandoverProspect::CHOOSE_NOW);
}

/// @brief A completion stays open only until the opportunity after it; a choice made later cannot hand over
TEST(OrbitalGameAuthority, ClosesHandoverPromptAtOpportunityAfterCompletion) {
  // When: the prospect is queried in step 30, after the opportunity at step 21
  game_authority::HandoverProspect prospect = game_authority::handoverProspect(
      withOrderCompleted(), PLAYER_A, {StepId{30}, std::nullopt, std::nullopt, false, false});

  // Then: A is not prompted
  EXPECT_EQ(prospect, game_authority::HandoverProspect::NONE);
}

TEST(OrbitalGameAuthority, OffersNoHandoverAfterInterruption) {
  // Given: A's order 1 was interrupted in step 22
  State state = step(
      withOrderExecuting(), 22, {}, {OrderEnded{OrderNumber{1}, PLAYER_A, EndReason::INTERRUPTED, OrderNumber{2}}});

  // When: the prospect is queried in step 23
  game_authority::HandoverProspect prospect =
      game_authority::handoverProspect(state, PLAYER_A, {StepId{23}, std::nullopt, std::nullopt, false, false});

  // Then: A is not prompted
  EXPECT_EQ(prospect, game_authority::HandoverProspect::NONE);
}

/// @brief A successor chosen in the window is shown as ready, so the player knows not to change it
TEST(OrbitalGameAuthority, ShowsSuccessorReadyForQualifyingSelection) {
  // When: the prospect is queried in step 5 with a selection submitted then
  game_authority::HandoverProspect prospect = game_authority::handoverProspect(
      withOrderExecuting(), PLAYER_A, {StepId{5}, StepId{15}, StepId{5}, false, false});

  // Then: the successor is ready
  EXPECT_EQ(prospect, game_authority::HandoverProspect::SUCCESSOR_READY);
}

/// @brief Changing one's mind costs the handover, as for a counted handover
TEST(OrbitalGameAuthority, PromisesNoHandoverForReplacingSelection) {
  // When: the prospect is queried with a selection that replaced another
  game_authority::HandoverProspect prospect =
      game_authority::handoverProspect(withOrderExecuting(), PLAYER_A, {StepId{5}, StepId{15}, StepId{5}, true, false});

  // Then: no handover is promised
  EXPECT_EQ(prospect, game_authority::HandoverProspect::NONE);
}

/// @brief The predecessor's observed activation counts, not an estimate of it
TEST(OrbitalGameAuthority, PromisesNoHandoverForSelectionBeforePredecessorActivated) {
  // When: the prospect is queried with a selection submitted in step 1, before 1 activated in step 2
  game_authority::HandoverProspect prospect = game_authority::handoverProspect(
      withOrderExecuting(), PLAYER_A, {StepId{5}, StepId{15}, StepId{1}, false, false});

  // Then: no handover is promised
  EXPECT_EQ(prospect, game_authority::HandoverProspect::NONE);
}

/// @brief A transmitted order takes over next; whatever is chosen now follows that order
TEST(OrbitalGameAuthority, PromisesNoHandoverWhileTransmittedOrderWaits) {
  // When: the prospect is queried while a transmitted order waits for activation
  game_authority::HandoverProspect prospect = game_authority::handoverProspect(
      withOrderExecuting(), PLAYER_A, {StepId{5}, StepId{15}, std::nullopt, false, true});

  // Then: no handover is promised
  EXPECT_EQ(prospect, game_authority::HandoverProspect::NONE);
}

/// @brief The last opportunity of the game is in step 599; a choice made after it is never transmitted
TEST(OrbitalGameAuthority, PromisesNoHandoverWhoseTransmissionFallsAfterGameEnd) {
  // When: the prospect is queried in step 599 while the executing order is expected to complete in step 600
  game_authority::HandoverProspect prospect = game_authority::handoverProspect(
      withOrderExecuting(), PLAYER_A, {StepId{599}, StepId{600}, std::nullopt, false, false});

  // Then: no handover is promised
  EXPECT_EQ(prospect, game_authority::HandoverProspect::NONE);
}

/// @section game_authority::observeTransmission

namespace {

// Steps in which 1 executed from step 2 to step 15 and 2 from step 22 to step 25, with only 1's transmission known
State withSuccessorTransmissionUnknown() {
  State state = std::get<0>(game_authority::observeTransmission(runningGame(), transmittedByA(1, 0, 1)));
  state = step(std::move(state), 2, {activated(1)}, {});
  state = step(std::move(state), 15, {}, {completed(1)});
  state = step(std::move(state), 22, {activated(2)}, {});
  return step(std::move(state), 25, {}, {completed(2)});
}

}  // namespace

TEST(OrbitalGameAuthority, ReportsHandoverWhenSuccessorTransmissionIsLearnedLate) {
  // Given: 1 and 2 executed and completed before 2's transmission is known
  State state = withSuccessorTransmissionUnknown();

  // When: 2's transmission, submitted in step 10 and sent in step 21, is learned
  auto [observed, handovers] = game_authority::observeTransmission(std::move(state), transmittedByA(2, 10, 21));

  // Then: the handover is reported now
  EXPECT_EQ(handovers, (std::vector<game_authority::HandoverGranted>{{PLAYER_A, OrderNumber{2}, 1, 10}}));
}

/// @brief A player-local Order Selection Authority may synchronize its history after the Arena Authority reported the
/// completion
TEST(OrbitalGameAuthority, BanksHandoverWhenSuccessorTransmissionIsLearnedLate) {
  // Given: 1 and 2 executed and completed before 2's transmission is known
  State state = withSuccessorTransmissionUnknown();

  // When: 2's transmission, submitted in step 10 and sent in step 21, is learned
  State observed = std::get<0>(game_authority::observeTransmission(std::move(state), transmittedByA(2, 10, 21)));

  // Then: the handover bonus is banked as if the transmission had been known in time
  game_authority::PlayerScore score = game_authority::view(observed).players.at(PLAYER_A);
  EXPECT_EQ(score.banked, 10);
  EXPECT_EQ(score.chain, 1);
}

TEST(OrbitalGameAuthority, BanksLateTransmissionOnlyOnce) {
  // Given: 2's late transmission was learned and banked
  State state =
      std::get<0>(game_authority::observeTransmission(withSuccessorTransmissionUnknown(), transmittedByA(2, 10, 21)));

  // When: the same transmission is learned again
  State observed = std::get<0>(game_authority::observeTransmission(std::move(state), transmittedByA(2, 10, 21)));

  // Then: the bonus is not banked twice
  EXPECT_EQ(game_authority::view(observed).players.at(PLAYER_A).banked, 10);
}

TEST(OrbitalGameAuthority, IgnoresTransmissionOfAnotherRound) {
  // Given: 1 and 2 executed and completed before 2's transmission is known
  State state = withSuccessorTransmissionUnknown();

  // When: a transmission of another round is learned
  State observed = std::get<0>(game_authority::observeTransmission(
      std::move(state),
      OrderTransmitted{RoundId{2}, OrderNumber{2}, PLAYER_A, Point{0, 0}, StepId{10}, StepId{21}, std::nullopt}));

  // Then: nothing is banked
  EXPECT_EQ(game_authority::view(observed).players.at(PLAYER_A).banked, 0);
}

/// @brief Final standings stay final, also for transmissions still on their way
TEST(OrbitalGameAuthority, IgnoresTransmissionLearnedAfterConclusion) {
  // Given: 1 and 2 executed, and 2 completed on the final step before its transmission is known
  State state = std::get<0>(game_authority::observeTransmission(runningGame(), transmittedByA(1, 0, 1)));
  state = step(std::move(state), 2, {activated(1)}, {});
  state = step(std::move(state), 15, {}, {completed(1)});
  state = step(std::move(state), 22, {activated(2)}, {});
  state = std::get<0>(
      game_authority::observeStep(std::move(state), pacing_policy::LAST_STEP, {}, {completed(2)}, BOTH_ALIVE));

  // When: 2's transmission is learned after conclusion
  State observed = std::get<0>(game_authority::observeTransmission(std::move(state), transmittedByA(2, 10, 21)));

  // Then: nothing is banked
  EXPECT_EQ(game_authority::view(observed).players.at(PLAYER_A).banked, 0);
}

/// @brief Synchronization may deliver the transmission of an order that executed in between last; the histories are
/// taken as they are delivered
TEST(OrbitalGameAuthority, WithdrawsHandoverBonusWhenLateTransmissionRevealsOrderInBetween) {
  // Given: 1 executed until step 15, an order 3 not yet known executed from step 16 to step 19, then 2, submitted at
  //        step 10 and sent in step 21, executed from step 22 to step 25 and was banked as handing over from 1
  State state = std::get<0>(game_authority::observeTransmission(runningGame(), transmittedByA(1, 0, 1)));
  state = step(std::move(state), 2, {activated(1)}, {});
  state = std::get<0>(game_authority::observeTransmission(std::move(state), transmittedByA(2, 10, 21)));
  state = step(std::move(state), 15, {}, {completed(1)});
  state = step(std::move(state), 16, {activated(3)}, {});
  state = step(std::move(state), 19, {}, {completed(3)});
  state = step(std::move(state), 22, {activated(2)}, {});
  state = step(std::move(state), 25, {}, {completed(2)});

  // When: 3's transmission is learned, submitted in step 14
  State observed = std::get<0>(game_authority::observeTransmission(std::move(state), transmittedByA(3, 14, 15)));

  // Then: 2's predecessor was 3, which was not executing when 2 was submitted, so the bonus banked before is withdrawn
  game_authority::PlayerScore score = game_authority::view(observed).players.at(PLAYER_A);
  EXPECT_EQ(score.banked, 0);
  EXPECT_EQ(score.chain, 0);
}

/// @brief Resending must not move a submission in time; its first record is the domain fact
TEST(OrbitalGameAuthority, KeepsFirstSubmissionStepOfAnOrder) {
  // Given: 2's transmission with a submission at step 1, before 1 activated in step 2, was learned
  State state =
      std::get<0>(game_authority::observeTransmission(withSuccessorTransmissionUnknown(), transmittedByA(2, 1, 21)));

  // When: a record for order 2 with a later submission step is learned
  State observed = std::get<0>(game_authority::observeTransmission(std::move(state), transmittedByA(2, 10, 21)));

  // Then: the first step counts, so the completion stays no handover
  EXPECT_EQ(game_authority::view(observed).players.at(PLAYER_A).banked, 0);
}

}  // namespace orbital
}  // namespace snake
