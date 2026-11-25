// Tests for the Orbital Game Domain Application: whole-game histories across the Order Selection, Arena and Game
// Authorities.
//
// Most histories start in a small hand-picked world through the test setup seam, so they are reference-only (see
// TestAccess). Apart from that start, the tests use the Domain Application through its two interfaces only: they drive
// it at the boundary and through the execution interface, and observe what its players' side received.

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

#include "domain/common/snake_model.hpp"
#include "domain/orbital/orbital_game_domain_application.hpp"
#include "domain/orbital/orbital_game_domain_application_test_access.hpp"
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

// The comparable content of the views: heads, food, executing targets and step; selections and transmitted orders;
// points and remaining steps
auto viewedOutcome(const Views& views) {
  std::map<PlayerId, Point> heads;
  for (const auto& [player, snake] : views.arena.snakes) {
    heads[player] = snake_model::head(snake);
  }
  std::map<PlayerId, Point> targets;
  for (const auto& [player, active] : views.arena.active) {
    targets[player] = active.target;
  }
  std::map<PlayerId, std::pair<int, int>> points;
  for (const auto& [player, score] : views.game.players) {
    points[player] = {score.banked, score.held};
  }
  return std::make_tuple(heads,
                         views.arena.food,
                         targets,
                         views.arena.step,
                         views.selections.selections,
                         views.transmitted,
                         points,
                         views.game.remaining_steps);
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
    if (!has_submitted) {
      has_submitted = true;
      game->submit(app::Submit{PLAYER_A, Point{3, 3}});
    }
  }

  app::DomainApplication* game{nullptr};
  bool has_submitted{false};
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
   * @return Number of the selected order
   */
  OrderNumber submit(const PlayerId& player, Point target) {
    game_.submit(app::Submit{player, target});
    return std::get<SubmissionAccepted>(players_.submissions.back()).order;
  }

  /**
   * @brief The player's selected order, as the views show it
   */
  std::optional<OrderNumber> selectedOrderOf(const PlayerId& player) const {
    const order_selection_authority::View& selections = players_.views->selections;
    if (auto selection = selections.selections.find(player); selection != selections.selections.end()) {
      return selection->second.order.id;
    }
    return std::nullopt;
  }

  /**
   * @brief The selection an accepted submission replaced, as its OrderSubmitted names it
   */
  std::optional<OrderNumber> replacedBy(const PlayerId& player, OrderNumber id) const {
    for (const OrderSubmitted& submitted : players_.submitted) {
      if (submitted.player == player && submitted.order == id) {
        return submitted.replaced;
      }
    }
    ADD_FAILURE() << "order " << player << " #" << id.value << " was not submitted";
    return std::nullopt;
  }

  /**
   * @brief Whether a step reported the order as transmitted
   */
  bool isTransmitted(const PlayerId& player, OrderNumber id) const {
    return std::any_of(players_.steps.begin(), players_.steps.end(), [&](const game_boundary::StepReport& step) {
      return std::any_of(step.transmissions.begin(), step.transmissions.end(), [&](const OrderTransmitted& sent) {
        return sent.player == player && sent.order == id;
      });
    });
  }

  /**
   * @brief The Arena Authority's activation outcome for the order, if a step reported one
   */
  std::optional<ActivationOutcome> activationOf(const PlayerId& player, OrderNumber id) const {
    for (const game_boundary::StepReport& step : players_.steps) {
      for (const ActivationOutcome& outcome : step.activations) {
        if (std::visit([&](const auto& item) { return item.player == player && item.order == id; }, outcome)) {
          return outcome;
        }
      }
    }
    return std::nullopt;
  }

  /**
   * @brief How the order ended, if a step or the end of the round reported it
   */
  std::optional<EndReason> endingOf(const PlayerId& player, OrderNumber id) const {
    std::vector<ArenaEvents> reports = players_.ended_with_round;
    for (const game_boundary::StepReport& step : players_.steps) {
      reports.push_back(step.events);
    }
    for (const ArenaEvents& events : reports) {
      for (const ArenaEvent& event : events) {
        const auto* ended = std::get_if<OrderEnded>(&event);
        if (ended && ended->player == player && ended->order == id) {
          return ended->reason;
        }
      }
    }
    return std::nullopt;
  }

  /**
   * @brief The order the player's snake executes, as the Arena view shows it
   */
  std::optional<OrderNumber> executingOrderOf(const PlayerId& player) const {
    if (auto active = arena().active.find(player); active != arena().active.end()) {
      return active->second.id;
    }
    return std::nullopt;
  }

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
/// Whole-game histories: selection and transmission (Order Selection Authority) → execution (Arena Authority) → scoring
/// (Game Authority). Timing: steps run every 200 ms; the first transmission opportunities come at 200 ms, 4200 ms and
/// 8000 ms, so an order submitted at 0 ms is transmitted at 200 ms and activates at 400 ms, and one submitted later
/// in the first 4 s activates at 4400 ms.

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
  // Given: 1300 ms into a game, A's order is selected, and only what follows is observed
  start(makeLanes({}));
  elapse(1300);
  OrderNumber order = submit(PLAYER_A, Point{3, 3});
  players_.clear();

  // When: a game is started again
  game_.start(app::Start{RoundId{2}});

  // Then: the start is refused, and the views it reports show the running game unchanged
  ASSERT_EQ(players_.calls, (std::vector<std::string>{"startRejected", "viewsUpdated"}));
  EXPECT_EQ(players_.start_rejections.front().reason, StartRejectionReason::ALREADY_STARTED);
  EXPECT_EQ(players_.views->arena.step, StepId{6});
  EXPECT_EQ(selectedOrderOf(PLAYER_A), order);
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
  // Given: F to (10,2) activates at 400 ms and collects food at (5,2) and (7,2)
  start(makeLanes({Point{5, 2}, Point{7, 2}}));
  OrderNumber f = submit(PLAYER_A, Point{10, 2});

  // When: time passes until F has arrived at 1600 ms
  elapse(1600);

  // Then: F completed and its 20 points count
  EXPECT_EQ(endingOf(PLAYER_A, f), EndReason::COMPLETED);
  EXPECT_EQ(scoreOf(PLAYER_A).banked, 20);
  EXPECT_EQ(scoreOf(PLAYER_A).held, 0);
  EXPECT_EQ(scoreOf(PLAYER_A).chain, 0);
}

