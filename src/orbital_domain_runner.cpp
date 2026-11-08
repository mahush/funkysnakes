// Orbital Orders domain runner: plays the defining histories through the Domain Application and
// prints what happened, step by step: commitments (Order Authority), activations and endings (Arena Authority)
// and the resulting score and handover chain (Game Authority). No actors, timers, networking or rendering.

#include <chrono>
#include <iostream>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "orbital/orbital_game_domain_application.hpp"

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

std::string order(OrderId id) {
  switch (id.value) {
    case 1:
      return "F";
    case 2:
      return "G";
    case 3:
      return "H";
    default:
      return "#" + std::to_string(id.value);
  }
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

std::string describe(const ActivationOutcome& outcome) {
  if (const auto* activated = std::get_if<OrderActivated>(&outcome)) {
    return order(activated->order) + " activates";
  }
  return order(std::get<OrderRejected>(outcome).order) + " rejected";
}

std::string describe(const ArenaEvent& event) {
  return std::visit(
      [](const auto& item) -> std::string {
        using TEvent = std::decay_t<decltype(item)>;
        if constexpr (std::is_same_v<TEvent, OrderEnded>) {
          return order(item.order) + " " + endReason(item.reason);
        } else if constexpr (std::is_same_v<TEvent, FoodCollected>) {
          return std::string{item.player == PLAYER_A ? "A" : "B"} + " eats at " + point(item.position) +
                 (item.during ? " during " + order(*item.during) : " (no order)");
        } else {
          return "collision";
        }
      },
      event);
}

// The players' side of the runner: prints what A learns, step by step. A commitment and the score
// after a run of steps are printed once the views that follow them arrive.
class PrintingPlayers : public game_boundary::PlayersEndOutbound {
 public:
  // Name and target of the next submission, for the trace
  void name(std::string order_name, Point target) {
    order_name_ = std::move(order_name);
    target_ = target;
  }

  void startRejected(const game_boundary::StartRejected& /* rejected */) override {
    std::cout << "  start rejected: a game already started\n";
  }

  void submissionOutcome(const game_boundary::SubmissionOutcome& outcome) override {
    if (std::holds_alternative<game_boundary::SubmissionRejected>(outcome)) {
      std::cout << "  A's order " << order_name_ << " to " << point(target_) << " is rejected: the game is over\n";
    }
  }

  void orderCommitted(const OrderCommitted& committed) override { committed_ = committed; }

  void stepReported(const game_boundary::StepReport& step) override {
    for (const ActivationOutcome& outcome : step.activations) {
      std::cout << "  " << step.at.count() << " ms  " << describe(outcome) << "\n";
    }
    for (const ArenaEvent& event : step.events) {
      std::cout << "  " << step.at.count() << " ms  " << describe(event) << "\n";
    }
    stepped_ = true;
  }

  void ordersEndedWithRound(const ArenaEvents& events) override {
    for (const ArenaEvent& event : events) {
      std::cout << "  end of game  " << describe(event) << "\n";
    }
  }

  void concluded(const game_authority::GameConcluded& /* concluded */) override { std::cout << "  game over\n"; }

  void viewsUpdated(const game_boundary::Views& views) override {
    if (committed_) {
      std::cout << "  " << committed_->committed_at.count() << " ms  A commits " << order_name_ << " to "
                << point(target_) << ", released at " << views.pending_release_times.at(PLAYER_A).count() << " ms\n";
      committed_.reset();
    }
    if (stepped_) {
      const game_authority::PlayerScore& score = views.game.players.at(PLAYER_A);
      std::cout << "            A: banked " << score.banked << ", held " << score.held << ", chain " << score.chain
                << " (next handover +" << score.next_handover << ")\n";
      stepped_ = false;
    }
  }

 private:
  std::string order_name_;
  Point target_{0, 0};
  std::optional<OrderCommitted> committed_;
  bool stepped_{false};
};

struct Commit {
  int at_ms;
  Point target;
  std::string name;
};

// F to (10,2) collects two foods and arrives at 1600 ms; when its successors are committed decides
// whether F's +20 counts and whether completions hand over
void runHistory(const std::string& title, const std::vector<Commit>& commits, int until_ms) {
  std::cout << "\n" << title << "\n";
  PrintingPlayers players;
  app::DomainApplication game{players, makeCornerRandom()};
  game.startWithSetup(makeBoard());
  int now_ms = 0;
  for (const Commit& commit : commits) {
    game.elapse(app::TimeElapsed{milliseconds{commit.at_ms - now_ms}});
    players.name(commit.name, commit.target);
    game.submit(app::Submit{PLAYER_A, commit.target});
    now_ms = commit.at_ms;
  }
  game.elapse(app::TimeElapsed{milliseconds{until_ms - now_ms}});
}

}  // namespace

int main() {
  std::cout << "Orbital Orders - Domain Application traces\n"
               "Steps every 200 ms; orbital delay 0.2-2 s; food +10 during an order is held\n"
               "until the order completes, and is lost if a replacement activates first;\n"
               "food without an order banks +5 at once.\n"
               "An order completing while its successor is already committed and still pending is a\n"
               "handover: +10 for the first of a chain, doubling for each consecutive one.\n";

  const Commit f{0, Point{10, 2}, "F"};
  runHistory("History 1: G is committed before F completes - F's +20 is banked and F hands over",
             {f, Commit{1400, Point{10, 6}, "G"}},
             2000);
  runHistory("History 2: G activates before F completes - F's +20 is lost and nothing is handed over",
             {f, Commit{800, Point{10, 6}, "G"}},
             1200);
  runHistory("History 3: G is committed after F completed - F's +20 is banked, but no handover",
             {f, Commit{1600, Point{10, 6}, "G"}},
             2000);
  runHistory("History 4: H is committed before G completes - the second handover doubles the bonus",
             {f, Commit{1400, Point{10, 6}, "G"}, Commit{2200, Point{3, 6}, "H"}},
             2600);
  return 0;
}
