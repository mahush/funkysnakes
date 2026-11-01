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

// Commitment of an order of player A
OrderCommitted committedByA(int order, int at_ms) {
  return OrderCommitted{ROUND, OrderId{order}, PLAYER_A, milliseconds{at_ms}, std::nullopt};
}

OrderEnded completed(int order) { return OrderEnded{OrderId{order}, EndReason::COMPLETED, std::nullopt}; }

OrderActivated activated(int order) { return OrderActivated{OrderId{order}, std::nullopt, {}}; }

// A step of a running game that concludes nothing
State step(State state, int at_ms, const std::vector<ActivationOutcome>& activations, const ArenaEvents& events) {
  return std::get<0>(
      game_authority::observeStep(std::move(state), milliseconds{at_ms}, activations, events, BOTH_ALIVE));
}

// Game in which A's order 1 executes, with successor 2 committed at 500 ms and still pending
State withPendingSuccessor() {
  State state = game_authority::observeCommitment(runningGame(), committedByA(1, 0));
  state = step(std::move(state), 200, {activated(1)}, {});
  return game_authority::observeCommitment(std::move(state), committedByA(2, 500));
}

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

/// @brief The Arena's ROUND_ENDED events after conclusion must not change the final scores
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

/// @brief The prepared successor is what turns a completion into a handover
TEST(OrbitalGameAuthority, AwardsBaseBonusForCompletionWithPendingSuccessor) {
  // Given: A's order 1 executes and successor 2 is committed and pending
  State state = withPendingSuccessor();

  // When: order 1 completes
  State observed = step(std::move(state), 1000, {}, {completed(1)});

  // Then: A banks the base handover bonus and the chain has one handover
  game_authority::PlayerScore score = game_authority::view(observed).players.at(PLAYER_A);
  EXPECT_EQ(score.banked, 10);
  EXPECT_EQ(score.chain, 1);
  EXPECT_EQ(score.next_handover, 20);
}

TEST(OrbitalGameAuthority, DoublesBonusForConsecutiveHandover) {
  // Given: order 1 handed over to 2, 2 activated, and successor 3 is committed and pending
  State state = step(withPendingSuccessor(), 1000, {}, {completed(1)});
  state = step(std::move(state), 1200, {activated(2)}, {});
  state = game_authority::observeCommitment(std::move(state), committedByA(3, 1500));

  // When: order 2 completes
  State observed = step(std::move(state), 2000, {}, {completed(2)});

  // Then: the second handover earns twice the base bonus
  game_authority::PlayerScore score = game_authority::view(observed).players.at(PLAYER_A);
  EXPECT_EQ(score.banked, 10 + 20);
  EXPECT_EQ(score.chain, 2);
}

TEST(OrbitalGameAuthority, BreaksChainOnCompletionWithoutPendingSuccessor) {
  // Given: order 1 handed over to 2, and 2 executes with no successor committed
  State state = step(withPendingSuccessor(), 1000, {}, {completed(1)});
  state = step(std::move(state), 1200, {activated(2)}, {});

  // When: order 2 completes
  State observed = step(std::move(state), 2000, {}, {completed(2)});

  // Then: no further bonus is banked and the chain is broken
  game_authority::PlayerScore score = game_authority::view(observed).players.at(PLAYER_A);
  EXPECT_EQ(score.banked, 10);
  EXPECT_EQ(score.chain, 0);
  EXPECT_EQ(score.next_handover, 10);
}

/// @brief A successor that activates too early interrupts instead of taking over
TEST(OrbitalGameAuthority, BreaksChainWhenSuccessorInterruptsExecutingOrder) {
  // Given: order 1 handed over to 2, and 2 executes with successor 3 committed
  State state = step(withPendingSuccessor(), 1000, {}, {completed(1)});
  state = step(std::move(state), 1200, {activated(2)}, {});
  state = game_authority::observeCommitment(std::move(state), committedByA(3, 1300));

  // When: 3 activates before 2 completes, interrupting it
  State observed = step(std::move(state),
                        1600,
                        {OrderActivated{OrderId{3}, OrderId{2}, {}}},
                        {OrderEnded{OrderId{2}, EndReason::INTERRUPTED, OrderId{3}}});

  // Then: the chain is broken
  EXPECT_EQ(game_authority::view(observed).players.at(PLAYER_A).chain, 0);
}