/// @brief The defining history: F collects +20 while G is on its way, and G activates first
TEST_F(OrbitalGameDomainApplication, LosesPointsOfOrderInterruptedByReplacement) {
  // Given: F to (2,5), around the wrapping row 2, collected food at (5,2) and (7,2); G is submitted at 1000 ms
  //        (transmitted at 4200 ms)
  start(makeLanes({Point{5, 2}, Point{7, 2}}));
  OrderNumber f = submit(PLAYER_A, Point{2, 5});
  elapse(1000);
  OrderNumber g = submit(PLAYER_A, Point{10, 6});

  // When: time passes until G has activated at 4400 ms, before F could arrive at 4600 ms
  elapse(3400);

  // Then: F was interrupted and its 20 points are lost, while the snake kept its growth
  EXPECT_EQ(endingOf(PLAYER_A, f), EndReason::INTERRUPTED);
  EXPECT_EQ(executingOrderOf(PLAYER_A), g);
  EXPECT_EQ(scoreOf(PLAYER_A).banked, 0);
  EXPECT_EQ(scoreOf(PLAYER_A).held, 0);
  EXPECT_EQ(snake_model::length(arena().snakes.at(PLAYER_A)), 5U);
}

/// @brief Changing one's mind costs nothing until the replacement actually activates
TEST_F(OrbitalGameDomainApplication, KeepsHeldPointsWhenReplacementIsOnlySubmitted) {
  // Given: F to (10,2) collected food at (5,2) and (7,2)
  start(makeLanes({Point{5, 2}, Point{7, 2}}));
  OrderNumber f = submit(PLAYER_A, Point{10, 2});
  elapse(1000);

  // When: G is submitted
  OrderNumber g = submit(PLAYER_A, Point{10, 6});

  // Then: F still holds its 20 points and G waits for a transmission
  EXPECT_EQ(scoreOf(PLAYER_A).held, 20);
  EXPECT_EQ(executingOrderOf(PLAYER_A), f);
  EXPECT_EQ(selectedOrderOf(PLAYER_A), g);
}

/// @subsection handover chain
///
/// F to (19,2) activates at 400 ms and arrives at 3400 ms. A successor submitted at 3000 ms is transmitted at the
/// opportunity at 4200 ms, after F's completion, and activates at 4400 ms with the snake at (3,2); towards (4,6) it
/// arrives at 5200 ms.

/// @brief The defining history: G is submitted while F executes and takes over after F completed
TEST_F(OrbitalGameDomainApplication, AwardsHandoverBonusWhenSuccessorSubmittedBeforeCompletionCompletes) {
  // Given: F collects food at (5,2) and (7,2); G is submitted at 3000 ms
  start(makeLanes({Point{5, 2}, Point{7, 2}}));
  OrderNumber f = submit(PLAYER_A, Point{19, 2});
  elapse(3000);
  OrderNumber g = submit(PLAYER_A, Point{4, 6});

  // When: time passes until G has completed at 5200 ms
  elapse(2200);

  // Then: F's 20 food points and the base handover bonus count; the chain has one handover
  EXPECT_EQ(endingOf(PLAYER_A, f), EndReason::COMPLETED);
  EXPECT_EQ(endingOf(PLAYER_A, g), EndReason::COMPLETED);
  EXPECT_EQ(scoreOf(PLAYER_A).banked, 20 + 10);
  EXPECT_EQ(scoreOf(PLAYER_A).chain, 1);
  EXPECT_EQ(scoreOf(PLAYER_A).next_handover, 20);
}

