// Tests for the Orbital Game Authority: scoring of steps, conclusion and result.

#include <gtest/gtest.h>

#include <chrono>
#include <set>
#include <tuple>
#include <vector>

#include "orbital/game_authority.hpp"

namespace snake {
namespace orbital {

using std::chrono::milliseconds;

namespace {

using game_authority::Result;
using game_authority::State;

constexpr RoundId ROUND{1};
const std::set<PlayerId> BOTH_ALIVE{PLAYER_A, PLAYER_B};

State runningGame() { return game_authority::startRound(ROUND, {PLAYER_A, PLAYER_B}); }

// Transmission of an order of player A that replaced no earlier selection
OrderTransmitted transmittedByA(int order, int submitted_ms, int transmitted_ms) {
  return OrderTransmitted{
      ROUND, OrderId{order}, PLAYER_A, milliseconds{submitted_ms}, milliseconds{transmitted_ms}, std::nullopt};
}

OrderEnded completed(int order) { return OrderEnded{OrderId{order}, EndReason::COMPLETED, std::nullopt}; }

OrderActivated activated(int order) { return OrderActivated{OrderId{order}, std::nullopt, {}}; }

// A step of a running game that concludes nothing
State step(State state, int at_ms, const std::vector<ActivationOutcome>& activations, const ArenaEvents& events) {
  return std::get<0>(
      game_authority::observeStep(std::move(state), milliseconds{at_ms}, activations, events, BOTH_ALIVE));
}

// Game in which A's order 1 executed from 400 ms and completed at 3000 ms, and successor 2, submitted at 2000 ms
// while 1 executed and transmitted at the first opportunity after, 4200 ms, activated at 4400 ms
State withSuccessorExecuting() {
  State state = game_authority::observeTransmission(runningGame(), transmittedByA(1, 0, 200));
  state = step(std::move(state), 400, {activated(1)}, {});
  state = game_authority::observeTransmission(std::move(state), transmittedByA(2, 2000, 4200));
  state = step(std::move(state), 3000, {}, {completed(1)});
  return step(std::move(state), 4400, {activated(2)}, {});
}

// Game in which 1 handed over to 2, which completed at 5000 ms: the chain has one handover
State withOneHandover() { return step(withSuccessorExecuting(), 5000, {}, {completed(2)}); }

}  // namespace

/// @section game_authority::startRound

TEST(OrbitalGameAuthority, StartsRunningWithZeroPointsAndFullDuration) {
  // When: a game starts
  game_authority::View view = game_authority::view(runningGame());

  // Then: both players have no points and two minutes remain
  EXPECT_EQ(view.players.at(PLAYER_A).banked, 0);
  EXPECT_EQ(view.players.at(PLAYER_B).banked, 0);
  EXPECT_EQ(view.remaining, std::chrono::minutes{2});
  EXPECT_TRUE(game_authority::isRunning(runningGame()));
}

/// @section game_authority::observeStep

/// @subsection scoring

TEST(OrbitalGameAuthority, ShowsBankedAndHeldPointsSeparately) {
  // When: a step with food during an order and food without an order is observed
  auto [state, concluded] = game_authority::observeStep(
      runningGame(),
      milliseconds{200},
      {},
      {FoodCollected{PLAYER_A, Point{1, 1}, OrderId{1}}, FoodCollected{PLAYER_B, Point{2, 2}, std::nullopt}},
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
  State state =
      step(runningGame(),
           200,
           {},
           {FoodCollected{PLAYER_A, Point{1, 1}, OrderId{1}}, FoodCollected{PLAYER_A, Point{2, 1}, OrderId{1}}});

  // When: F completes
  State banked = step(state, 400, {}, {completed(1)});

  // Then: the 20 points are banked and nothing is held any more
  game_authority::View view = game_authority::view(banked);
  EXPECT_EQ(view.players.at(PLAYER_A).banked, 20);
  EXPECT_EQ(view.players.at(PLAYER_A).held, 0);
}

TEST(OrbitalGameAuthority, DropsHeldPointsOfInterruptedOrder) {
  // Given: order F of A holds 10 points
  State state = step(runningGame(), 200, {}, {FoodCollected{PLAYER_A, Point{1, 1}, OrderId{1}}});

  // When: F is interrupted
  State interrupted = step(state, 400, {}, {OrderEnded{OrderId{1}, EndReason::INTERRUPTED, OrderId{2}}});

  // Then: the points are gone and the banked score is unchanged
  game_authority::View view = game_authority::view(interrupted);
  EXPECT_EQ(view.players.at(PLAYER_A).banked, 0);
  EXPECT_EQ(view.players.at(PLAYER_A).held, 0);
}

/// @brief Food collected during an order and its completion in the same step count in order
TEST(OrbitalGameAuthority, BanksFoodCollectedOnArrivalStep) {
  // When: a step with collection during order 1 followed by its completion is observed
  State state = step(runningGame(), 200, {}, {FoodCollected{PLAYER_A, Point{1, 1}, OrderId{1}}, completed(1)});

  // Then: the points are banked
  EXPECT_EQ(game_authority::view(state).players.at(PLAYER_A).banked, 10);
}

TEST(OrbitalGameAuthority, AwardsNoPointsForCompletionAlone) {
  // When: the completion of an order without held points is observed
  State state = step(runningGame(), 200, {}, {completed(1)});

  // Then: A has no points
  EXPECT_EQ(game_authority::view(state).players.at(PLAYER_A).banked, 0);
}

/// @brief A replacement's own points start fresh; the interrupted order's points do not move over
TEST(OrbitalGameAuthority, KeepsHeldPointsPerOrder) {
  // Given: order F of A holds 10 points
  State state = step(runningGame(), 200, {}, {FoodCollected{PLAYER_A, Point{1, 1}, OrderId{1}}});

  // When: G interrupts F and collects food
  State next = step(
      state,
      400,
      {},
      {OrderEnded{OrderId{1}, EndReason::INTERRUPTED, OrderId{2}}, FoodCollected{PLAYER_A, Point{2, 1}, OrderId{2}}});

  // Then: only G's 10 points are held
  game_authority::View view = game_authority::view(next);
  EXPECT_EQ(view.players.at(PLAYER_A).held, 10);
  EXPECT_EQ(view.players.at(PLAYER_A).banked, 0);
}

/// @brief A penalty counts at once and does not touch an executing order's held points
TEST(OrbitalGameAuthority, KeepsHeldPointsOnCollisionPenalty) {
  // Given: order F of B holds 10 points
  State state = step(runningGame(), 200, {}, {FoodCollected{PLAYER_B, Point{1, 1}, OrderId{1}}});

  // When: B is bitten
  State bitten = step(state, 400, {}, {Bitten{PLAYER_B, PLAYER_A}});

  // Then: B lost ten banked points and F still holds its 10 points
  game_authority::View view = game_authority::view(bitten);
  EXPECT_EQ(view.players.at(PLAYER_B).banked, -10);
  EXPECT_EQ(view.players.at(PLAYER_B).held, 10);
}

TEST(OrbitalGameAuthority, CountsDownRemainingTime) {
  // When: a step at 30 s is observed
  auto [state, concluded] = game_authority::observeStep(runningGame(), milliseconds{30000}, {}, {}, BOTH_ALIVE);

  // Then: 90 s remain
  EXPECT_EQ(game_authority::view(state).remaining, milliseconds{90000});
}

/// @subsection conclusion

TEST(OrbitalGameAuthority, KeepsRunningBeforeDurationHasElapsed) {
  // When: the step at 119800 ms is observed
  auto [state, concluded] = game_authority::observeStep(runningGame(), milliseconds{119800}, {}, {}, BOTH_ALIVE);

  // Then: the game still runs
  EXPECT_FALSE(concluded.has_value());
  EXPECT_TRUE(game_authority::isRunning(state));
}

TEST(OrbitalGameAuthority, ConcludesWhenDurationHasElapsed) {
  // When: the step at two minutes is observed
  auto [state, concluded] = game_authority::observeStep(runningGame(), std::chrono::minutes{2}, {}, {}, BOTH_ALIVE);

  // Then: the game concluded
  EXPECT_TRUE(concluded.has_value());
  EXPECT_FALSE(game_authority::isRunning(state));
}

TEST(OrbitalGameAuthority, ConcludesWhenNoSnakeIsAlive) {
  // When: a step after which no snake is alive is observed
  auto [state, concluded] =
      game_authority::observeStep(runningGame(), milliseconds{5000}, {}, {MutualBite{PLAYER_A, PLAYER_B}}, {});

  // Then: the game concluded early
  EXPECT_TRUE(concluded.has_value());
}

/// @brief The step due at the end of the game still counts, including a completion it brings
TEST(OrbitalGameAuthority, BanksCompletionOnFinalStepBeforeConclusion) {
  // Given: order F of A holds 20 points
  auto [state, first] = game_authority::observeStep(
      runningGame(),
      milliseconds{1000},
      {},
      {FoodCollected{PLAYER_A, Point{1, 1}, OrderId{1}}, FoodCollected{PLAYER_A, Point{2, 1}, OrderId{1}}},
      BOTH_ALIVE);

  // When: F completes on the step at two minutes
  auto [final_state, concluded] = game_authority::observeStep(
      state, std::chrono::minutes{2}, {}, {OrderEnded{OrderId{1}, EndReason::COMPLETED, std::nullopt}}, BOTH_ALIVE);

  // Then: A's 20 points count and A wins
  EXPECT_EQ(game_authority::view(final_state).players.at(PLAYER_A).banked, 20);
  EXPECT_EQ(concluded->result, (Result{PlayerId{PLAYER_A}}));
}

/// @brief Conclusion ends the round for an order still executing; the Scoring Policy drops its points then
TEST(OrbitalGameAuthority, SettlesExecutingOrderAsEndedByRoundAtConclusion) {
  // Given: B has 10 banked points and A's executing order holds 20 points
  auto [state, first] = game_authority::observeStep(runningGame(),
                                                    milliseconds{1000},
                                                    {},
                                                    {FoodCollected{PLAYER_B, Point{3, 3}, std::nullopt},
                                                     FoodCollected{PLAYER_A, Point{1, 1}, OrderId{1}},
                                                     FoodCollected{PLAYER_A, Point{2, 1}, OrderId{1}}},
                                                    BOTH_ALIVE);

  // When: the game concludes while A's order still executes
  auto [final_state, concluded] = game_authority::observeStep(state, std::chrono::minutes{2}, {}, {}, BOTH_ALIVE);

  // Then: A's held points are dropped and B wins
  EXPECT_EQ(game_authority::view(final_state).players.at(PLAYER_A).held, 0);
  EXPECT_EQ(concluded->result, (Result{PlayerId{PLAYER_B}}));
}

TEST(OrbitalGameAuthority, DrawsOnEqualBankedScores) {
  // When: the game concludes with equal banked scores
  auto [state, concluded] = game_authority::observeStep(runningGame(), std::chrono::minutes{2}, {}, {}, BOTH_ALIVE);

  // Then: the result is a draw
  EXPECT_EQ(concluded->result, (Result{std::nullopt}));
}

/// @brief The Arena Authority's ROUND_ENDED events after conclusion must not change the final scores
TEST(OrbitalGameAuthority, IgnoresStepsAfterConclusion) {
  // Given: a concluded game in which B leads
  auto [state, concluded] = game_authority::observeStep(
      runningGame(), std::chrono::minutes{2}, {}, {FoodCollected{PLAYER_B, Point{3, 3}, std::nullopt}}, BOTH_ALIVE);

  // When: further events are observed
  auto [later, again] = game_authority::observeStep(state,
                                                    std::chrono::minutes{2},
                                                    {},
                                                    {OrderEnded{OrderId{1}, EndReason::ROUND_ENDED, std::nullopt},
                                                     FoodCollected{PLAYER_A, Point{1, 1}, std::nullopt}},
                                                    BOTH_ALIVE);

  // Then: nothing changes and no second conclusion is announced
  EXPECT_FALSE(again.has_value());
  EXPECT_EQ(game_authority::view(later).players.at(PLAYER_A).banked, 0);
  EXPECT_EQ(game_authority::view(later).result, (Result{PlayerId{PLAYER_B}}));
}

/// @subsection handover chain
///
/// Opportunities come at 200 ms, 4200 ms, 8000 ms, 11800 ms and 15400 ms. A successor is sent at the first opportunity
/// at or after its predecessor's completion.

/// @brief The successor earns the bonus with its own completion, when it took over from a predecessor that completed
TEST(OrbitalGameAuthority, AwardsBaseBonusWhenSuccessorCompletes) {
  // Given: 1 completed at 3000 ms and successor 2, submitted while 1 executed, executes
  State state = withSuccessorExecuting();

  // When: 2 completes
  State observed = step(std::move(state), 5000, {}, {completed(2)});

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
  State observed = step(
      std::move(state), 4600, {}, {SelfBitten{PLAYER_A}, OrderEnded{OrderId{2}, EndReason::ELIMINATED, std::nullopt}});

  // Then: the bonus is dropped, only the bite penalty counts, and the chain is broken
  game_authority::PlayerScore score = game_authority::view(observed).players.at(PLAYER_A);
  EXPECT_EQ(score.held, 0);
  EXPECT_EQ(score.banked, -10);
  EXPECT_EQ(score.chain, 0);
}

/// @brief A takeover that is no handover breaks the chain at once, not only when that order ends
TEST(OrbitalGameAuthority, BreaksChainWhenOrderTakesOverWithoutHandover) {
  // Given: the chain has one handover; 3 was submitted at 8100 ms, after the opportunity at 8000 ms that followed 2's
  //        completion, and transmitted at 11800 ms
  State state = game_authority::observeTransmission(withOneHandover(), transmittedByA(3, 8100, 11800));

  // When: 3 activates
  State observed = step(std::move(state), 12000, {activated(3)}, {});

  // Then: the chain is broken while the earned bonus stays banked
  game_authority::PlayerScore score = game_authority::view(observed).players.at(PLAYER_A);
  EXPECT_EQ(score.chain, 0);
  EXPECT_EQ(score.banked, 10);
}

TEST(OrbitalGameAuthority, DoublesBonusForConsecutiveHandover) {
  // Given: 1 handed over to 2, which executed from 4400 ms to 5000 ms; successor 3 was submitted at 4600 ms,
  //        transmitted at 8000 ms and activated at 8200 ms
  State state = game_authority::observeTransmission(withSuccessorExecuting(), transmittedByA(3, 4600, 8000));
  state = step(std::move(state), 5000, {}, {completed(2)});
  state = step(std::move(state), 8200, {activated(3)}, {});

  // When: 3 completes
  State observed = step(std::move(state), 9000, {}, {completed(3)});

  // Then: the second handover earns twice the base bonus
  game_authority::PlayerScore score = game_authority::view(observed).players.at(PLAYER_A);
  EXPECT_EQ(score.banked, 10 + 20);
  EXPECT_EQ(score.chain, 2);
}

/// @brief The successor need not take over right away; driving straight in between is no reason to lose the handover
TEST(OrbitalGameAuthority, AwardsBonusAfterIdleGapBetweenPredecessorAndSuccessor) {
  // Given: 1 completed at 1000 ms; successor 2, submitted at 700 ms, was transmitted at 4200 ms and activated at
  //        4400 ms
  State state = game_authority::observeTransmission(runningGame(), transmittedByA(1, 0, 200));
  state = step(std::move(state), 400, {activated(1)}, {});
  state = step(std::move(state), 1000, {}, {completed(1)});
  state = game_authority::observeTransmission(std::move(state), transmittedByA(2, 700, 4200));
  state = step(std::move(state), 4400, {activated(2)}, {});

  // When: 2 completes
  State observed = step(std::move(state), 5000, {}, {completed(2)});

  // Then: the handover counts
  EXPECT_EQ(game_authority::view(observed).players.at(PLAYER_A).banked, 10);
}

/// @brief The window reaches past the predecessor's arrival up to the opportunity that sends the successor
TEST(OrbitalGameAuthority, AwardsBonusToSuccessorSubmittedAfterCompletionBeforeOpportunity) {
  // Given: 1 completed at 3000 ms; 2 was submitted at 3500 ms, transmitted at 4200 ms and activated at 4400 ms
  State state = game_authority::observeTransmission(runningGame(), transmittedByA(1, 0, 200));
  state = step(std::move(state), 400, {activated(1)}, {});
  state = step(std::move(state), 3000, {}, {completed(1)});
  state = game_authority::observeTransmission(std::move(state), transmittedByA(2, 3500, 4200));
  state = step(std::move(state), 4400, {activated(2)}, {});

  // When: 2 completes
  State observed = step(std::move(state), 5000, {}, {completed(2)});

  // Then: the handover counts
  EXPECT_EQ(game_authority::view(observed).players.at(PLAYER_A).banked, 10);
}

/// @brief Submitting too late: the first opportunity after the predecessor's completion has passed
TEST(OrbitalGameAuthority, DeniesHandoverToSuccessorSentAfterFirstOpportunityFromCompletion) {
  // Given: 1 completed at 3000 ms; 2 was submitted at 4300 ms, after the opportunity at 4200 ms, transmitted at
  //        8000 ms and activated at 8200 ms
  State state = game_authority::observeTransmission(runningGame(), transmittedByA(1, 0, 200));
  state = step(std::move(state), 400, {activated(1)}, {});
  state = step(std::move(state), 3000, {}, {completed(1)});
  state = game_authority::observeTransmission(std::move(state), transmittedByA(2, 4300, 8000));
  state = step(std::move(state), 8200, {activated(2)}, {});

  // When: 2 completes
  State observed = step(std::move(state), 9000, {}, {completed(2)});

  // Then: the completion is no handover
  game_authority::PlayerScore score = game_authority::view(observed).players.at(PLAYER_A);
  EXPECT_EQ(score.banked, 0);
  EXPECT_EQ(score.chain, 0);
}

/// @brief A transmission at the end of a step comes after that step's completions, so it sends the successor in time
TEST(OrbitalGameAuthority, AwardsBonusToSuccessorSentAtOpportunityOfCompletionStep) {
  // Given: 1 completed in the step at 4200 ms; 2, submitted at 3000 ms, was transmitted at the end of that step and
  //        activated at 4400 ms
  State state = game_authority::observeTransmission(runningGame(), transmittedByA(1, 0, 200));
  state = step(std::move(state), 400, {activated(1)}, {});
  state = step(std::move(state), 4200, {}, {completed(1)});
  state = game_authority::observeTransmission(std::move(state), transmittedByA(2, 3000, 4200));
  state = step(std::move(state), 4400, {activated(2)}, {});

  // When: 2 completes
  State observed = step(std::move(state), 5000, {}, {completed(2)});

  // Then: the handover counts
  EXPECT_EQ(game_authority::view(observed).players.at(PLAYER_A).banked, 10);
}

/// @brief A submission at the time of the activating step was made after that step, so the predecessor executed then
TEST(OrbitalGameAuthority, AwardsBonusToSuccessorSubmittedAtTimeOfPredecessorActivation) {
  // Given: 1 activated in the step at 400 ms and completed at 3000 ms; 2 was submitted at 400 ms, transmitted at
  //        4200 ms and activated at 4400 ms
  State state = game_authority::observeTransmission(runningGame(), transmittedByA(1, 0, 200));
  state = step(std::move(state), 400, {activated(1)}, {});
  state = game_authority::observeTransmission(std::move(state), transmittedByA(2, 400, 4200));
  state = step(std::move(state), 3000, {}, {completed(1)});
  state = step(std::move(state), 4400, {activated(2)}, {});

  // When: 2 completes
  State observed = step(std::move(state), 5000, {}, {completed(2)});

  // Then: the handover counts
  EXPECT_EQ(game_authority::view(observed).players.at(PLAYER_A).banked, 10);
}

/// @brief An order chosen after its predecessor was sent but before it activated follows it without interrupting;
/// still, it was not chosen while the predecessor executed
TEST(OrbitalGameAuthority, DeniesHandoverToSuccessorSubmittedBeforePredecessorActivated) {
  // Given: 1 was transmitted at 200 ms, activated at 400 ms and completed at 3000 ms; 2 was submitted at 300 ms,
  //        transmitted at 4200 ms and activated at 4400 ms
  State state = game_authority::observeTransmission(runningGame(), transmittedByA(1, 0, 200));
  state = game_authority::observeTransmission(std::move(state), transmittedByA(2, 300, 4200));
  state = step(std::move(state), 400, {activated(1)}, {});
  state = step(std::move(state), 3000, {}, {completed(1)});
  state = step(std::move(state), 4400, {activated(2)}, {});

  // When: 2 completes
  State observed = step(std::move(state), 5000, {}, {completed(2)});

  // Then: the completion is no handover
  EXPECT_EQ(game_authority::view(observed).players.at(PLAYER_A).banked, 0);
}

/// @brief Changing one's mind about the successor costs its handover, even though only the last choice was sent
TEST(OrbitalGameAuthority, DeniesHandoverToSuccessorThatReplacedSelection) {
  // Given: 1 completed at 3000 ms; successor 2 replaced selection 9 before it was transmitted, and executes
  State state = game_authority::observeTransmission(runningGame(), transmittedByA(1, 0, 200));
  state = step(std::move(state), 400, {activated(1)}, {});
  state = game_authority::observeTransmission(
      std::move(state),
      OrderTransmitted{ROUND, OrderId{2}, PLAYER_A, milliseconds{2000}, milliseconds{4200}, OrderId{9}});
  state = step(std::move(state), 3000, {}, {completed(1)});
  state = step(std::move(state), 4400, {activated(2)}, {});

  // When: 2 completes
  State observed = step(std::move(state), 5000, {}, {completed(2)});

  // Then: the completion is no handover
  game_authority::PlayerScore score = game_authority::view(observed).players.at(PLAYER_A);
  EXPECT_EQ(score.banked, 0);
  EXPECT_EQ(score.chain, 0);
}

/// @brief The chain follows what executed: a second order submitted during the same predecessor follows the first
/// successor, during which it was not submitted
TEST(OrbitalGameAuthority, DeniesHandoverToSecondOrderSubmittedDuringSamePredecessor) {
  // Given: 1 handed over to 2, which completed at 5000 ms; 3, submitted at 2500 ms while 1 executed, was transmitted
  //        at 8000 ms and activated at 8200 ms
  State state = game_authority::observeTransmission(withOneHandover(), transmittedByA(3, 2500, 8000));
  state = step(std::move(state), 8200, {activated(3)}, {});

  // When: 3 completes
  State observed = step(std::move(state), 9000, {}, {completed(3)});

  // Then: only 2's handover counts and the chain is broken
  game_authority::PlayerScore score = game_authority::view(observed).players.at(PLAYER_A);
  EXPECT_EQ(score.banked, 10);
  EXPECT_EQ(score.chain, 0);
}

/// @brief A successor that activates too early interrupts instead of taking over, and cannot hand over later
TEST(OrbitalGameAuthority, BreaksChainWhenSuccessorInterruptsExecutingOrder) {
  // Given: the chain has one handover; 3 executes from 8200 ms; 4, submitted at 8300 ms and transmitted at 11800 ms,
  //        interrupted 3 at 12000 ms
  State state = game_authority::observeTransmission(withOneHandover(), transmittedByA(3, 5100, 8000));
  state = step(std::move(state), 8200, {activated(3)}, {});
  state = game_authority::observeTransmission(std::move(state), transmittedByA(4, 8300, 11800));
  state = step(std::move(state),
               12000,
               {OrderActivated{OrderId{4}, OrderId{3}, {}}},
               {OrderEnded{OrderId{3}, EndReason::INTERRUPTED, OrderId{4}}});

  // When: 4 completes
  State observed = step(std::move(state), 12800, {}, {completed(4)});

  // Then: the chain is broken and 4 earned nothing
  game_authority::PlayerScore score = game_authority::view(observed).players.at(PLAYER_A);
  EXPECT_EQ(score.chain, 0);
  EXPECT_EQ(score.banked, 10);
}

TEST(OrbitalGameAuthority, BreaksChainOnCompletionWithoutHandover) {
  // Given: the chain has one handover; 3 was submitted at 8100 ms, after the opportunity at 8000 ms that followed 2's
  //        completion, transmitted at 11800 ms and executes
  State state = game_authority::observeTransmission(withOneHandover(), transmittedByA(3, 8100, 11800));
  state = step(std::move(state), 12000, {activated(3)}, {});

  // When: 3 completes
  State observed = step(std::move(state), 12600, {}, {completed(3)});

  // Then: no further bonus is banked and the chain is broken
  game_authority::PlayerScore score = game_authority::view(observed).players.at(PLAYER_A);
  EXPECT_EQ(score.banked, 10);
  EXPECT_EQ(score.chain, 0);
  EXPECT_EQ(score.next_handover, 10);
}

TEST(OrbitalGameAuthority, BreaksChainWhenExecutingOrderIsEliminated) {
  // Given: the chain has one handover and 3 executes
  State state = game_authority::observeTransmission(withOneHandover(), transmittedByA(3, 5100, 8000));
  state = step(std::move(state), 8200, {activated(3)}, {});

  // When: A's snake dies while executing 3
  State observed = step(
      std::move(state), 8400, {}, {SelfBitten{PLAYER_A}, OrderEnded{OrderId{3}, EndReason::ELIMINATED, std::nullopt}});

  // Then: the chain is broken
  EXPECT_EQ(game_authority::view(observed).players.at(PLAYER_A).chain, 0);
}

/// @brief A snake can die between orders, right after a handover; no order ending is reported then
TEST(OrbitalGameAuthority, BreaksChainWhenSnakeDiesWithoutExecutingOrder) {
  // Given: the chain has one handover and no order executes
  State state = withOneHandover();

  // When: A's snake dies in the next step
  auto [observed, concluded] =
      game_authority::observeStep(std::move(state), milliseconds{5200}, {}, {SelfBitten{PLAYER_A}}, {PLAYER_B});

  // Then: the chain is broken, while the earned bonus stays banked
  game_authority::PlayerScore score = game_authority::view(observed).players.at(PLAYER_A);
  EXPECT_EQ(score.chain, 0);
  EXPECT_EQ(score.banked, 10 - 10);
}

/// @brief A rejected order never executed: it neither breaks the chain nor becomes anyone's predecessor
TEST(OrbitalGameAuthority, PassesOverRejectedOrderInChain) {
  // Given: the chain has one handover; 3 executes from 8200 ms to 12600 ms; 4 was rejected at 12000 ms; 5, submitted at
  //        11900 ms while 3 executed, was transmitted at 15400 ms and activated at 15600 ms
  State state = game_authority::observeTransmission(withOneHandover(), transmittedByA(3, 4600, 8000));
  state = step(std::move(state), 8200, {activated(3)}, {});
  state = game_authority::observeTransmission(std::move(state), transmittedByA(4, 8300, 11800));
  state = step(std::move(state), 12000, {OrderRejected{OrderId{4}, RejectionReason::TARGET_OFF_BOARD}}, {});
  state = game_authority::observeTransmission(std::move(state), transmittedByA(5, 11900, 15400));
  state = step(std::move(state), 12600, {}, {completed(3)});
  state = step(std::move(state), 15600, {activated(5)}, {});

  // When: 5 completes
  State observed = step(std::move(state), 16000, {}, {completed(5)});

  // Then: 3 and 5 both handed over, so the chain has three handovers
  game_authority::PlayerScore score = game_authority::view(observed).players.at(PLAYER_A);
  EXPECT_EQ(score.banked, 10 + 20 + 40);
  EXPECT_EQ(score.chain, 3);
}

/// @brief The successor's held points are its own: its handover bonus comes in addition to them
TEST(OrbitalGameAuthority, BanksSuccessorPointsWithItsHandoverBonus) {
  // Given: successor 2 executes and collected food
  State state = step(withSuccessorExecuting(), 4600, {}, {FoodCollected{PLAYER_A, Point{1, 1}, OrderId{2}}});

  // When: 2 completes
  State observed = step(std::move(state), 5000, {}, {completed(2)});

  // Then: its food points and the handover bonus are banked
  EXPECT_EQ(game_authority::view(observed).players.at(PLAYER_A).banked, 10 + 10);
}

TEST(OrbitalGameAuthority, BreaksChainAtConclusion) {
  // Given: the chain has one handover
  State state = withOneHandover();

  // When: the game concludes
  auto [observed, concluded] =
      game_authority::observeStep(std::move(state), std::chrono::minutes{2}, {}, {}, BOTH_ALIVE);

  // Then: the earned bonus counts, and no chain is left
  game_authority::PlayerScore score = game_authority::view(observed).players.at(PLAYER_A);
  EXPECT_EQ(score.banked, 10);
  EXPECT_EQ(score.chain, 0);
}

/// @subsection open completion

/// @brief Presentation can tell that a successor chosen now would still hand over after the arrival
TEST(OrbitalGameAuthority, ViewsCompletionThatSuccessorCanStillFollow) {
  // Given: A's order 1 executes
  State state = game_authority::observeTransmission(runningGame(), transmittedByA(1, 0, 200));
  state = step(std::move(state), 400, {activated(1)}, {});

  // When: 1 completes at 3000 ms
  State observed = step(std::move(state), 3000, {}, {completed(1)});

  // Then: the view keeps that completion open for a successor
  EXPECT_EQ(game_authority::view(observed).players.at(PLAYER_A).open_completion, milliseconds{3000});
}

TEST(OrbitalGameAuthority, ClosesCompletionWhenNextOrderActivates) {
  // Given: 1 completed at 3000 ms and successor 2 was transmitted
  State state = game_authority::observeTransmission(runningGame(), transmittedByA(1, 0, 200));
  state = step(std::move(state), 400, {activated(1)}, {});
  state = step(std::move(state), 3000, {}, {completed(1)});
  state = game_authority::observeTransmission(std::move(state), transmittedByA(2, 2000, 4200));

  // When: 2 activates
  State observed = step(std::move(state), 4400, {activated(2)}, {});

  // Then: no completion is open any more
  EXPECT_FALSE(game_authority::view(observed).players.at(PLAYER_A).open_completion.has_value());
}

TEST(OrbitalGameAuthority, KeepsNoCompletionOpenAfterInterruption) {
  // Given: A's order 1 executes
  State state = game_authority::observeTransmission(runningGame(), transmittedByA(1, 0, 200));
  state = step(std::move(state), 400, {activated(1)}, {});

  // When: 1 is interrupted
  State observed = step(std::move(state), 4400, {}, {OrderEnded{OrderId{1}, EndReason::INTERRUPTED, OrderId{2}}});

  // Then: no completion is open
  EXPECT_FALSE(game_authority::view(observed).players.at(PLAYER_A).open_completion.has_value());
}

/// @section game_authority::observeTransmission

namespace {

// Steps in which 1 executed from 400 ms to 3000 ms and 2 from 4400 ms to 5000 ms, with only 1's transmission known
State withSuccessorTransmissionUnknown() {
  State state = game_authority::observeTransmission(runningGame(), transmittedByA(1, 0, 200));
  state = step(std::move(state), 400, {activated(1)}, {});
  state = step(std::move(state), 3000, {}, {completed(1)});
  state = step(std::move(state), 4400, {activated(2)}, {});
  return step(std::move(state), 5000, {}, {completed(2)});
}

}  // namespace

/// @brief A player-local Order Authority may synchronize its history after the Arena Authority reported the completion
TEST(OrbitalGameAuthority, BanksHandoverWhenSuccessorTransmissionIsLearnedLate) {
  // Given: 1 and 2 executed and completed before 2's transmission is known
  State state = withSuccessorTransmissionUnknown();

  // When: 2's transmission, submitted at 2000 ms and sent at 4200 ms, is learned
  State observed = game_authority::observeTransmission(std::move(state), transmittedByA(2, 2000, 4200));

  // Then: the handover bonus is banked as if the transmission had been known in time
  game_authority::PlayerScore score = game_authority::view(observed).players.at(PLAYER_A);
  EXPECT_EQ(score.banked, 10);
  EXPECT_EQ(score.chain, 1);
}

TEST(OrbitalGameAuthority, BanksLateTransmissionOnlyOnce) {
  // Given: 2's late transmission was learned and banked
  State state = game_authority::observeTransmission(withSuccessorTransmissionUnknown(), transmittedByA(2, 2000, 4200));

  // When: the same transmission is learned again
  State observed = game_authority::observeTransmission(std::move(state), transmittedByA(2, 2000, 4200));

  // Then: the bonus is not banked twice
  EXPECT_EQ(game_authority::view(observed).players.at(PLAYER_A).banked, 10);
}

TEST(OrbitalGameAuthority, IgnoresTransmissionOfAnotherRound) {
  // Given: 1 and 2 executed and completed before 2's transmission is known
  State state = withSuccessorTransmissionUnknown();

  // When: a transmission of another round is learned
  State observed = game_authority::observeTransmission(
      std::move(state),
      OrderTransmitted{RoundId{2}, OrderId{2}, PLAYER_A, milliseconds{2000}, milliseconds{4200}, std::nullopt});

  // Then: nothing is banked
  EXPECT_EQ(game_authority::view(observed).players.at(PLAYER_A).banked, 0);
}

/// @brief Final standings stay final, also for transmissions still on their way
TEST(OrbitalGameAuthority, IgnoresTransmissionLearnedAfterConclusion) {
  // Given: 1 and 2 executed, and 2 completed on the final step before its transmission is known
  State state = game_authority::observeTransmission(runningGame(), transmittedByA(1, 0, 200));
  state = step(std::move(state), 400, {activated(1)}, {});
  state = step(std::move(state), 3000, {}, {completed(1)});
  state = step(std::move(state), 4400, {activated(2)}, {});
  state = std::get<0>(
      game_authority::observeStep(std::move(state), std::chrono::minutes{2}, {}, {completed(2)}, BOTH_ALIVE));

  // When: 2's transmission is learned after conclusion
  State observed = game_authority::observeTransmission(std::move(state), transmittedByA(2, 2000, 4200));

  // Then: nothing is banked
  EXPECT_EQ(game_authority::view(observed).players.at(PLAYER_A).banked, 0);
}

/// @brief Synchronization may deliver the transmission of an order that executed in between last; the histories are
/// taken as they are delivered
TEST(OrbitalGameAuthority, WithdrawsHandoverBonusWhenLateTransmissionRevealsOrderInBetween) {
  // Given: 1 executed until 3000 ms, an order 3 not yet known executed from 3200 ms to 3800 ms, then 2, submitted at
  //        2000 ms and sent at 4200 ms, executed from 4400 ms to 5000 ms and was banked as handing over from 1
  State state = game_authority::observeTransmission(runningGame(), transmittedByA(1, 0, 200));
  state = step(std::move(state), 400, {activated(1)}, {});
  state = game_authority::observeTransmission(std::move(state), transmittedByA(2, 2000, 4200));
  state = step(std::move(state), 3000, {}, {completed(1)});
  state = step(std::move(state), 3200, {activated(3)}, {});
  state = step(std::move(state), 3800, {}, {completed(3)});
  state = step(std::move(state), 4400, {activated(2)}, {});
  state = step(std::move(state), 5000, {}, {completed(2)});

  // When: 3's transmission is learned, submitted at 2900 ms
  State observed = game_authority::observeTransmission(std::move(state), transmittedByA(3, 2900, 3000));

  // Then: 2's predecessor was 3, which was not executing when 2 was submitted, so the bonus banked before is withdrawn
  game_authority::PlayerScore score = game_authority::view(observed).players.at(PLAYER_A);
  EXPECT_EQ(score.banked, 0);
  EXPECT_EQ(score.chain, 0);
}

/// @brief Resending must not move a submission in time; its first record is the domain fact
TEST(OrbitalGameAuthority, KeepsFirstSubmissionTimeOfAnOrder) {
  // Given: 2's transmission with a submission at 300 ms, before 1 activated at 400 ms, was learned
  State state = game_authority::observeTransmission(withSuccessorTransmissionUnknown(), transmittedByA(2, 300, 4200));

  // When: a record for order 2 with a later submission time is learned
  State observed = game_authority::observeTransmission(std::move(state), transmittedByA(2, 2000, 4200));

  // Then: the first time counts, so the completion stays no handover
  EXPECT_EQ(game_authority::view(observed).players.at(PLAYER_A).banked, 0);
}

}  // namespace orbital
}  // namespace snake