TEST(OrbitalGameAuthority, BreaksChainWhenExecutingOrderIsEliminated) {
  // Given: order 1 handed over to 2, and 2 executes with successor 3 committed
  State state = step(withPendingSuccessor(), 1000, {}, {completed(1)});
  state = step(std::move(state), 1200, {activated(2)}, {});
  state = game_authority::observeCommitment(std::move(state), committedByA(3, 1300));

  // When: A's snake dies while executing 2
  State observed = step(
      std::move(state), 1600, {}, {SelfBitten{PLAYER_A}, OrderEnded{OrderId{2}, EndReason::ELIMINATED, std::nullopt}});

  // Then: the chain is broken
  EXPECT_EQ(game_authority::view(observed).players.at(PLAYER_A).chain, 0);
}

/// @brief A snake can die between orders, right after a handover; no order ending is reported then
TEST(OrbitalGameAuthority, BreaksChainWhenSnakeDiesWithoutExecutingOrder) {
  // Given: order 1 handed over to 2, which has not activated yet
  State state = step(withPendingSuccessor(), 1000, {}, {completed(1)});

  // When: A's snake dies in the next step
  auto [observed, concluded] =
      game_authority::observeStep(std::move(state), milliseconds{1200}, {}, {SelfBitten{PLAYER_A}}, {PLAYER_B});

  // Then: the chain is broken, while the earned bonus stays banked
  game_authority::PlayerScore score = game_authority::view(observed).players.at(PLAYER_A);
  EXPECT_EQ(score.chain, 0);
  EXPECT_EQ(score.banked, 10 - 10);
}

/// @brief A successor that is refused does not continue the chain it was prepared for
TEST(OrbitalGameAuthority, BreaksChainWhenSuccessorIsRejected) {
  // Given: order 1 handed over to 2
  State state = step(withPendingSuccessor(), 1000, {}, {completed(1)});

  // When: the Arena rejects 2
  State observed = step(std::move(state), 1200, {OrderRejected{OrderId{2}, RejectionReason::TARGET_OFF_BOARD}}, {});

  // Then: the earned bonus stays banked, but the chain is broken
  game_authority::PlayerScore score = game_authority::view(observed).players.at(PLAYER_A);
  EXPECT_EQ(score.banked, 10);
  EXPECT_EQ(score.chain, 0);
}

/// @brief A late refusal of a superseded draft concerns no prepared successor, as for the Order Authority
TEST(OrbitalGameAuthority, KeepsChainWhenSupersededDraftIsRejected) {
  // Given: order 1 handed over to 2, 2 executes, and draft 3 was superseded by pending successor 4
  State state = step(withPendingSuccessor(), 1000, {}, {completed(1)});
  state = step(std::move(state), 1200, {activated(2)}, {});
  state = game_authority::observeCommitment(std::move(state), committedByA(3, 1300));
  state = game_authority::observeCommitment(
      std::move(state), OrderCommitted{ROUND, OrderId{4}, PLAYER_A, milliseconds{1400}, OrderId{3}});

  // When: the Arena reports a refusal of draft 3
  State observed = step(std::move(state), 1600, {OrderRejected{OrderId{3}, RejectionReason::ROUND_ENDED}}, {});

  // Then: the chain still has its handover
  EXPECT_EQ(game_authority::view(observed).players.at(PLAYER_A).chain, 1);
}