/// @brief Front ends learn of a handover from the step that counted it instead of inferring it from the views
TEST_F(OrbitalGameDomainApplication, ReportsHandoverWithStepOfSuccessorTakeover) {
  // Given: F executes; G is submitted at 3000 ms
  start(makeLanes({Point{5, 2}, Point{7, 2}}));
  submit(PLAYER_A, Point{19, 2});
  elapse(3000);
  OrderNumber g = submit(PLAYER_A, Point{4, 6});

  // When: time passes until G took over in step 22, at 4400 ms
  elapse(1400);

  // Then: that step reports the first handover of A's chain
  const game_boundary::StepReport& takeover = players_.steps.back();
  EXPECT_EQ(takeover.step, StepId{22});
  EXPECT_EQ(takeover.handovers, (std::vector<game_authority::HandoverGranted>{{PLAYER_A, g, 1, 10}}));
}

/// @brief The bonus is the successor's: decided when it takes over, held until it reaches its target
TEST_F(OrbitalGameDomainApplication, HoldsHandoverBonusWhileSuccessorExecutes) {
  // Given: G is submitted at 3000 ms while F executes
  start(makeLanes({}));
  submit(PLAYER_A, Point{19, 2});
  elapse(3000);
  OrderNumber g = submit(PLAYER_A, Point{4, 6});

  // When: time passes until G has activated at 4400 ms
  elapse(1400);

  // Then: G executes, the chain has one handover and its bonus is held, not yet banked
  EXPECT_EQ(executingOrderOf(PLAYER_A), g);
  EXPECT_EQ(scoreOf(PLAYER_A).chain, 1);
  EXPECT_EQ(scoreOf(PLAYER_A).held, 10);
  EXPECT_EQ(scoreOf(PLAYER_A).banked, 0);
}

TEST_F(OrbitalGameDomainApplication, DoublesHandoverBonusForConsecutiveHandover) {
  // Given: F hands over to G, which executes from 4400 ms to 5200 ms; H to (4,5) is submitted at 4600 ms,
  //        transmitted at the opportunity at 8000 ms and activates at 8200 ms
  start(makeLanes({}));
  submit(PLAYER_A, Point{19, 2});
  elapse(3000);
  submit(PLAYER_A, Point{4, 6});
  elapse(1600);
  OrderNumber h = submit(PLAYER_A, Point{4, 5});

  // When: time passes until H has completed
  elapse(5400);

  // Then: both handovers count, the second worth twice the first
  EXPECT_EQ(endingOf(PLAYER_A, h), EndReason::COMPLETED);
  EXPECT_EQ(scoreOf(PLAYER_A).banked, 10 + 20);
  EXPECT_EQ(scoreOf(PLAYER_A).chain, 2);
}

/// @brief The window reaches past G's arrival: a successor chosen before the next opportunity still follows G
TEST_F(OrbitalGameDomainApplication, AwardsHandoverToSuccessorSubmittedAfterCompletionBeforeOpportunity) {
  // Given: F hands over to G, which arrives at 5200 ms; H is submitted only at 5200 ms, after that step, and
  //        transmitted at the next opportunity at 8000 ms
  start(makeLanes({}));
  submit(PLAYER_A, Point{19, 2});
  elapse(3000);
  OrderNumber g = submit(PLAYER_A, Point{4, 6});
  elapse(2200);
  OrderNumber h = submit(PLAYER_A, Point{4, 5});

  // When: time passes until H has completed
  elapse(4800);

  // Then: both handovers count
  EXPECT_EQ(endingOf(PLAYER_A, g), EndReason::COMPLETED);
  EXPECT_EQ(endingOf(PLAYER_A, h), EndReason::COMPLETED);
  EXPECT_EQ(scoreOf(PLAYER_A).banked, 10 + 20);
  EXPECT_EQ(scoreOf(PLAYER_A).chain, 2);
}

