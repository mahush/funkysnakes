// Tests for the host composition of the distributed Orbital realization: driven through its interfaces only (start,
// steps, the devices' forwarded facts), observed through what it reports, tells the devices and records.

#include <gtest/gtest.h>

#include <chrono>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

#include "domain/orbital/order_selection_authority.hpp"
#include "domain/orbital/pacing_policy.hpp"
#include "domain/orbital/transmission_schedule_policy.hpp"
#include "orbital_loopback/host_composition.hpp"
#include "orbital_recording_players.hpp"
#include "test_printers.hpp"

namespace snake {
namespace orbital {
namespace loopback {

using game_boundary::Start;
using placement_link::GameStarted;
using platform::transmission_reception_authority::DiscardedTransmission;
using platform::transmission_reception_authority::LateTransmission;
using platform::transmission_reception_authority::Receipt;
using platform::transmission_reception_authority::SupersededTransmission;
using std::chrono::milliseconds;

namespace {

constexpr RoundId ROUND{1};
constexpr milliseconds T0{1000};  // Host game clock when the game started

RandomIntGeneratorFn makeFixedRandom() {
  return [](int /* min */, int max) { return max; };
}

OrderSubmitted makeOrderSubmitted(const PlayerId& player, int order, Point target, int submitted_at) {
  return OrderSubmitted{ROUND, OrderNumber{order}, player, target, StepId{submitted_at}, std::nullopt};
}

OrderTransmitted makeOrderTransmitted(const PlayerId& player, int order, Point target, int transmitted_at) {
  return OrderTransmitted{
      ROUND, OrderNumber{order}, player, target, StepId{transmitted_at}, StepId{transmitted_at}, std::nullopt};
}

// A player's device for tests: records what the host tells it, into a call log shared with the players' side
struct RecordingPhone : placement_link::HostToPhone {
  RecordingPhone(std::string name, std::vector<std::string>& calls) : name{std::move(name)}, calls{calls} {}

  void gameStarted(const GameStarted& started) override {
    calls.push_back(name + ".gameStarted");
    starts.push_back(started);
  }

  void concluded(const game_authority::GameConcluded& concluded) override {
    calls.push_back(name + ".concluded");
    conclusions.push_back(concluded);
  }

  std::string name;
  std::vector<std::string>& calls;  // Shared with the players' side
  std::vector<GameStarted> starts;
  std::vector<game_authority::GameConcluded> conclusions;
};

// A players' side that forwards a submission record to the host as soon as it learns of the first step
struct ForwardingOnFirstStep : RecordingPlayers {
  void stepReported(const game_boundary::StepReport& step) override {
    RecordingPlayers::stepReported(step);
    if (!has_forwarded) {
      has_forwarded = true;
      host->forward(makeOrderSubmitted(PLAYER_A, 1, Point{12, 8}, 1));
    }
  }

  HostComposition* host{nullptr};
  bool has_forwarded{false};
};

/**
 * @brief One host with fixed randomness, the players' side and two devices that record what it reports and tells
 */
class HostCompositionTest : public ::testing::Test {
 protected:
  HostCompositionTest()
      : phone_a_{"phoneA", players_.calls},
        phone_b_{"phoneB", players_.calls},
        host_{players_, {phone_a_, phone_b_}, makeFixedRandom()},
        steps_reached_{0} {}

  void startGame() { host_.start(Start{ROUND}, T0); }

  // Let the host time reach the time of the next step
  void runNextStep() {
    ++steps_reached_;
    host_.runDueSteps(T0 + pacing_policy::timeOfStep(StepId{steps_reached_}));
  }

  void runSteps(int count) {
    for (int i = 0; i < count; ++i) {
      runNextStep();
    }
  }

  // Run steps one at a time until the host reports the conclusion
  void runToConclusion() {
    while (steps_reached_ < pacing_policy::LAST_STEP.value && players_.conclusions.empty()) {
      runNextStep();
    }
  }