/// @brief A handover needs one uninterrupted prepared successor; a later successor cannot restore it
TEST(OrbitalGameAuthority, DeniesHandoverWhenPendingSuccessorIsRevised) {
  // Given: successor 2 was superseded by 3, which is pending
  State state = game_authority::observeCommitment(
      withPendingSuccessor(), OrderCommitted{ROUND, OrderId{3}, PLAYER_A, milliseconds{700}, OrderId{2}});

  // When: order 1 completes
  State observed = step(std::move(state), 1000, {}, {completed(1)});

  // Then: the completion is no handover
  game_authority::PlayerScore score = game_authority::view(observed).players.at(PLAYER_A);
  EXPECT_EQ(score.banked, 0);
  EXPECT_EQ(score.chain, 0);
}

/// @brief Revising the successor costs the handover only, not the points of the order that completes
TEST(OrbitalGameAuthority, BanksHeldPointsWhenPendingSuccessorIsRevised) {
  // Given: order 1 collected food, and successor 2 was superseded by 3
  State state = step(withPendingSuccessor(), 600, {}, {FoodCollected{PLAYER_A, Point{1, 1}, OrderId{1}}});
  state = game_authority::observeCommitment(std::move(state),
                                            OrderCommitted{ROUND, OrderId{3}, PLAYER_A, milliseconds{700}, OrderId{2}});

  // When: order 1 completes
  State observed = step(std::move(state), 1000, {}, {completed(1)});

  // Then: the food points are banked without a handover bonus
  EXPECT_EQ(game_authority::view(observed).players.at(PLAYER_A).banked, 10);
}

/// @brief A step runs before a commitment made at the same time on the game clock
TEST(OrbitalGameAuthority, IgnoresSuccessorCommittedAtTimeOfCompletion) {
  // Given: A's order 1 executes and successor 2 is committed at 1000 ms
  State state = game_authority::observeCommitment(runningGame(), committedByA(1, 0));
  state = step(std::move(state), 200, {activated(1)}, {});
  state = game_authority::observeCommitment(std::move(state), committedByA(2, 1000));

  // When: order 1 completes in the step at 1000 ms
  State observed = step(std::move(state), 1000, {}, {completed(1)});

  // Then: the completion is no handover
  EXPECT_EQ(game_authority::view(observed).players.at(PLAYER_A).banked, 0);
}

TEST(OrbitalGameAuthority, BreaksChainAtConclusion) {
  // Given: order 1 handed over to 2
  State state = step(withPendingSuccessor(), 1000, {}, {completed(1)});

  // When: the game concludes
  auto [observed, concluded] =
      game_authority::observeStep(std::move(state), std::chrono::minutes{2}, {}, {}, BOTH_ALIVE);

  // Then: the earned bonus counts, and no chain is left
  game_authority::PlayerScore score = game_authority::view(observed).players.at(PLAYER_A);
  EXPECT_EQ(score.banked, 10);
  EXPECT_EQ(score.chain, 0);
}

/// @section game_authority::observeCommitment

/// @brief A player-local Order Authority may synchronize its history after the Arena reported the completion
TEST(OrbitalGameAuthority, BanksHandoverWhenSuccessorCommitmentIsLearnedLate) {
  // Given: order 1 activated and completed, before any commitment of a successor is known
  State state = game_authority::observeCommitment(runningGame(), committedByA(1, 0));
  state = step(std::move(state), 200, {activated(1)}, {});
  state = step(std::move(state), 1000, {}, {completed(1)});

  // When: the commitment of successor 2 at 500 ms is learned
  State observed = game_authority::observeCommitment(std::move(state), committedByA(2, 500));

  // Then: the handover bonus is banked as if the commitment had been known in time
  game_authority::PlayerScore score = game_authority::view(observed).players.at(PLAYER_A);
  EXPECT_EQ(score.banked, 10);
  EXPECT_EQ(score.chain, 1);
}