/// @brief Submitting too late: the opportunity after G's arrival has passed before the successor exists
TEST_F(OrbitalGameDomainApplication, BreaksChainWhenSuccessorMissesOpportunityAfterCompletion) {
  // Given: F hands over to G, which arrives at 5200 ms; H is submitted only at 8000 ms, after the opportunity of that
  //        step, and transmitted at 11800 ms
  start(makeLanes({}));
  submit(PLAYER_A, Point{19, 2});
  elapse(3000);
  OrderNumber g = submit(PLAYER_A, Point{4, 6});
  elapse(5000);
  OrderNumber h = submit(PLAYER_A, Point{4, 5});

  // When: time passes until H has completed at 13000 ms
  elapse(5200);

  // Then: only G's handover counts and the chain is broken
  EXPECT_EQ(endingOf(PLAYER_A, g), EndReason::COMPLETED);
  EXPECT_EQ(endingOf(PLAYER_A, h), EndReason::COMPLETED);
  EXPECT_EQ(scoreOf(PLAYER_A).banked, 10);
  EXPECT_EQ(scoreOf(PLAYER_A).chain, 0);
}

/// @brief Submitting too early: an opportunity before G's arrival sends H, which then interrupts G and costs its
/// handover
TEST_F(OrbitalGameDomainApplication, BreaksChainWhenSuccessorActivatesFirst) {
  // Given: F with food at (5,2) and (7,2) hands over to G, which takes the long way round to (2,5) from 4400 ms;
  //        H is submitted at 4600 ms and transmitted at the opportunity at 8000 ms
  start(makeLanes({Point{5, 2}, Point{7, 2}}));
  submit(PLAYER_A, Point{19, 2});
  elapse(3000);
  OrderNumber g = submit(PLAYER_A, Point{2, 5});
  elapse(1600);
  OrderNumber h = submit(PLAYER_A, Point{3, 6});

  // When: time passes until H has activated at 8200 ms, before G could arrive
  elapse(3600);

  // Then: G was interrupted, so its handover is lost: only F's 20 points count and the chain is broken
  EXPECT_EQ(endingOf(PLAYER_A, g), EndReason::INTERRUPTED);
  EXPECT_EQ(executingOrderOf(PLAYER_A), h);
  EXPECT_EQ(scoreOf(PLAYER_A).banked, 20);
  EXPECT_EQ(scoreOf(PLAYER_A).chain, 0);
}

/// @brief Changing one's mind about the successor before it is sent costs its handover, not the predecessor's points
TEST_F(OrbitalGameDomainApplication, DeniesHandoverButBanksPointsWhenSelectedSuccessorIsReplaced) {
  // Given: F collects food at (5,2) and (7,2); G is submitted at 3000 ms and replaced by G' at 3100 ms, before the
  //        opportunity at 4200 ms
  start(makeLanes({Point{5, 2}, Point{7, 2}}));
  submit(PLAYER_A, Point{19, 2});
  elapse(3000);
  OrderNumber g = submit(PLAYER_A, Point{4, 8});
  elapse(100);
  OrderNumber replacement = submit(PLAYER_A, Point{4, 6});

  // When: time passes until G' has completed at 5200 ms
  elapse(2100);

  // Then: G was never sent, G' took over and completed, F's food points count, and G' earned no handover
  EXPECT_EQ(replacedBy(PLAYER_A, replacement), g);
  EXPECT_FALSE(isTransmitted(PLAYER_A, g));
  EXPECT_EQ(endingOf(PLAYER_A, replacement), EndReason::COMPLETED);
  EXPECT_EQ(scoreOf(PLAYER_A).banked, 20);
  EXPECT_EQ(scoreOf(PLAYER_A).chain, 0);
}

/// @brief A refused order never executed, so the successor that follows it still hands over from F
TEST_F(OrbitalGameDomainApplication, AwardsHandoverPastRejectedOrder) {
  // Given: F takes the long way round to (2,5) until 4600 ms; G, with a target outside the board, is submitted at
  //        600 ms, transmitted at 4200 ms and rejected at 4400 ms; H is submitted at 4400 ms while F still executes
  start(makeLanes({}));
  submit(PLAYER_A, Point{2, 5});
  elapse(600);
  OrderNumber g = submit(PLAYER_A, Point{25, 2});
  elapse(3800);
  OrderNumber h = submit(PLAYER_A, Point{2, 6});

  // When: time passes until H, transmitted at 8000 ms, has completed
  elapse(6000);

  // Then: G was rejected, and H's completion is a handover from F
  EXPECT_EQ(activationOf(PLAYER_A, g),
            (ActivationOutcome{OrderRejected{g, PLAYER_A, RejectionReason::TARGET_OFF_BOARD}}));
  EXPECT_EQ(endingOf(PLAYER_A, h), EndReason::COMPLETED);
  EXPECT_EQ(scoreOf(PLAYER_A).banked, 10);
  EXPECT_EQ(scoreOf(PLAYER_A).chain, 1);
}

/// @subsection transmission and activation

