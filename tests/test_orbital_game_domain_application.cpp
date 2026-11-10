// Tests for the Orbital Game Domain Application: whole-game histories across the Order, Arena and Game Authorities.
//
// Most histories start in a small hand-picked world through the test setup seam, so they are reference-only (see
// TestAccess). Apart from that start, the tests use the Domain Application through its two interfaces only: they drive
// it at the boundary and through the execution interface, and observe what its players' side received.

#include <gtest/gtest.h>

#include <chrono>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "common/snake_model.hpp"
#include "orbital/orbital_game_domain_application.hpp"
#include "orbital/orbital_game_domain_application_test_access.hpp"
#include "orbital/order_release_policy.hpp"
#include "orbital_lanes.hpp"
#include "orbital_recording_players.hpp"
#include "test_printers.hpp"

namespace snake {
namespace orbital {

using std::chrono::milliseconds;

namespace {

namespace app = orbital_game_domain_application;
using arena_authority::RoundSetup;
using game_boundary::StartRejectionReason;
using game_boundary::SubmissionAccepted;
using game_boundary::SubmissionRejected;
using game_boundary::SubmissionRejectionReason;
using game_boundary::Views;
using order_authority::Status;

Status statusOf(const Views& views, OrderId id) {
  for (const order_authority::OrderRecord& record : views.orders.orders) {
    if (record.order.id == id) {
      return record.status;
    }
  }
  ADD_FAILURE() << "order " << id.value << " is not known";
  return Status::PENDING;
}

// The comparable content of the three views: heads, food, executing targets and step; order statuses; points and
// remaining time
auto viewedOutcome(const Views& views) {
  std::map<PlayerId, Point> heads;
  for (const auto& [player, snake] : views.arena.snakes) {
    heads[player] = snake_model::head(snake);
  }
  std::map<PlayerId, Point> targets;
  for (const auto& [player, active] : views.arena.active) {
    targets[player] = active.target;
  }
  std::vector<Status> statuses;
  for (const order_authority::OrderRecord& record : views.orders.orders) {
    statuses.push_back(record.status);
  }
  std::map<PlayerId, std::pair<int, int>> points;
  for (const auto& [player, score] : views.game.players) {
    points[player] = {score.banked, score.held};
  }
  return std::make_tuple(heads, views.arena.food, targets, views.arena.step, statuses, points, views.game.remaining);
}

/**
 * @brief Random source like the corner one that fails while the given switch is on
 */
RandomIntGeneratorFn makeSwitchableRandom(std::shared_ptr<bool> failing) {
  return [failing](int /* min */, int max) {
    if (*failing) {
      throw std::runtime_error{"no entropy"};
    }
    return max;
  };
}

/**
 * @brief A players' side that submits an order for A as soon as it learns of the first step
 */
struct SubmittingOnFirstStep : RecordingPlayers {
  void stepReported(const game_boundary::StepReport& step) override {
    RecordingPlayers::stepReported(step);
    if (!submitted) {
      submitted = true;
      game->submit(app::Submit{PLAYER_A, Point{3, 3}});
    }
  }

  app::DomainApplication* game{nullptr};
  bool submitted{false};
};

/**
 * @brief A players' side that, on the first step it learns of, submits an order for A and then fails
 */
struct FailingAfterSubmittingOnFirstStep : RecordingPlayers {
  void stepReported(const game_boundary::StepReport& step) override {
    RecordingPlayers::stepReported(step);
    if (!failed) {
      failed = true;
      game->submit(app::Submit{PLAYER_A, Point{3, 3}});
      throw std::runtime_error{"players' side failed"};
    }
  }

  app::DomainApplication* game{nullptr};
  bool failed{false};
};

/**
 * @brief One game in a Domain Application with fixed randomness, and the players' side that records what it reports
 */
class OrbitalGameDomainApplication : public ::testing::Test {
 protected:
  void start(const RoundSetup& setup) { app::TestAccess::startWithSetup(game_, setup); }

  /**
   * @brief Let the given game time pass, supplied through the execution interface
   */
  void elapse(int ms) { game_.elapse(app::TimeElapsed{milliseconds{ms}}); }

  /**
   * @brief Submit an order that is expected to be accepted
   *
   * @return Identity of the committed order
   */
  OrderId submit(const PlayerId& player, Point target) {
    game_.submit(app::Submit{player, target});
    return std::get<SubmissionAccepted>(players_.submissions.back()).order;
  }

  Status statusOf(OrderId id) const { return orbital::statusOf(*players_.views, id); }

  const game_authority::PlayerScore& scoreOf(const PlayerId& player) const {
    return players_.views->game.players.at(player);
  }

  const arena_authority::View& arena() const { return players_.views->arena; }

