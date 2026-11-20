// Orbital Orders domain runner: plays the defining histories through the Domain Application and
// prints what happened, step by step: selections and transmissions (Order Authority), activations and
// endings (Arena Authority) and the resulting score and handover chain (Game Authority). No actors, timers, networking
// or rendering. Each history starts in a small hand-picked world through the Domain Application's test
// setup seam, so the traces are reference-only (see TestAccess).

#include <chrono>
#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "orbital/orbital_game_domain_application.hpp"
#include "orbital/orbital_game_domain_application_test_access.hpp"

namespace {

using namespace snake;
using namespace snake::orbital;
namespace app = orbital_game_domain_application;
using std::chrono::milliseconds;

// New food always appears in the far corner, so histories are reproducible
RandomIntGeneratorFn makeCornerRandom() {
  return [](int /* min */, int max) { return max; };
}

// Board 20x10: A in row 2 heading right with food at (5,2) and (7,2); B in row 7 heading right
arena_authority::RoundSetup makeBoard() {
  return arena_authority::RoundSetup{RoundId{1},
                                     StartLayout{Board{20, 10},
                                                 {{PLAYER_A, SnakeSetup{Point{2, 2}, Direction::RIGHT, 3}},
                                                  {PLAYER_B, SnakeSetup{Point{2, 7}, Direction::RIGHT, 3}}}},
                                     {Point{5, 2}, Point{7, 2}, Point{15, 9}, Point{16, 9}, Point{17, 9}}};
}

std::string point(Point p) { return "(" + std::to_string(p.x) + "," + std::to_string(p.y) + ")"; }

// Name of each submitted order by its player and identity, for the trace
using OrderNames = std::map<std::pair<PlayerId, OrderId>, std::string>;

std::string order(const OrderNames& names, const PlayerId& player, OrderId id) {
  auto it = names.find({player, id});
  return it != names.end() ? it->second : player + " #" + std::to_string(id.value);
}

std::string endReason(EndReason reason) {
  switch (reason) {
    case EndReason::COMPLETED:
      return "completed";
    case EndReason::INTERRUPTED:
      return "interrupted";
    case EndReason::ELIMINATED:
      return "eliminated";
    case EndReason::ROUND_ENDED:
      return "ended with the round";
  }
  return "?";
}

std::string describe(const OrderNames& names, const ActivationOutcome& outcome) {
  if (const auto* activated = std::get_if<OrderActivated>(&outcome)) {
    return order(names, activated->player, activated->order) + " activates";
  }
  const auto& rejected = std::get<OrderRejected>(outcome);
  return order(names, rejected.player, rejected.order) + " rejected";
}

std::string describe(const OrderNames& names, const ArenaEvent& event) {
  return std::visit(
      [&names](const auto& item) -> std::string {
        using TEvent = std::decay_t<decltype(item)>;
        if constexpr (std::is_same_v<TEvent, OrderEnded>) {
          return order(names, item.player, item.order) + " " + endReason(item.reason);
        } else if constexpr (std::is_same_v<TEvent, FoodCollected>) {
          return std::string{item.player == PLAYER_A ? "A" : "B"} + " eats at " + point(item.position) +
                 (item.during ? " during " + order(names, item.player, *item.during) : " (no order)");
        } else {
          return "collision";
        }
      },
      event);
}

// The players' side of the runner: prints what A learns, step by step. The score after a run of steps
// is printed once the views that follow them arrive.
class PrintingPlayers : public game_boundary::PlayersEndOutbound {
 public:
  // Name, target and time of the next submission, for the trace
  void name(std::string order_name, Point target, int at_ms) {
    order_name_ = std::move(order_name);
    target_ = target;
    at_ms_ = at_ms;
  }

  void startRejected(const game_boundary::StartRejected& /* rejected */) override {
    std::cout << "  start rejected: a game already started\n";
  }

  void submissionOutcome(const game_boundary::SubmissionOutcome& outcome) override {
    if (std::holds_alternative<game_boundary::SubmissionRejected>(outcome)) {
      std::cout << "  A's order " << order_name_ << " to " << point(target_) << " is rejected: the game is over\n";
    }
  }

  void orderSubmitted(const OrderSubmitted& submitted) override {
    std::cout << "  " << at_ms_ << " ms  A selects " << order_name_ << " to " << point(target_) << " in step "
              << submitted.submitted_at.value << "\n";
    names_[{submitted.player, submitted.order}] = order_name_;
  }

  void stepReported(const game_boundary::StepReport& step) override {
    for (const ActivationOutcome& outcome : step.activations) {
      std::cout << "  " << timeOf(step) << " ms  " << describe(names_, outcome) << "\n";
    }
    for (const ArenaEvent& event : step.events) {
      std::cout << "  " << timeOf(step) << " ms  " << describe(names_, event) << "\n";
    }
    for (const OrderTransmitted& transmitted : step.transmissions) {
      std::cout << "  " << timeOf(step) << " ms  A transmits " << names_.at({transmitted.player, transmitted.order})
                << "\n";
    }
    stepped_ = true;
  }