  RecordingPlayers players_;
  RecordingPhone phone_a_;
  RecordingPhone phone_b_;
  HostComposition host_;
  int steps_reached_;  // Steps whose time the host time has reached
};

}  // namespace

/// @section HostComposition::start

TEST_F(HostCompositionTest, TellsEveryDeviceOfStartAfterItsViews) {
  // When: the host starts the game at T0
  startGame();

  // Then: the views are reported, then each device learns the round and T0
  EXPECT_EQ(players_.calls, (std::vector<std::string>{"viewsUpdated", "phoneA.gameStarted", "phoneB.gameStarted"}));
  EXPECT_EQ(phone_a_.starts.size(), 1U);
  EXPECT_EQ(phone_a_.starts.front().round, ROUND);
  EXPECT_EQ(phone_a_.starts.front().t0, T0);
  EXPECT_EQ(phone_b_.starts.size(), 1U);
}

TEST_F(HostCompositionTest, RejectsSecondStartWithoutTellingDevices) {
  // Given: the game started
  startGame();
  players_.clear();

  // When: the host is started again
  host_.start(Start{RoundId{2}}, T0 + milliseconds{500});

  // Then: the start is rejected with the views of the running game, and no device is told again
  EXPECT_EQ(players_.calls, (std::vector<std::string>{"startRejected", "viewsUpdated"}));
  EXPECT_EQ(players_.start_rejections.front().reason, game_boundary::StartRejectionReason::ALREADY_STARTED);
  EXPECT_EQ(players_.views->arena.step, StepId{0});
  EXPECT_EQ(phone_a_.starts.size(), 1U);
}

/// @section HostComposition::runDueSteps

TEST_F(HostCompositionTest, ReportsNothingBeforeGameStarted) {
  // When: the host time reaches the time of step 1 before the game started
  host_.runDueSteps(T0 + pacing_policy::timeOfStep(StepId{1}));

  // Then: nothing is reported
  EXPECT_TRUE(players_.calls.empty());
}

TEST_F(HostCompositionTest, RunsEveryDueStepInOrder) {
  // Given: the game started
  startGame();
  players_.clear();

  // When: the host time reaches the time of step 3 at once
  host_.runDueSteps(T0 + pacing_policy::timeOfStep(StepId{3}));

  // Then: steps 1 to 3 are reported in order, followed by the views once
  EXPECT_EQ(players_.calls, (std::vector<std::string>{"stepReported", "stepReported", "stepReported", "viewsUpdated"}));
  ASSERT_EQ(players_.steps.size(), 3U);
  EXPECT_EQ(players_.steps.front().step, StepId{1});
  EXPECT_EQ(players_.steps.back().step, StepId{3});
}

/// @brief The steps are timed from the start time T0, not from zero
TEST_F(HostCompositionTest, RunsNoStepBeforeItsTime) {
  // Given: the game started
  startGame();
  players_.clear();

  // When: the host time is just before the time of step 1
  host_.runDueSteps(T0 + pacing_policy::timeOfStep(StepId{1}) - milliseconds{1});

  // Then: only the views are reported, still before step 1
  EXPECT_EQ(players_.calls, (std::vector<std::string>{"viewsUpdated"}));
  EXPECT_EQ(players_.views->arena.step, StepId{0});
}

TEST_F(HostCompositionTest, RunsNoStepAgainForEarlierTime) {
  // Given: steps 1 and 2 ran
  startGame();
  runSteps(2);
  players_.clear();

  // When: the host time given is that of step 1 again
  host_.runDueSteps(T0 + pacing_policy::timeOfStep(StepId{1}));

  // Then: only the unchanged views are reported
  EXPECT_EQ(players_.calls, (std::vector<std::string>{"viewsUpdated"}));
  EXPECT_EQ(players_.views->arena.step, StepId{2});
}

TEST_F(HostCompositionTest, HandsTransmissionToArenaInItsScheduledStep) {
  // Given: A's order transmitted in step 1 arrived before step 1 ran
  startGame();
  host_.forward(makeOrderTransmitted(PLAYER_A, 1, Point{12, 8}, 1));
  runNextStep();

  // When: step 2, its scheduled step, runs
  runNextStep();

  // Then: the order is activated in step 2, no longer waits, and nothing is recorded as late
  const game_boundary::StepReport& step = players_.steps.back();
  EXPECT_EQ(step.step, StepId{2});
  ASSERT_EQ(step.activations.size(), 1U);
  EXPECT_TRUE(std::holds_alternative<OrderActivated>(step.activations.front()));
  EXPECT_TRUE(players_.views->transmitted.empty());
  EXPECT_EQ(players_.views->arena.active.count(PLAYER_A), 1U);
  EXPECT_TRUE(host_.record().late.empty());
}

/// @brief A step report on the host carries no transmissions: they reach the host after their step
TEST_F(HostCompositionTest, ReportsStepWithoutTransmissions) {
  // Given: A's order transmitted in step 1 arrived before step 1 ran
  startGame();
  host_.forward(makeOrderTransmitted(PLAYER_A, 1, Point{12, 8}, 1));

  // When: step 1 runs
  runNextStep();

  // Then: its report lists no transmission
  EXPECT_TRUE(players_.steps.back().transmissions.empty());
}

TEST_F(HostCompositionTest, RecordsLateTransmission) {
  // Given: steps 1 and 2 ran, then A's order transmitted in step 1 arrived, after its scheduled step 2
  startGame();
  runSteps(2);
  const OrderTransmitted transmitted = makeOrderTransmitted(PLAYER_A, 1, Point{12, 8}, 1);
  host_.forward(transmitted);

  // When: step 3 runs
  runNextStep();

  // Then: the order is activated in step 3 and recorded as late for it
  EXPECT_EQ(players_.steps.back().activations.size(), 1U);
  EXPECT_EQ(host_.record().late, (std::vector<LateTransmission>{{transmitted, StepId{3}}}));
}

TEST_F(HostCompositionTest, RecordsSupersededTransmission) {
  // Given: after step 2, A's late order 1 and its order 2 due in step 3 arrived
  startGame();
  runSteps(2);
  const OrderTransmitted older = makeOrderTransmitted(PLAYER_A, 1, Point{12, 8}, 1);
  host_.forward(older);
  host_.forward(makeOrderTransmitted(PLAYER_A, 2, Point{20, 3}, 2));

  // When: step 3 runs
  runNextStep();

  // Then: order 2 is activated, and order 1 is recorded as superseded by it in step 3
  ASSERT_EQ(players_.steps.back().activations.size(), 1U);
  EXPECT_EQ(std::get<OrderActivated>(players_.steps.back().activations.front()).order, OrderNumber{2});
  EXPECT_EQ(host_.record().superseded, (std::vector<SupersededTransmission>{{older, OrderNumber{2}, StepId{3}}}));
}

TEST_F(HostCompositionTest, RecordsTransmissionsDiscardedAtConclusion) {
  // Given: A's order transmitted in the last step arrived early, before that step ran
  startGame();
  const OrderTransmitted early = makeOrderTransmitted(PLAYER_A, 1, Point{12, 8}, pacing_policy::LAST_STEP.value);
  host_.forward(early);

  // When: the game runs to its conclusion
  runToConclusion();

  // Then: the order is recorded as discarded
  EXPECT_EQ(host_.record().discarded, (std::vector<DiscardedTransmission>{{early}}));
}

/// @brief The views the players see last are the concluded ones, before any device learns of the conclusion
TEST_F(HostCompositionTest, TellsEveryDeviceOfConclusionAfterItsReport) {
  // Given: the game started
  startGame();
  players_.clear();

  // When: the game runs to its conclusion
  runToConclusion();

  // Then: the concluding step, the conclusion and the views are reported, then each device learns of it
  ASSERT_GE(players_.calls.size(), 5U);
  EXPECT_EQ(
      std::vector<std::string>(players_.calls.end() - 5, players_.calls.end()),
      (std::vector<std::string>{"stepReported", "concluded", "viewsUpdated", "phoneA.concluded", "phoneB.concluded"}));
  EXPECT_EQ(players_.steps.back().step, pacing_policy::LAST_STEP);
  EXPECT_EQ(phone_a_.conclusions.size(), 1U);
  EXPECT_EQ(phone_b_.conclusions.size(), 1U);
}

TEST_F(HostCompositionTest, ReportsOrdersEndedWithRound) {
  // Given: A's order executes with a long route when the game is about to conclude
  startGame();
  runSteps(pacing_policy::LAST_STEP.value - 3);
  host_.forward(makeOrderTransmitted(PLAYER_A, 1, Point{1, 25}, pacing_policy::LAST_STEP.value - 3));
  players_.clear();

  // When: the game runs to its conclusion
  runToConclusion();

  // Then: the executing order ends with the round, reported before the conclusion
  EXPECT_EQ(players_.ended_with_round.size(), 1U);
  ASSERT_GE(players_.calls.size(), 6U);
  EXPECT_EQ(std::vector<std::string>(players_.calls.end() - 6, players_.calls.end() - 2),
            (std::vector<std::string>{"stepReported", "ordersEndedWithRound", "concluded", "viewsUpdated"}));
}

TEST_F(HostCompositionTest, RunsNoStepAfterConclusion) {
  // Given: the game concluded
  startGame();
  runToConclusion();
  players_.clear();

  // When: the host time reaches the time another step would have
  host_.runDueSteps(T0 + pacing_policy::timeOfStep(pacing_policy::LAST_STEP) + pacing_policy::STEP_INTERVAL);

  // Then: only the unchanged views are reported, and no device is told anything
  EXPECT_EQ(players_.calls, (std::vector<std::string>{"viewsUpdated"}));
  EXPECT_EQ(players_.views->arena.step, pacing_policy::LAST_STEP);
}

/// @brief A report never mixes two inputs, so the last views are always the current ones
TEST(HostComposition, TakesInputFromWithinReportAfterIt) {
  // Given: a started host whose players' side forwards a submission on learning of the first step
  ForwardingOnFirstStep players;
  std::vector<std::string> calls;
  RecordingPhone phone{"phone", calls};
  HostComposition host{players, {phone}, makeFixedRandom()};
  players.host = &host;
  host.start(Start{ROUND}, T0);
  players.clear();

  // When: step 1 runs
  host.runDueSteps(T0 + pacing_policy::timeOfStep(StepId{1}));

  // Then: the step's report is complete before the forwarded submission is shown, in views of its own
  EXPECT_EQ(players.calls, (std::vector<std::string>{"stepReported", "viewsUpdated", "viewsUpdated"}));
  EXPECT_EQ(players.views->selections.selections.count(PLAYER_A), 1U);
}

/// @section HostComposition::forward(OrderSubmitted)

TEST_F(HostCompositionTest, ShowsForwardedSelection) {
  // Given: the game started
  startGame();

  // When: A's device forwards its submission record
  host_.forward(makeOrderSubmitted(PLAYER_A, 1, Point{12, 8}, 3));

  // Then: the views show it as A's selection, with its route
  const order_selection_authority::Selection& selection = players_.views->selections.selections.at(PLAYER_A);
  EXPECT_EQ(selection.order, (Order{ROUND, OrderNumber{1}, PLAYER_A, Point{12, 8}}));
  EXPECT_EQ(selection.submitted_at, StepId{3});
  EXPECT_EQ(players_.views->selected_routes.count(PLAYER_A), 1U);
}

/// @brief The selection view is a projection: it shows what the devices' Order Selection Authorities hold
TEST_F(HostCompositionTest, ShowsSelectionsAsOrderSelectionAuthorityHoldsThem) {
  // Given: the game started
  startGame();

  // When: A and B submit, A replaces its selection, both are transmitted, and A submits again, each record forwarded
  order_selection_authority::State selections = order_selection_authority::startRound(ROUND);
  auto forward_submit_fn = [&](const PlayerId& player, Point target, int at) {
    auto [next, submitted] = order_selection_authority::submit(
        std::move(selections), order_selection_authority::Submission{player, target, StepId{at}});
    selections = std::move(next);
    host_.forward(submitted);
  };
  forward_submit_fn(PLAYER_A, Point{12, 8}, 2);
  forward_submit_fn(PLAYER_B, Point{14, 20}, 5);
  forward_submit_fn(PLAYER_A, Point{20, 3}, 9);
  auto [after_transmit, transmissions] = order_selection_authority::transmit(std::move(selections), StepId{21});
  selections = std::move(after_transmit);
  for (const OrderTransmitted& transmitted : transmissions) {
    host_.forward(transmitted);
  }
  forward_submit_fn(PLAYER_A, Point{2, 25}, 22);

  // Then: the host shows the selections the Order Selection Authority's view shows
  EXPECT_EQ(players_.views->selections.selections, order_selection_authority::view(selections).selections);
}

/// @brief The host clears its projection in the step the reference cancels the selections
TEST_F(HostCompositionTest, ClearsSelectionsAtConclusionAsCancelDoes) {
  // Given: A's selection is shown
  startGame();
  auto [selections, submitted] = order_selection_authority::submit(
      order_selection_authority::startRound(ROUND),
      order_selection_authority::Submission{PLAYER_A, Point{12, 8}, StepId{pacing_policy::LAST_STEP.value - 5}});
  host_.forward(submitted);

  // When: the game runs to its conclusion
  runToConclusion();

  // Then: the host shows the selections the Order Selection Authority's view shows after cancel
  EXPECT_EQ(players_.views->selections.selections,
            order_selection_authority::view(order_selection_authority::cancel(std::move(selections))).selections);
}

TEST_F(HostCompositionTest, RecordsRoundNotStartedReceiptOfSubmissionBeforeStart) {
  // When: a submission record arrives before the game started
  const OrderSubmitted submitted = makeOrderSubmitted(PLAYER_A, 1, Point{12, 8}, 0);
  host_.forward(submitted);

  // Then: its receipt ROUND_NOT_STARTED is recorded, and nothing is reported
  EXPECT_EQ(host_.record().submission_receipts,
            (std::vector<SubmissionReceipt>{{submitted, Receipt::ROUND_NOT_STARTED}}));
  EXPECT_TRUE(players_.calls.empty());
}

/// @brief A device that has not heard of the conclusion yet may still accept a submission; the host shows none
TEST_F(HostCompositionTest, ShowsNoSubmissionArrivingAfterConclusion) {
  // Given: the game concluded
  startGame();
  runToConclusion();

  // When: A's submission record arrives
  const OrderSubmitted submitted = makeOrderSubmitted(PLAYER_A, 1, Point{12, 8}, pacing_policy::LAST_STEP.value);
  host_.forward(submitted);

  // Then: no selection is shown, and its receipt ROUND_CONCLUDED is recorded
  EXPECT_TRUE(players_.views->selections.selections.empty());
  EXPECT_EQ(host_.record().submission_receipts,
            (std::vector<SubmissionReceipt>{{submitted, Receipt::ROUND_CONCLUDED}}));
}

/// @section HostComposition::forward(OrderTransmitted)

TEST_F(HostCompositionTest, EndsShowingSelectionOnItsTransmission) {
  // Given: A's selection is shown
  startGame();
  host_.forward(makeOrderSubmitted(PLAYER_A, 1, Point{12, 8}, 0));

  // When: its transmission arrives
  const OrderTransmitted transmitted = makeOrderTransmitted(PLAYER_A, 1, Point{12, 8}, 1);
  host_.forward(transmitted);

  // Then: it is no longer shown as selected but as transmitted, with its route
  EXPECT_TRUE(players_.views->selections.selections.empty());
  EXPECT_EQ(players_.views->transmitted, (std::vector<OrderTransmitted>{transmitted}));
  EXPECT_EQ(players_.views->transmitted_routes.count(PLAYER_A), 1U);
}

TEST_F(HostCompositionTest, RecordsReceiptOfTransmission) {
  // Given: the game started
  startGame();

  // When: A's transmission arrives
  const OrderTransmitted transmitted = makeOrderTransmitted(PLAYER_A, 1, Point{12, 8}, 1);
  host_.forward(transmitted);

  // Then: its receipt is recorded, with no handover counted
  EXPECT_EQ(host_.record().receipts, (std::vector<TransmissionReceipt>{{transmitted, Receipt::RECEIVED, {}}}));
}

/// @brief A transmission lost at the conclusion counts as reported through its receipt
TEST_F(HostCompositionTest, RecordsRoundConcludedReceiptAfterConclusion) {
  // Given: the game concluded
  startGame();
  runToConclusion();

  // When: A's transmission of the last opportunity arrives
  const OrderTransmitted transmitted =
      makeOrderTransmitted(PLAYER_A, 1, Point{12, 8}, transmission_schedule_policy::opportunitiesOfGame().back().value);
  host_.forward(transmitted);

  // Then: its receipt ROUND_CONCLUDED is recorded, and nothing waits
  EXPECT_EQ(host_.record().receipts, (std::vector<TransmissionReceipt>{{transmitted, Receipt::ROUND_CONCLUDED, {}}}));
  EXPECT_TRUE(players_.views->transmitted.empty());
}

/// @brief A transmission can reach a host that has not started, e.g. after a restart; it takes no effect
TEST_F(HostCompositionTest, RecordsRoundNotStartedReceiptOfTransmissionBeforeStart) {
  // When: a transmission arrives before the game started
  const OrderTransmitted transmitted = makeOrderTransmitted(PLAYER_A, 1, Point{12, 8}, 1);
  host_.forward(transmitted);

  // Then: its receipt ROUND_NOT_STARTED is recorded, and nothing is reported
  EXPECT_EQ(host_.record().receipts, (std::vector<TransmissionReceipt>{{transmitted, Receipt::ROUND_NOT_STARTED, {}}}));
  EXPECT_TRUE(players_.calls.empty());
}

}  // namespace loopback
}  // namespace orbital
}  // namespace snake