  RecordingPlayers players_;
  app::DomainApplication game_{players_, makeCornerRandom()};
};

}  // namespace

/// @section DomainApplication
///
/// Whole-game histories: release (Order Authority) → execution (Arena Authority) → scoring (Game Authority).
/// Timing: an order issued at 0 ms is released at 200 ms; steps run every 200 ms.

/// @subsection construction

/// @brief The execution interface promises that no game exists without a random source
TEST_F(OrbitalGameDomainApplication, RefusesEmptyRandomSource) {
  // When/Then: a Domain Application without a random number source is refused
  EXPECT_THROW((app::DomainApplication{players_, RandomIntGeneratorFn{}}), std::invalid_argument);
}

/// @subsection start

/// @brief A square board looks square in the terminal, where each cell is drawn two columns wide
TEST_F(OrbitalGameDomainApplication, StartsOnSquareBoardWithSnakesSideBySide) {
  // When: a game starts with the default setup
  game_.start(app::Start{RoundId{1}});

  // Then: the board is 28x28 and both snakes start with seven segments heading right, five rows apart
  EXPECT_EQ(arena().board.width, 28);
  EXPECT_EQ(arena().board.height, 28);
  EXPECT_EQ(snake_model::head(arena().snakes.at(PLAYER_A)), (Point{6, 11}));
  EXPECT_EQ(snake_model::head(arena().snakes.at(PLAYER_B)), (Point{6, 16}));
  EXPECT_EQ(snake_model::length(arena().snakes.at(PLAYER_A)), 7u);
  EXPECT_EQ(snake_model::length(arena().snakes.at(PLAYER_B)), 7u);
  EXPECT_EQ(snake_model::currentDirection(arena().snakes.at(PLAYER_A)), Direction::RIGHT);
  EXPECT_EQ(snake_model::currentDirection(arena().snakes.at(PLAYER_B)), Direction::RIGHT);
}

/// @brief One Domain Application plays one game; a start while it runs must not silently replace it
TEST_F(OrbitalGameDomainApplication, RejectsStartWhileGameIsRunning) {
  // Given: 1300 ms into a game, A's order is pending, and only what follows is observed
  start(makeLanes({}));
  elapse(1300);
  OrderId order = submit(PLAYER_A, Point{3, 3});
  players_.clear();

  // When: a game is started again
  game_.start(app::Start{RoundId{2}});

  // Then: the start is refused, and the views it reports show the running game unchanged
  ASSERT_EQ(players_.calls, (std::vector<std::string>{"startRejected", "viewsUpdated"}));
  EXPECT_EQ(players_.start_rejections.front().reason, StartRejectionReason::ALREADY_STARTED);
  EXPECT_EQ(players_.views->now, milliseconds{1300});
  EXPECT_EQ(statusOf(order), Status::PENDING);
}

/// @brief A new game needs a new Domain Application, also after the game concluded
TEST_F(OrbitalGameDomainApplication, RejectsStartAfterConclusion) {
  // Given: the game is over, and only what follows is observed
  start(makeLanes({}));
  elapse(120000);
  players_.clear();

  // When: a game is started again
  game_.start(app::Start{RoundId{2}});

  // Then: the start is refused, and the views it reports show the concluded game with its result
  ASSERT_EQ(players_.calls, (std::vector<std::string>{"startRejected", "viewsUpdated"}));
  EXPECT_EQ(players_.start_rejections.front().reason, StartRejectionReason::ALREADY_STARTED);
  EXPECT_TRUE(players_.views->game.result.has_value());
}

/// @subsection held points

/// @brief F's food points count once F completes; no successor was prepared, so no handover bonus
TEST_F(OrbitalGameDomainApplication, BanksPointsOfOrderThatCompletes) {
  // Given: F to (10,2) activates at 200 ms and collects food at (5,2) and (7,2)
  start(makeLanes({Point{5, 2}, Point{7, 2}}));
  OrderId f = submit(PLAYER_A, Point{10, 2});

  // When: time passes until F has arrived at 1600 ms
  elapse(1600);

  // Then: F completed and its 20 points count
  EXPECT_EQ(statusOf(f), Status::COMPLETED);
  EXPECT_EQ(scoreOf(PLAYER_A).banked, 20);
  EXPECT_EQ(scoreOf(PLAYER_A).held, 0);
  EXPECT_EQ(scoreOf(PLAYER_A).chain, 0);
}

/// @brief The defining history: F collects +20 while G is pending, and G activates first
TEST_F(OrbitalGameDomainApplication, LosesPointsOfOrderInterruptedByReplacement) {
  // Given: F to (10,2) collected food at (5,2); G is submitted at 800 ms (released at 1048 ms)
  start(makeLanes({Point{5, 2}, Point{7, 2}}));
  OrderId f = submit(PLAYER_A, Point{10, 2});
  elapse(800);
  OrderId g = submit(PLAYER_A, Point{10, 6});

  // When: time passes until G has activated at 1200 ms, after F collected the second food
  elapse(400);

  // Then: F was interrupted and its 20 points are lost, while the snake kept its growth
  EXPECT_EQ(statusOf(f), Status::INTERRUPTED);
  EXPECT_EQ(statusOf(g), Status::EXECUTING);
  EXPECT_EQ(scoreOf(PLAYER_A).banked, 0);
  EXPECT_EQ(scoreOf(PLAYER_A).held, 0);
  EXPECT_EQ(snake_model::length(arena().snakes.at(PLAYER_A)), 5U);
}

/// @brief Changing one's mind costs nothing until the replacement actually activates
TEST_F(OrbitalGameDomainApplication, KeepsHeldPointsWhenReplacementIsOnlySubmitted) {
  // Given: F to (10,2) collected food at (5,2) and (7,2)
  start(makeLanes({Point{5, 2}, Point{7, 2}}));
  OrderId f = submit(PLAYER_A, Point{10, 2});
  elapse(1000);

  // When: G is submitted
  OrderId g = submit(PLAYER_A, Point{10, 6});

  // Then: F still holds its 20 points and G waits
  EXPECT_EQ(scoreOf(PLAYER_A).held, 20);
  EXPECT_EQ(statusOf(f), Status::EXECUTING);
  EXPECT_EQ(statusOf(g), Status::PENDING);
}

/// @subsection handover chain
///
/// F to (10,2) activates at 200 ms and arrives at 1600 ms. A successor committed at 1400 ms is
/// released at 1684 ms and so still pending when F completes.

/// @brief The defining history of the refinement: G is committed before F completes and still pending then
TEST_F(OrbitalGameDomainApplication, AwardsHandoverBonusWhenSuccessorIsCommittedBeforeCompletion) {
  // Given: F collects food at (5,2) and (7,2); G is committed at 1400 ms
  start(makeLanes({Point{5, 2}, Point{7, 2}}));
  OrderId f = submit(PLAYER_A, Point{10, 2});
  elapse(1400);
  OrderId g = submit(PLAYER_A, Point{10, 6});

  // When: time passes until G has activated at 1800 ms
  elapse(400);

  // Then: F's 20 food points and the base handover bonus count; the chain has one handover
  EXPECT_EQ(statusOf(f), Status::COMPLETED);
  EXPECT_EQ(statusOf(g), Status::EXECUTING);
  EXPECT_EQ(scoreOf(PLAYER_A).banked, 20 + 10);
  EXPECT_EQ(scoreOf(PLAYER_A).chain, 1);
  EXPECT_EQ(scoreOf(PLAYER_A).next_handover, 20);
}

TEST_F(OrbitalGameDomainApplication, DoublesHandoverBonusForConsecutiveHandover) {
  // Given: F hands over to G, which activates at 1800 ms towards (10,6) and arrives at 2400 ms;
  //        H is committed at 2200 ms (released at 2532 ms)
  start(makeLanes({}));
  submit(PLAYER_A, Point{10, 2});
  elapse(1400);
  OrderId g = submit(PLAYER_A, Point{10, 6});
  elapse(800);
  OrderId h = submit(PLAYER_A, Point{3, 6});

  // When: time passes until G has completed
  elapse(200);

  // Then: both handovers count, the second worth twice the first
  EXPECT_EQ(statusOf(g), Status::COMPLETED);
  EXPECT_EQ(statusOf(h), Status::PENDING);
  EXPECT_EQ(scoreOf(PLAYER_A).banked, 10 + 20);
  EXPECT_EQ(scoreOf(PLAYER_A).chain, 2);
}

/// @brief Committing too late: F completes before its successor exists
TEST_F(OrbitalGameDomainApplication, BreaksChainWhenSuccessorIsCommittedAfterCompletion) {
  // Given: F hands over to G, which arrives at 2400 ms; H is committed only at 2400 ms, after that step
  start(makeLanes({}));
  submit(PLAYER_A, Point{10, 2});
  elapse(1400);
  OrderId g = submit(PLAYER_A, Point{10, 6});
  elapse(1000);
  OrderId h = submit(PLAYER_A, Point{3, 6});

  // When: time passes until H has activated
  elapse(600);

  // Then: only F's handover counts and the chain is broken
  EXPECT_EQ(statusOf(g), Status::COMPLETED);
  EXPECT_EQ(statusOf(h), Status::EXECUTING);
  EXPECT_EQ(scoreOf(PLAYER_A).banked, 10);
  EXPECT_EQ(scoreOf(PLAYER_A).chain, 0);
}

/// @brief Committing too early: the successor activates first and interrupts G
TEST_F(OrbitalGameDomainApplication, BreaksChainAndLosesPointsWhenSuccessorActivatesFirst) {
  // Given: F hands over to G, which collects food at (10,4) on its way to (10,6); H is committed at 1900 ms
  start(makeLanes({Point{10, 4}}));
  submit(PLAYER_A, Point{10, 2});
  elapse(1400);
  OrderId g = submit(PLAYER_A, Point{10, 6});
  elapse(500);
  OrderId h = submit(PLAYER_A, Point{3, 6});

  // When: time passes until H has activated at 2400 ms, the step at which G would have arrived
  elapse(500);

  // Then: G was interrupted, its food points are lost, F's handover still counts and the chain is broken
  EXPECT_EQ(statusOf(g), Status::INTERRUPTED);
  EXPECT_EQ(statusOf(h), Status::EXECUTING);
  EXPECT_EQ(scoreOf(PLAYER_A).banked, 10);
  EXPECT_EQ(scoreOf(PLAYER_A).held, 0);
  EXPECT_EQ(scoreOf(PLAYER_A).chain, 0);
}

/// @brief A handover needs one uninterrupted prepared successor; revising it costs the handover
TEST_F(OrbitalGameDomainApplication, DeniesHandoverButBanksPointsWhenPendingSuccessorIsRevised) {
  // Given: F collects food at (5,2) and (7,2); G is committed at 1200 ms and superseded by G' at 1400 ms
  start(makeLanes({Point{5, 2}, Point{7, 2}}));
  submit(PLAYER_A, Point{10, 2});
  elapse(1200);
  OrderId g = submit(PLAYER_A, Point{10, 8});
  elapse(200);
  OrderId revised = submit(PLAYER_A, Point{10, 6});

  // When: time passes until G' has activated
  elapse(400);

  // Then: G never activated, G' took over, F's food points count, and F's completion is no handover
  EXPECT_EQ(statusOf(g), Status::SUPERSEDED);
  EXPECT_EQ(statusOf(revised), Status::EXECUTING);
  EXPECT_EQ(scoreOf(PLAYER_A).banked, 20);
  EXPECT_EQ(scoreOf(PLAYER_A).chain, 0);
}

/// @brief A prepared successor that the Arena Authority refuses ends the chain it was prepared for
TEST_F(OrbitalGameDomainApplication, BreaksChainWhenSuccessorIsRejected) {
  // Given: G, with a target outside the board, is committed at 1400 ms
  start(makeLanes({}));
  submit(PLAYER_A, Point{10, 2});
  elapse(1400);
  OrderId g = submit(PLAYER_A, Point{25, 2});

  // When: time passes until G was evaluated at 1800 ms
  elapse(400);

  // Then: F's handover counts, but G was rejected and the chain is broken
  EXPECT_EQ(statusOf(g), Status::REJECTED);
  EXPECT_EQ(scoreOf(PLAYER_A).banked, 10);
  EXPECT_EQ(scoreOf(PLAYER_A).chain, 0);
}

/// @subsection pending orders

/// @brief An order whose release time falls exactly on a step activates on that step
TEST_F(OrbitalGameDomainApplication, ActivatesOrderOnStepAtItsReleaseTime) {
  // Given: G is committed at 0 ms, so it is released at 200 ms
  start(makeLanes({}));
  OrderId g = submit(PLAYER_A, Point{10, 6});

  // When: the step at 200 ms runs
  elapse(200);

  // Then: G executes
  EXPECT_EQ(statusOf(g), Status::EXECUTING);
}

/// @brief An order can activate only at the first step at or after its release time, not at a step before it
TEST_F(OrbitalGameDomainApplication, KeepsOrderPendingAtStepBeforeItsReleaseTime) {
  // Given: G is committed at 1 ms, so it is released at 201 ms
  start(makeLanes({}));
  elapse(1);
  OrderId g = submit(PLAYER_A, Point{10, 6});

  // When: the step at 200 ms runs
  elapse(199);

  // Then: G is still pending
  EXPECT_EQ(statusOf(g), Status::PENDING);
}

/// @brief A revision handled before the activation step prevents the old pending order from activating
TEST_F(OrbitalGameDomainApplication, SupersedesPendingOrderWithRevisionBeforeItsActivationStep) {
  // Given: G was submitted at 0 ms (released at 200 ms) and time is at 199 ms
  start(makeLanes({}));
  OrderId g = submit(PLAYER_A, Point{10, 6});
  elapse(199);

  // When: H is submitted and the step at 200 ms runs
  OrderId h = submit(PLAYER_A, Point{12, 2});
  elapse(1);

  // Then: G never activated and H waits for its own release time
  EXPECT_EQ(statusOf(g), Status::SUPERSEDED);
  EXPECT_EQ(statusOf(h), Status::PENDING);
  EXPECT_TRUE(arena().active.empty());
}

/// @brief A refused replacement neither stops the executing order nor costs its points
TEST_F(OrbitalGameDomainApplication, KeepsExecutingOrderAndItsPointsWhenReplacementIsRejected) {
  // Given: F to (10,2) collected food at (5,2), and G with a target outside the board was submitted
  start(makeLanes({Point{5, 2}}));
  OrderId f = submit(PLAYER_A, Point{10, 2});
  elapse(600);
  OrderId g = submit(PLAYER_A, Point{25, 2});

  // When: time passes until G was evaluated
  elapse(600);

  // Then: G is rejected, F still executes with its 10 held points
  EXPECT_EQ(statusOf(g), Status::REJECTED);
  EXPECT_EQ(statusOf(f), Status::EXECUTING);
  EXPECT_EQ(scoreOf(PLAYER_A).held, 10);
}

/// @subsection shared board

/// @brief A food race: the loser's positional order stays valid after the food is gone
TEST_F(OrbitalGameDomainApplication, CompletesPositionalOrderAfterOpponentTookTheFood) {
  // Given: A orders its snake to the food at (8,2); B heads down column 8 and takes the food at once
  RoundSetup setup = makeLanes({Point{8, 2}});
  setup.layout.snakes.at(PLAYER_B) = SnakeSetup{Point{8, 1}, Direction::DOWN, 2};
  start(setup);
  OrderId order = submit(PLAYER_A, Point{8, 2});

  // When: A's snake arrives at (8,2)
  elapse(1200);

  // Then: B, eating without an order, banked 5 for the food and A's order completed without points
  EXPECT_EQ(statusOf(order), Status::COMPLETED);
  EXPECT_EQ(scoreOf(PLAYER_B).banked, 5);
  EXPECT_EQ(scoreOf(PLAYER_A).banked, 0);
}

/// @subsection conclusion

/// @brief The step due at the end of the game counts, including the completion it brings
TEST_F(OrbitalGameDomainApplication, BanksCompletionOnFinalStepBeforeConclusion) {
  // Given: at 118400 ms A heads right in row 2; an order to (19,5) collects food at (19,3) and arrives at 120000 ms
  start(makeLanes({Point{19, 3}}));
  elapse(118400);
  OrderId order = submit(PLAYER_A, Point{19, 5});

  // When: the remaining time of the game passes
  elapse(1600);

  // Then: the order completed on the final step, its 10 points count and A wins
  EXPECT_EQ(statusOf(order), Status::COMPLETED);
  EXPECT_EQ(scoreOf(PLAYER_A).banked, 10);
  ASSERT_EQ(players_.conclusions.size(), 1U);
  EXPECT_EQ(players_.conclusions.front().result, (game_authority::Result{PlayerId{PLAYER_A}}));
  EXPECT_EQ(players_.steps.back().at, std::chrono::minutes{2});
}

TEST_F(OrbitalGameDomainApplication, ConcludesWithDrawWhenBothSnakesDie) {
  // Given: A and B head towards each other in row 5
  RoundSetup setup = makeLanes({});
  setup.layout.snakes.at(PLAYER_A) = SnakeSetup{Point{5, 5}, Direction::RIGHT, 2};
  setup.layout.snakes.at(PLAYER_B) = SnakeSetup{Point{7, 5}, Direction::LEFT, 2};
  start(setup);

  // When: their heads meet
  elapse(200);

  // Then: the game concluded at once as a draw
  ASSERT_EQ(players_.conclusions.size(), 1U);
  EXPECT_EQ(players_.conclusions.front().result, (game_authority::Result{std::nullopt}));
}

/// @brief Once the game is over, executing orders end with it and pending ones are cancelled
TEST_F(OrbitalGameDomainApplication, EndsExecutingAndCancelsPendingOrdersAtConclusion) {
  // Given: F executes since 118400 ms, and G submitted at 119800 ms (released at 120012 ms) is still
  //        pending at the end
  start(makeLanes({}));
  elapse(118000);
  OrderId f = submit(PLAYER_A, Point{12, 6});
  elapse(1800);
  OrderId g = submit(PLAYER_A, Point{3, 3});

  // When: the game ends
  elapse(200);

  // Then: F ended with the round and G was cancelled
  EXPECT_EQ(statusOf(f), Status::ENDED_BY_ROUND);
  EXPECT_EQ(statusOf(g), Status::CANCELLED);
  EXPECT_TRUE(arena().active.empty());
}

TEST_F(OrbitalGameDomainApplication, RejectsSubmissionBeforeStart) {
  // When: a player submits an order before a game has started
  game_.submit(app::Submit{PLAYER_A, Point{3, 3}});

  // Then: it is rejected because no game has started, and nothing is committed
  ASSERT_EQ(players_.submissions.size(), 1U);
  EXPECT_EQ(std::get<SubmissionRejected>(players_.submissions.front()).reason, SubmissionRejectionReason::NOT_STARTED);
  EXPECT_TRUE(players_.commitments.empty());
}

TEST_F(OrbitalGameDomainApplication, RejectsSubmissionAfterConclusion) {
  // Given: the game is over
  start(makeLanes({}));
  elapse(120000);

  // When: a player submits an order
  game_.submit(app::Submit{PLAYER_A, Point{3, 3}});

  // Then: it is rejected because the round is over
  ASSERT_EQ(players_.submissions.size(), 1U);
  EXPECT_EQ(std::get<SubmissionRejected>(players_.submissions.front()).reason, SubmissionRejectionReason::ROUND_OVER);
}

TEST_F(OrbitalGameDomainApplication, RunsNoStepsAfterConclusion) {
  // Given: the game is over, and only what follows is observed
  start(makeLanes({}));
  elapse(120000);
  players_.clear();

  // When: more time passes
  elapse(1000);

  // Then: no step runs
  EXPECT_TRUE(players_.steps.empty());
}

/// @brief Game time ends with the step that concludes the game, also when more time passes in the same advance
TEST_F(OrbitalGameDomainApplication, StopsGameTimeAtConcludingStepOfOneAdvance) {
  // Given: a game started
  start(makeLanes({}));

  // When: two minutes and one second pass at once
  elapse(121000);

  // Then: the game concluded on the step at two minutes, which was the last to run, and the clock stays there
  ASSERT_EQ(players_.conclusions.size(), 1U);
  EXPECT_EQ(players_.steps.size(), 600U);
  EXPECT_EQ(players_.steps.back().at, std::chrono::minutes{2});
  EXPECT_EQ(players_.views->now, milliseconds{120000});
}

/// @subsection time

/// @brief One time advance can run several steps; every step's report is kept
TEST_F(OrbitalGameDomainApplication, ReportsEveryStepOfOneTimeAdvance) {
  // Given: a game started
  start(makeLanes({}));

  // When: one second passes at once
  elapse(1000);

  // Then: five steps ran, at 200 ms intervals
  ASSERT_EQ(players_.steps.size(), 5U);
  EXPECT_EQ(players_.steps.front().at, milliseconds{200});
  EXPECT_EQ(players_.steps.back().at, milliseconds{1000});
}

/// @brief Before a game has started there is no game time that could pass
TEST_F(OrbitalGameDomainApplication, IgnoresTimeBeforeStart) {
  // When: one second passes before a game has started
  elapse(1000);

  // Then: no step runs and nothing is reported, not even views
  EXPECT_TRUE(players_.steps.empty());
  EXPECT_FALSE(players_.views.has_value());
}

/// @brief Game time cannot run backward, so a later submission is never committed earlier
TEST_F(OrbitalGameDomainApplication, RefusesNegativeElapsedTime) {
  // Given: a game started
  start(makeLanes({}));

  // When/Then: a negative elapsed time is refused
  EXPECT_THROW(elapse(-1), std::invalid_argument);
}

/// @brief Cutting the same game time into smaller advances does not change what happens
TEST_F(OrbitalGameDomainApplication, ReportsSameStepsAndViewsForOneAdvanceAndItsParts) {
  // Given: in two games, A's order to (7,2) is pending; it activates at 200 ms and collects the food at (6,2) on
  //        its way
  RecordingPlayers at_once_players;
  app::DomainApplication at_once{at_once_players, makeCornerRandom()};
  RecordingPlayers in_parts_players;
  app::DomainApplication in_parts{in_parts_players, makeCornerRandom()};
  for (app::DomainApplication* game : {&at_once, &in_parts}) {
    app::TestAccess::startWithSetup(*game, makeLanes({Point{6, 2}}));
    game->submit(app::Submit{PLAYER_A, Point{7, 2}});
  }

  // When: one second passes, at once in the first game and in five parts in the second
  at_once.elapse(app::TimeElapsed{milliseconds{1000}});
  for (int part = 0; part < 5; ++part) {
    in_parts.elapse(app::TimeElapsed{milliseconds{200}});
  }

  // Then: both report the same steps with the same activations and events, and lead to the same views
  const std::vector<game_boundary::StepReport>& at_once_steps = at_once_players.steps;
  const std::vector<game_boundary::StepReport>& in_parts_steps = in_parts_players.steps;
  ASSERT_EQ(at_once_steps.size(), 5U);
  ASSERT_EQ(at_once_steps[0].activations.size(), 1U);
  ASSERT_FALSE(at_once_steps[3].events.empty());
  ASSERT_EQ(in_parts_steps.size(), at_once_steps.size());
  for (size_t i = 0; i < at_once_steps.size(); ++i) {
    EXPECT_EQ(in_parts_steps[i].step, at_once_steps[i].step);
    EXPECT_EQ(in_parts_steps[i].at, at_once_steps[i].at);
    EXPECT_EQ(in_parts_steps[i].activations, at_once_steps[i].activations);
    EXPECT_EQ(in_parts_steps[i].events, at_once_steps[i].events);
  }
  EXPECT_EQ(viewedOutcome(*in_parts_players.views), viewedOutcome(*at_once_players.views));
}

/// @brief A submission's commitment time is the current time of the game clock
TEST_F(OrbitalGameDomainApplication, CommitsSubmissionAtCurrentGameTime) {
  // Given: 1300 ms have passed
  start(makeLanes({}));
  elapse(1300);

  // When: a player submits an order
  game_.submit(app::Submit{PLAYER_A, Point{3, 3}});

  // Then: it is committed at 1300 ms
  ASSERT_EQ(players_.submissions.size(), 1U);
  EXPECT_EQ(std::get<SubmissionAccepted>(players_.submissions.front()).committed_at, milliseconds{1300});
}

/// @subsection order of occurrence

/// @brief The players learn an outcome before the views that already reflect it
TEST_F(OrbitalGameDomainApplication, ReportsAcceptedSubmissionInOrderOfOccurrence) {
  // Given: a game started, and only what follows is observed
  start(makeLanes({}));
  players_.clear();

  // When: a player submits an order
  game_.submit(app::Submit{PLAYER_A, Point{3, 3}});

  // Then: the outcome comes first, then the commitment, then the views
  EXPECT_EQ(players_.calls, (std::vector<std::string>{"submissionOutcome", "orderCommitted", "viewsUpdated"}));
}

/// @brief The step that concludes the game is reported before the orders that end with it and the conclusion
TEST_F(OrbitalGameDomainApplication, ReportsConclusionInOrderOfOccurrence) {
  // Given: F executes since 118400 ms and is still executing when the game ends, and only what follows is observed
  start(makeLanes({}));
  elapse(118000);
  submit(PLAYER_A, Point{12, 6});
  elapse(1800);
  players_.clear();

  // When: the game ends
  elapse(200);

  // Then: the final step, F ending with the round, the conclusion and the views are reported in that order
  EXPECT_EQ(players_.calls,
            (std::vector<std::string>{"stepReported", "ordersEndedWithRound", "concluded", "viewsUpdated"}));
}

/// @brief A call from within a report is the next interaction, so the players never end on stale views
TEST_F(OrbitalGameDomainApplication, TakesCallDuringReportAsNextInteraction) {
  // Given: a game whose players' side submits an order as soon as it learns of the first step
  SubmittingOnFirstStep players;
  app::DomainApplication game{players, makeCornerRandom()};
  players.game = &game;
  app::TestAccess::startWithSetup(game, makeLanes({}));
  players.clear();

  // When: two steps' worth of time passes
  game.elapse(app::TimeElapsed{milliseconds{400}});

  // Then: the report of both steps completes first; the submission follows, committed at the time after them
  EXPECT_EQ(
      players.calls,
      (std::vector<std::string>{
          "stepReported", "stepReported", "viewsUpdated", "submissionOutcome", "orderCommitted", "viewsUpdated"}));
  ASSERT_EQ(players.commitments.size(), 1U);
  EXPECT_EQ(players.commitments.front().committed_at, milliseconds{400});
  EXPECT_EQ(players.views->pending_release_times.at(PLAYER_A), order_release_policy::releaseTime(milliseconds{400}));
}

/// @brief A failing players' side must not let a call it made during the failed report take effect later
TEST_F(OrbitalGameDomainApplication, DropsWaitingCallsWhenReportFails) {
  // Given: a game whose players' side submits an order on the first step and then fails
  FailingAfterSubmittingOnFirstStep players;
  app::DomainApplication game{players, makeCornerRandom()};
  players.game = &game;
  app::TestAccess::startWithSetup(game, makeLanes({}));
  players.clear();

  // When/Then: the failure of the players' side propagates
  EXPECT_THROW(game.elapse(app::TimeElapsed{milliseconds{400}}), std::runtime_error);

  // Then: the submission made during the failed report never took effect
  EXPECT_TRUE(players.submissions.empty());
  EXPECT_TRUE(players.commitments.empty());
}

/// @brief After a failed report the game goes on from where the reported input left it
TEST_F(OrbitalGameDomainApplication, KeepsTakingCallsAfterReportFailed) {
  // Given: two steps' worth of time passed, and the players' side failed while that was reported
  FailingAfterSubmittingOnFirstStep players;
  app::DomainApplication game{players, makeCornerRandom()};
  players.game = &game;
  app::TestAccess::startWithSetup(game, makeLanes({}));
  EXPECT_THROW(game.elapse(app::TimeElapsed{milliseconds{400}}), std::runtime_error);

  // When: a player submits an order
  game.submit(app::Submit{PLAYER_A, Point{5, 5}});

  // Then: it is committed at the time the two steps reached
  ASSERT_EQ(players.commitments.size(), 1U);
  EXPECT_EQ(players.commitments.front().committed_at, milliseconds{400});
}

/// @brief An advance that an owner cannot complete is reported not even in part
TEST_F(OrbitalGameDomainApplication, ReportsNothingOfAdvanceThatAnOwnerFailed) {
  // Given: at 200 ms, A heads right towards the food at (5,2), which it eats at 600 ms, and from now on the
  //        random source fails, so the food cannot be replaced
  auto failing = std::make_shared<bool>(false);
  RecordingPlayers players;
  app::DomainApplication game{players, makeSwitchableRandom(failing)};
  app::TestAccess::startWithSetup(game, makeLanes({Point{5, 2}}));
  game.elapse(app::TimeElapsed{milliseconds{200}});
  *failing = true;
  players.clear();

  // When/Then: the advance over the steps at 400 ms and 600 ms fails with the random source
  EXPECT_THROW(game.elapse(app::TimeElapsed{milliseconds{400}}), std::runtime_error);

  // Then: nothing of it is reported, not even the step at 400 ms that ran before the failure
  EXPECT_TRUE(players.calls.empty());
}

/// @brief An advance that an owner failed takes no effect, so the game resumes from before it
TEST_F(OrbitalGameDomainApplication, ResumesFromBeforeAdvanceThatAnOwnerFailed) {
  // Given: at 200 ms, an advance to 600 ms failed with the random source when A ate the food at (5,2), and the
  //        random source works again
  auto failing = std::make_shared<bool>(false);
  RecordingPlayers players;
  app::DomainApplication game{players, makeSwitchableRandom(failing)};
  app::TestAccess::startWithSetup(game, makeLanes({Point{5, 2}}));
  game.elapse(app::TimeElapsed{milliseconds{200}});
  *failing = true;
  EXPECT_THROW(game.elapse(app::TimeElapsed{milliseconds{400}}), std::runtime_error);
  *failing = false;
  players.clear();

  // When: the same time passes again
  game.elapse(app::TimeElapsed{milliseconds{400}});

  // Then: the steps at 400 ms and 600 ms run from the state at 200 ms, and A ate the food
  ASSERT_EQ(players.steps.size(), 2U);
  EXPECT_EQ(players.steps.front().at, milliseconds{400});
  EXPECT_EQ(players.steps.back().at, milliseconds{600});
  EXPECT_EQ(players.views->now, milliseconds{600});
  EXPECT_EQ(snake_model::length(players.views->arena.snakes.at(PLAYER_A)), 4U);
}

/// @subsection views

/// @brief Players see how long a submission sent now would travel, so they can time it
TEST_F(OrbitalGameDomainApplication, ViewsDelayOfSubmissionIssuedNow) {
  // Given: 14900 ms have passed
  start(makeLanes({}));
  elapse(14900);

  // When: 100 ms more pass, reaching a quarter of the orbital cycle
  elapse(100);

  // Then: the views carry the current time and the delay halfway between 200 ms and 2000 ms
  EXPECT_EQ(players_.views->now, milliseconds{15000});
  EXPECT_EQ(players_.views->submission_delay, milliseconds{1100});
}

/// @brief Presentation scales the delay gauge without asking the Order Release Policy itself
TEST_F(OrbitalGameDomainApplication, ViewsRangeOfOrderDelay) {
  // When: a game starts
  game_.start(app::Start{RoundId{1}});

  // Then: the views show the Order Delay's range of 0.2-2 s
  ASSERT_TRUE(players_.views.has_value());
  EXPECT_EQ(players_.views->min_delay, milliseconds{200});
  EXPECT_EQ(players_.views->max_delay, milliseconds{2000});
}

/// @brief Game time ends with the game, so the link condition shown at the end stays as it was
TEST_F(OrbitalGameDomainApplication, StopsGameTimeAndLinkDelayAtConclusion) {
  // Given: the game ended at 120000 ms
  start(makeLanes({}));
  elapse(120000);

  // When: five more seconds pass
  elapse(5000);

  // Then: the views still show the time and delay of the end of the game
  EXPECT_EQ(players_.views->now, milliseconds{120000});
  EXPECT_EQ(players_.views->submission_delay, order_release_policy::delayAt(milliseconds{120000}));
}

/// @brief Presentation shows when a pending order is released without asking the Order Release Policy itself
TEST_F(OrbitalGameDomainApplication, ViewsReleaseTimeOfPendingOrder) {
  // Given: a game started
  start(makeLanes({}));

  // When: A orders (8,5) at 0 ms
  game_.submit(app::Submit{PLAYER_A, Point{8, 5}});

  // Then: the views show its release time of 200 ms
  EXPECT_EQ(players_.views->pending_release_times.at(PLAYER_A), milliseconds{200});
}

/// @brief The preview of an order in transit follows the snake until the order activates
TEST_F(OrbitalGameDomainApplication, ViewsPendingRouteFromCurrentSnakePosition) {
  // Given: at 200 ms, with A's snake at (3,2) heading right, A ordered (8,5) (released at 412 ms)
  start(makeLanes({}));
  elapse(200);
  submit(PLAYER_A, Point{8, 5});

  // When: the step at 400 ms moved the snake to (4,2)
  elapse(200);

  // Then: the pending route starts from (4,2): four moves right, three down
  using D = Direction;
  EXPECT_EQ(players_.views->pending_routes.at(PLAYER_A),
            (std::vector<Direction>{D::RIGHT, D::RIGHT, D::RIGHT, D::RIGHT, D::DOWN, D::DOWN, D::DOWN}));
}

/// @section TestAccess::startWithSetup

/// @brief A start with a given setup is admitted like any start: only before a game has started
TEST_F(OrbitalGameDomainApplication, RejectsStartWithSetupOnceAGameStarted) {
  // Given: the game is over, and only what follows is observed
  start(makeLanes({}));
  elapse(120000);
  players_.clear();

  // When: a game with another setup is started
  app::TestAccess::startWithSetup(game_, RoundSetup{RoundId{2}, makeLanes({}).layout, {}});

  // Then: the start is refused, and the views it reports show the concluded game where it ended
  ASSERT_EQ(players_.calls, (std::vector<std::string>{"startRejected", "viewsUpdated"}));
  EXPECT_EQ(players_.start_rejections.front().reason, StartRejectionReason::ALREADY_STARTED);
  EXPECT_EQ(players_.views->now, milliseconds{120000});
}

}  // namespace orbital
}  // namespace snake