TEST(OrbitalGameAuthority, BanksLateCommitmentOnlyOnce) {
  // Given: the late commitment of successor 2 was learned and banked
  State state = game_authority::observeCommitment(runningGame(), committedByA(1, 0));
  state = step(std::move(state), 200, {activated(1)}, {});
  state = step(std::move(state), 1000, {}, {completed(1)});
  state = game_authority::observeCommitment(std::move(state), committedByA(2, 500));

  // When: the same commitment is learned again
  State observed = game_authority::observeCommitment(std::move(state), committedByA(2, 500));

  // Then: the bonus is not banked twice
  EXPECT_EQ(game_authority::view(observed).players.at(PLAYER_A).banked, 10);
}

TEST(OrbitalGameAuthority, IgnoresCommitmentOfAnotherRound) {
  // Given: order 1 completed without a known successor
  State state = game_authority::observeCommitment(runningGame(), committedByA(1, 0));
  state = step(std::move(state), 200, {activated(1)}, {});
  state = step(std::move(state), 1000, {}, {completed(1)});

  // When: a commitment of another round is learned
  State observed = game_authority::observeCommitment(
      std::move(state), OrderCommitted{RoundId{2}, OrderId{2}, PLAYER_A, milliseconds{500}, std::nullopt});

  // Then: nothing is banked
  EXPECT_EQ(game_authority::view(observed).players.at(PLAYER_A).banked, 0);
}

/// @brief Final standings stay final, also for commitments still in transit
TEST(OrbitalGameAuthority, IgnoresCommitmentLearnedAfterConclusion) {
  // Given: order 1 completed on the final step without a known successor
  State state = game_authority::observeCommitment(runningGame(), committedByA(1, 0));
  state = step(std::move(state), 200, {activated(1)}, {});
  state = std::get<0>(
      game_authority::observeStep(std::move(state), std::chrono::minutes{2}, {}, {completed(1)}, BOTH_ALIVE));

  // When: the commitment of a successor is learned after conclusion
  State observed = game_authority::observeCommitment(std::move(state), committedByA(2, 500));

  // Then: nothing is banked
  EXPECT_EQ(game_authority::view(observed).players.at(PLAYER_A).banked, 0);
}

/// @brief Synchronization may deliver an older commitment first; the later one shows it was revised
TEST(OrbitalGameAuthority, WithdrawsHandoverBonusWhenLateCommitmentShowsCountedSuccessorWasRevised) {
  // Given: order 1 completed while successor 2 was the only known commitment, banked as a handover
  State state = step(withPendingSuccessor(), 1000, {}, {completed(1)});

  // When: a commitment at 700 ms is learned that superseded 2 before the completion
  State observed = game_authority::observeCommitment(
      std::move(state), OrderCommitted{ROUND, OrderId{3}, PLAYER_A, milliseconds{700}, OrderId{2}});

  // Then: the prepared successor was revised, so the bonus banked before is withdrawn
  game_authority::PlayerScore score = game_authority::view(observed).players.at(PLAYER_A);
  EXPECT_EQ(score.banked, 0);
  EXPECT_EQ(score.chain, 0);
}

/// @brief Resending must not move a commitment in time; its first record is the domain fact
TEST(OrbitalGameAuthority, KeepsFirstCommitmentTimeOfAnOrder) {
  // Given: successor 2's commitment at 1100 ms was learned, after the completion at 1000 ms
  State state = game_authority::observeCommitment(runningGame(), committedByA(1, 0));
  state = step(std::move(state), 200, {activated(1)}, {});
  state = game_authority::observeCommitment(std::move(state), committedByA(2, 1100));
  state = step(std::move(state), 1000, {}, {completed(1)});

  // When: a record for order 2 with an earlier time is learned
  State observed = game_authority::observeCommitment(std::move(state), committedByA(2, 500));

  // Then: the first time counts, so the completion stays no handover
  EXPECT_EQ(game_authority::view(observed).players.at(PLAYER_A).banked, 0);
}

}  // namespace orbital
}  // namespace snake