  void ordersEndedWithRound(const ArenaEvents& events) override {
    for (const ArenaEvent& event : events) {
      std::cout << "  end of game  " << describe(names_, event) << "\n";
    }
  }

  void concluded(const game_authority::GameConcluded& /* concluded */) override { std::cout << "  game over\n"; }

  void viewsUpdated(const game_boundary::Views& views) override {
    step_interval_ = views.step_interval;
    if (stepped_) {
      const game_authority::PlayerScore& score = views.game.players.at(PLAYER_A);
      std::cout << "            A: banked " << score.banked << ", held " << score.held << ", chain " << score.chain
                << " (next handover +" << score.next_handover << ")\n";
      stepped_ = false;
    }
  }

 private:
  // Time at which a reported step ran, in milliseconds since round start, from the step interval in the views
  long long timeOf(const game_boundary::StepReport& step) const { return (step_interval_ * step.step.value).count(); }

  std::string order_name_;
  Point target_{0, 0};
  int at_ms_{0};
  OrderNames names_;
  bool stepped_{false};
  std::chrono::milliseconds step_interval_{0};  // Step interval from the views, reported once the game started
};

struct PlannedSubmission {
  int at_ms;
  Point target;
  std::string name;
};

// F collects two foods on its way; when its successors are submitted and sent decides whether F's +20
// counts and whether completions hand over
void runHistory(const std::string& title, const std::vector<PlannedSubmission>& submissions, int until_ms) {
  std::cout << "\n" << title << "\n";
  PrintingPlayers players;
  app::DomainApplication game{players, makeCornerRandom()};
  app::TestAccess::startWithSetup(game, makeBoard());
  int now_ms = 0;
  for (const PlannedSubmission& submission : submissions) {
    game.elapse(app::TimeElapsed{milliseconds{submission.at_ms - now_ms}});
    players.name(submission.name, submission.target, submission.at_ms);
    game.submit(app::Submit{PLAYER_A, submission.target});
    now_ms = submission.at_ms;
  }
  game.elapse(app::TimeElapsed{milliseconds{until_ms - now_ms}});
}

}  // namespace

int main() {
  std::cout << "Orbital Orders - Domain Application traces\n"
               "Steps every 200 ms. A submission selects an order; a new submission replaces a\n"
               "selection not yet sent. The orbital link offers transmission opportunities at 0.2 s,\n"
               "then 4 s apart, shrinking to 0.6 s by the end of the game; there the selection is\n"
               "sent and activates at the next step. A sent order cannot be recalled.\n"
               "Food +10 during an order is held until the order completes, and is lost if a\n"
               "replacement activates first; food without an order banks +5 at once.\n"
               "A successor chosen after its predecessor activated, sent at the first opportunity\n"
               "at or after the predecessor's completion and replacing no earlier selection, earns a\n"
               "handover bonus when it takes over, held like food until it completes: +10 for the\n"
               "first of a chain, doubling for each consecutive one.\n";

  // F to (19,2) arrives at 3400 ms; the next opportunity comes at 4200 ms
  const PlannedSubmission f{0, Point{19, 2}, "F"};
  // F to (2,5) takes the long way round the wrapping row 2 and would arrive at 4600 ms
  const PlannedSubmission long_f{0, Point{2, 5}, "F"};
  runHistory("History 1: G is submitted before F completes and sent after it - F's +20 is banked, G hands over",
             {f, PlannedSubmission{3000, Point{4, 6}, "G"}},
             5400);
  runHistory("History 2: G is sent before F completes - G interrupts F, F's +20 is lost",
             {long_f, PlannedSubmission{1000, Point{10, 6}, "G"}},
             4400);
  runHistory("History 3: G is submitted after the opportunity that followed F's completion - no handover",
             {f, PlannedSubmission{4200, Point{4, 6}, "G"}},
             9400);
  runHistory("History 4: H is submitted before G completes - the second handover doubles the bonus",
             {f, PlannedSubmission{3000, Point{4, 6}, "G"}, PlannedSubmission{4600, Point{4, 5}, "H"}},
             10000);
  runHistory("History 5: G is replaced by G' before it is sent - F's +20 is banked, but G' does not hand over",
             {f, PlannedSubmission{3000, Point{4, 8}, "G"}, PlannedSubmission{3100, Point{4, 6}, "G'"}},
             5400);
  runHistory("History 6: G is submitted after F completed, before the next opportunity - G still hands over",
             {f, PlannedSubmission{3600, Point{4, 6}, "G"}},
             5400);
  return 0;
}