TEST_F(OrbitalGameDomainApplication, TransmitsSelectedOrderAtOpportunity) {
  // Given: G is submitted at 0 ms
  start(makeLanes({}));
  OrderNumber g = submit(PLAYER_A, Point{10, 6});

  // When: the step at 200 ms, an opportunity, runs
  elapse(200);

  // Then: G is transmitted at the end of that step, with its submission time, and not yet tried
  EXPECT_FALSE(activationOf(PLAYER_A, g).has_value());
  ASSERT_EQ(players_.steps.back().transmissions.size(), 1U);
  EXPECT_EQ(players_.steps.back().transmissions.front().order, g);
  EXPECT_EQ(players_.steps.back().transmissions.front().submitted_at, StepId{0});
}

/// @brief An order transmitted at a step is tried at the next step, before that step's movement
TEST_F(OrbitalGameDomainApplication, ActivatesTransmittedOrderAtNextStep) {
  // Given: G was transmitted at 200 ms
  start(makeLanes({}));
  OrderNumber g = submit(PLAYER_A, Point{10, 6});
  elapse(200);

  // When: the step at 400 ms runs
  elapse(200);

  // Then: G was activated in that step
  EXPECT_EQ(executingOrderOf(PLAYER_A), g);
  ASSERT_EQ(players_.steps.back().activations.size(), 1U);
}

/// @brief Without an opportunity the selection waits; a submission at the time of an opportunity's step came after it
TEST_F(OrbitalGameDomainApplication, KeepsOrderSelectedUntilNextOpportunity) {
  // Given: the step at 200 ms was an opportunity, and the next one comes at 4200 ms; G is submitted at 200 ms
  start(makeLanes({}));
  elapse(200);
  OrderNumber g = submit(PLAYER_A, Point{10, 6});

  // When: time passes until just before the next opportunity
  elapse(3800);

  // Then: G is still selected
  EXPECT_EQ(selectedOrderOf(PLAYER_A), g);
}

/// @brief Only the latest selection is sent; earlier ones never leave the player
TEST_F(OrbitalGameDomainApplication, TransmitsOnlyLatestSelection) {
  // Given: G and then H were submitted before the opportunity at 200 ms
  start(makeLanes({}));
  OrderNumber g = submit(PLAYER_A, Point{10, 6});
  elapse(100);
  OrderNumber h = submit(PLAYER_A, Point{12, 2});

  // When: the step at 200 ms runs
  elapse(100);

  // Then: G was replaced and H transmitted
  EXPECT_EQ(replacedBy(PLAYER_A, h), g);
  EXPECT_FALSE(isTransmitted(PLAYER_A, g));
  EXPECT_TRUE(isTransmitted(PLAYER_A, h));
}

/// @brief A transmitted order cannot be recalled; a correction follows it at a later opportunity
TEST_F(OrbitalGameDomainApplication, ActivatesTransmittedOrderDespiteLaterSubmission) {
  // Given: G was transmitted at 200 ms, and H is submitted at 200 ms
  start(makeLanes({}));
  OrderNumber g = submit(PLAYER_A, Point{10, 6});
  elapse(200);
  OrderNumber h = submit(PLAYER_A, Point{12, 2});

  // When: time passes until the next opportunity at 4200 ms
  elapse(4000);

  // Then: G activated at 400 ms and H was transmitted after it
  ASSERT_TRUE(activationOf(PLAYER_A, g).has_value());
  EXPECT_TRUE(std::holds_alternative<OrderActivated>(*activationOf(PLAYER_A, g)));
  EXPECT_TRUE(isTransmitted(PLAYER_A, h));
  EXPECT_FALSE(activationOf(PLAYER_A, h).has_value());
}

/// @brief A refused replacement neither stops the executing order nor costs its points
TEST_F(OrbitalGameDomainApplication, KeepsExecutingOrderAndItsPointsWhenReplacementIsRejected) {
  // Given: F to (2,5) collected food at (5,2), and G with a target outside the board was submitted at 600 ms
  start(makeLanes({Point{5, 2}}));
  OrderNumber f = submit(PLAYER_A, Point{2, 5});
  elapse(600);
  OrderNumber g = submit(PLAYER_A, Point{25, 2});

  // When: time passes until G, transmitted at 4200 ms, was evaluated at 4400 ms
  elapse(3800);

  // Then: G is rejected, F still executes with its 10 held points
  EXPECT_EQ(activationOf(PLAYER_A, g),
            (ActivationOutcome{OrderRejected{g, PLAYER_A, RejectionReason::TARGET_OFF_BOARD}}));
  EXPECT_EQ(executingOrderOf(PLAYER_A), f);
  EXPECT_EQ(scoreOf(PLAYER_A).held, 10);
}

/// @subsection shared board

/// @brief A food race: the loser's positional order stays valid after the food is gone
TEST_F(OrbitalGameDomainApplication, CompletesPositionalOrderAfterOpponentTookTheFood) {
  // Given: A orders its snake to the food at (8,2); B heads down column 8 and takes the food at once
  RoundSetup setup = makeLanes({Point{8, 2}});
  setup.layout.snakes.at(PLAYER_B) = SnakeSetup{Point{8, 1}, Direction::DOWN, 2};
  start(setup);
  OrderNumber order = submit(PLAYER_A, Point{8, 2});

  // When: A's snake arrives at (8,2)
  elapse(1200);

  // Then: B, eating without an order, banked 5 for the food and A's order completed without points
  EXPECT_EQ(endingOf(PLAYER_A, order), EndReason::COMPLETED);
  EXPECT_EQ(scoreOf(PLAYER_B).banked, 5);
  EXPECT_EQ(scoreOf(PLAYER_A).banked, 0);
}

/// @subsection conclusion

/// @brief The step due at the end of the game counts, including the completion it brings
TEST_F(OrbitalGameDomainApplication, BanksCompletionOnFinalStepBeforeConclusion) {
  // Given: at 118400 ms A heads right in row 2; an order to (19,5) collects food at (19,3) and arrives at 120000 ms
  start(makeLanes({Point{19, 3}}));
  elapse(118400);
  OrderNumber order = submit(PLAYER_A, Point{19, 5});

  // When: the remaining time of the game passes
  elapse(1600);

  // Then: the order completed on the final step, its 10 points count and A wins
  EXPECT_EQ(endingOf(PLAYER_A, order), EndReason::COMPLETED);
  EXPECT_EQ(scoreOf(PLAYER_A).banked, 10);
  ASSERT_EQ(players_.conclusions.size(), 1U);
  EXPECT_EQ(players_.conclusions.front().result, (game_authority::Result{PlayerId{PLAYER_A}}));
  EXPECT_EQ(players_.steps.back().step, StepId{600});
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

/// @brief Once the game is over, executing orders end with it and orders not yet executing are cancelled
TEST_F(OrbitalGameDomainApplication, EndsExecutingAndCancelsWaitingOrdersAtConclusion) {
  // Given: F executes since 118400 ms, and G submitted at 119800 ms would be transmitted only at the end of the step
  //        that concludes the game
  start(makeLanes({}));
  elapse(118000);
  OrderNumber f = submit(PLAYER_A, Point{12, 6});
  elapse(1800);
  OrderNumber g = submit(PLAYER_A, Point{3, 3});

  // When: the game ends
  elapse(200);

  // Then: F ended with the round, and G was cancelled without being sent
  EXPECT_EQ(endingOf(PLAYER_A, f), EndReason::ROUND_ENDED);
  EXPECT_FALSE(selectedOrderOf(PLAYER_A).has_value());
  EXPECT_FALSE(isTransmitted(PLAYER_A, g));
  EXPECT_TRUE(arena().active.empty());
}

TEST_F(OrbitalGameDomainApplication, RejectsSubmissionBeforeStart) {
  // When: a player submits an order before a game has started
  game_.submit(app::Submit{PLAYER_A, Point{3, 3}});

  // Then: it is rejected because no game has started, and nothing is selected
  ASSERT_EQ(players_.submissions.size(), 1U);
  EXPECT_EQ(std::get<SubmissionRejected>(players_.submissions.front()).reason, SubmissionRejectionReason::NOT_STARTED);
  EXPECT_TRUE(players_.submitted.empty());
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

/// @brief Both players share one end, so a refusal names whose submission it refuses
TEST_F(OrbitalGameDomainApplication, NamesPlayerOfRejectedSubmission) {
  // When: player B submits an order before a game has started
  game_.submit(app::Submit{PLAYER_B, Point{3, 3}});

  // Then: the refusal names player B
  ASSERT_EQ(players_.submissions.size(), 1U);
  EXPECT_EQ(std::get<SubmissionRejected>(players_.submissions.front()).player, PLAYER_B);
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

  // Then: the game concluded on step 600 at two minutes, which was the last to run, and the views stay there
  ASSERT_EQ(players_.conclusions.size(), 1U);
  EXPECT_EQ(players_.steps.size(), 600U);
  EXPECT_EQ(players_.steps.back().step, StepId{600});
  EXPECT_EQ(players_.views->arena.step, StepId{600});
}

/// @subsection time

/// @brief One time advance can run several steps; every step's report is kept
TEST_F(OrbitalGameDomainApplication, ReportsEveryStepOfOneTimeAdvance) {
  // Given: a game started
  start(makeLanes({}));

  // When: one second passes at once
  elapse(1000);

  // Then: five steps ran, one per 200 ms
  ASSERT_EQ(players_.steps.size(), 5U);
  EXPECT_EQ(players_.steps.front().step, StepId{1});
  EXPECT_EQ(players_.steps.back().step, StepId{5});
}

/// @brief Before a game has started there is no game time that could pass
TEST_F(OrbitalGameDomainApplication, IgnoresTimeBeforeStart) {
  // When: one second passes before a game has started
  elapse(1000);

  // Then: no step runs and nothing is reported, not even views
  EXPECT_TRUE(players_.steps.empty());
  EXPECT_FALSE(players_.views.has_value());
}

/// @brief Game time cannot run backward, so a later submission is never submitted earlier
TEST_F(OrbitalGameDomainApplication, RefusesNegativeElapsedTime) {
  // Given: a game started
  start(makeLanes({}));

  // When/Then: a negative elapsed time is refused
  EXPECT_THROW(elapse(-1), std::invalid_argument);
}

/// @brief Cutting the same game time into smaller advances does not change what happens
TEST_F(OrbitalGameDomainApplication, ReportsSameStepsAndViewsForOneAdvanceAndItsParts) {
  // Given: in two games, A's order to (7,2) is selected; it is transmitted at 200 ms, activates at 400 ms and
  //        collects the food at (6,2) on its way
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
  ASSERT_EQ(at_once_steps[0].transmissions.size(), 1U);
  ASSERT_EQ(at_once_steps[1].activations.size(), 1U);
  ASSERT_FALSE(at_once_steps[3].events.empty());
  ASSERT_EQ(in_parts_steps.size(), at_once_steps.size());
  for (size_t i = 0; i < at_once_steps.size(); ++i) {
    EXPECT_EQ(in_parts_steps[i].step, at_once_steps[i].step);
    EXPECT_EQ(in_parts_steps[i].activations, at_once_steps[i].activations);
    EXPECT_EQ(in_parts_steps[i].events, at_once_steps[i].events);
    EXPECT_EQ(in_parts_steps[i].transmissions, at_once_steps[i].transmissions);
  }
  EXPECT_EQ(viewedOutcome(*in_parts_players.views), viewedOutcome(*at_once_players.views));
}

/// @brief A submission happens in the step ongoing on the game clock, the last one that ran
TEST_F(OrbitalGameDomainApplication, SubmitsInCurrentStep) {
  // Given: 1300 ms have passed, so step 6 ran at 1200 ms
  start(makeLanes({}));
  elapse(1300);

  // When: a player submits an order
  game_.submit(app::Submit{PLAYER_A, Point{3, 3}});

  // Then: it is submitted in step 6
  ASSERT_EQ(players_.submissions.size(), 1U);
  EXPECT_EQ(std::get<SubmissionAccepted>(players_.submissions.front()).submitted_at, StepId{6});
}

/// @brief Both players share one end and number their orders apart, so the outcome names the order's player
TEST_F(OrbitalGameDomainApplication, NamesPlayerOfAcceptedSubmission) {
  // Given: player A's first order was accepted
  start(makeLanes({}));
  game_.submit(app::Submit{PLAYER_A, Point{3, 3}});

  // When: player B submits their first order
  game_.submit(app::Submit{PLAYER_B, Point{4, 4}});

  // Then: the outcome identifies it as B's order 1
  ASSERT_EQ(players_.submissions.size(), 2U);
  const SubmissionAccepted& accepted = std::get<SubmissionAccepted>(players_.submissions.back());
  EXPECT_EQ(accepted.player, PLAYER_B);
  EXPECT_EQ(accepted.order, OrderNumber{1});
}

/// @subsection order of occurrence

/// @brief The players learn an outcome before the views that already reflect it
TEST_F(OrbitalGameDomainApplication, ReportsAcceptedSubmissionInOrderOfOccurrence) {
  // Given: a game started, and only what follows is observed
  start(makeLanes({}));
  players_.clear();

  // When: a player submits an order
  game_.submit(app::Submit{PLAYER_A, Point{3, 3}});

  // Then: the outcome comes first, then the submission record, then the views
  EXPECT_EQ(players_.calls, (std::vector<std::string>{"submissionOutcome", "orderSubmitted", "viewsUpdated"}));
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

  // Then: the report of both steps completes first; the submission follows, submitted in the second step
  EXPECT_EQ(
      players.calls,
      (std::vector<std::string>{
          "stepReported", "stepReported", "viewsUpdated", "submissionOutcome", "orderSubmitted", "viewsUpdated"}));
  ASSERT_EQ(players.submitted.size(), 1U);
  EXPECT_EQ(players.submitted.front().submitted_at, StepId{2});
  EXPECT_EQ(players.views->selections.selections.count(PLAYER_A), 1U);
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
  EXPECT_TRUE(players.submitted.empty());
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

  // Then: it is submitted in the second step
  ASSERT_EQ(players.submitted.size(), 1U);
  EXPECT_EQ(players.submitted.front().submitted_at, StepId{2});
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

  // Then: steps 2 and 3 run from the state after step 1, and A ate the food
  ASSERT_EQ(players.steps.size(), 2U);
  EXPECT_EQ(players.steps.front().step, StepId{2});
  EXPECT_EQ(players.steps.back().step, StepId{3});
  EXPECT_EQ(players.views->arena.step, StepId{3});
  EXPECT_EQ(snake_model::length(players.views->arena.snakes.at(PLAYER_A)), 4U);
}

/// @subsection views

/// @brief Players see the whole schedule, so they can time a submission around a predecessor's arrival
TEST_F(OrbitalGameDomainApplication, ViewsTransmissionScheduleOfGame) {
  // When: a game starts
  game_.start(app::Start{RoundId{1}});

  // Then: the views carry every opportunity of the 600 steps: 20 steps apart after the first, 3 apart at the end
  ASSERT_TRUE(players_.views.has_value());
  const std::vector<StepId>& schedule = players_.views->transmission_schedule;
  ASSERT_EQ(schedule.size(), 71U);
  EXPECT_EQ(schedule[0], StepId{1});
  EXPECT_EQ(schedule[1], StepId{21});
  EXPECT_EQ(schedule[2], StepId{40});
  EXPECT_EQ(schedule.back(), StepId{599});
  EXPECT_EQ(schedule[schedule.size() - 2], StepId{596});
}

/// @brief Presentation places steps on the timeline without asking the Pacing Policy itself
TEST_F(OrbitalGameDomainApplication, ViewsStepInterval) {
  // When: a game starts
  game_.start(app::Start{RoundId{1}});

  // Then: the views show the step interval of 200 ms
  ASSERT_TRUE(players_.views.has_value());
  EXPECT_EQ(players_.views->step_interval, milliseconds{200});
}

/// @brief Presentation spans the timeline of the game without asking the Pacing Policy itself
TEST_F(OrbitalGameDomainApplication, ViewsLastStepOfGame) {
  // When: a game starts
  game_.start(app::Start{RoundId{1}});

  // Then: the views show step 600 as the last step of the game
  ASSERT_TRUE(players_.views.has_value());
  EXPECT_EQ(players_.views->last_step, StepId{600});
}

/// @brief Presentation shows when an executing order arrives without counting its moves itself
TEST_F(OrbitalGameDomainApplication, ViewsExpectedCompletionOfExecutingOrder) {
  // Given: with A's snake at (2,2) heading right, A selected (8,5), which was transmitted in step 1
  start(makeLanes({}));
  submit(PLAYER_A, Point{8, 5});
  elapse(200);

  // When: step 2 activates the order, whose eight moves start from (3,2)
  elapse(200);

  // Then: the order is expected to complete with its last move in step 9
  EXPECT_EQ(players_.views->expected_completions, (std::map<PlayerId, StepId>{{PLAYER_A, StepId{9}}}));
}

TEST_F(OrbitalGameDomainApplication, ViewsRouteOfSelectedOrder) {
  // Given: a game started, with A's snake at (2,2) heading right
  start(makeLanes({}));

  // When: A selects (8,5)
  submit(PLAYER_A, Point{8, 5});

  // Then: the selected route starts from (2,2): six moves right, three down
  using D = Direction;
  EXPECT_EQ(
      players_.views->selected_routes.at(PLAYER_A),
      (std::vector<Direction>{D::RIGHT, D::RIGHT, D::RIGHT, D::RIGHT, D::RIGHT, D::RIGHT, D::DOWN, D::DOWN, D::DOWN}));
}

/// @brief The preview of a transmitted order starts from the snake where it is when the order was sent
TEST_F(OrbitalGameDomainApplication, ViewsRouteOfTransmittedOrderFromCurrentSnakePosition) {
  // Given: with A's snake at (2,2) heading right, A selected (8,5)
  start(makeLanes({}));
  submit(PLAYER_A, Point{8, 5});

  // When: the step at 200 ms moved the snake to (3,2) and transmitted the order
  elapse(200);

  // Then: the transmitted route starts from (3,2): five moves right, three down
  using D = Direction;
  EXPECT_EQ(players_.views->transmitted_routes.at(PLAYER_A),
            (std::vector<Direction>{D::RIGHT, D::RIGHT, D::RIGHT, D::RIGHT, D::RIGHT, D::DOWN, D::DOWN, D::DOWN}));
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
  EXPECT_EQ(players_.views->arena.step, StepId{600});
}

}  // namespace orbital
}  // namespace snake
