// Orbital Orders domain runner: plays the defining histories through the Domain Application and
// prints what happened, step by step. No actors, timers, networking or rendering.

#include <chrono>
#include <iostream>
#include <string>
#include <tuple>
#include <type_traits>
#include <variant>
#include <vector>

#include "orbital/orbital_game_domain_application.hpp"

namespace {

using namespace snake;
using namespace snake::orbital;
namespace app = orbital_game_domain_application;
using std::chrono::milliseconds;

// New food always appears in the far corner, so histories are reproducible
RandomIntGeneratorFn cornerRandom() {
  return [](int /* min */, int max) { return max; };
}

// Board 20x10: A in row 2 heading right with food at (5,2) and (7,2); B in row 7 heading right
arena_authority::RoundSetup board() {
  return arena_authority::RoundSetup{RoundId{1},
                                     Board{20, 10},
                                     {{PLAYER_A, arena_authority::SnakeSetup{Point{2, 2}, Direction::RIGHT, 3}},
                                      {PLAYER_B, arena_authority::SnakeSetup{Point{2, 7}, Direction::RIGHT, 3}}},
                                     {Point{5, 2}, Point{7, 2}, Point{15, 9}, Point{16, 9}, Point{17, 9}}};
}

std::string point(Point p) { return "(" + std::to_string(p.x) + "," + std::to_string(p.y) + ")"; }

std::string order(OrderId id) { return id.value == 1 ? "F" : id.value == 2 ? "G" : "#" + std::to_string(id.value); }

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

void printScore(const app::State& state) {
  const game_authority::PlayerScore score = game_authority::view(*state.game).players.at(PLAYER_A);
  std::cout << "            A: credited " << score.credited << ", unbanked " << score.unbanked << "\n";
}

void printSteps(const app::Observations& observations, const app::State& state) {
  for (const game_boundary::StepReport& step : observations.steps) {
    std::vector<std::string> lines;
    for (const ActivationOutcome& outcome : step.activations) {
      lines.push_back(describe(outcome));
    }
    for (const ArenaEvent& event : step.events) {
      lines.push_back(describe(event));
    }
    for (const std::string& line : lines) {
      std::cout << "  " << step.at.count() << " ms  " << line << "\n";
    }
  }
  printScore(state);
}

app::State submit(app::State state, Point target, const std::string& name) {
  auto [next, observations] = app::apply(std::move(state), app::Submit{PLAYER_A, target});
  const auto& accepted = std::get<game_boundary::SubmissionAccepted>(*observations.submission);
  const order_authority::View orders = order_authority::view(*next.orders);
  std::cout << "  " << accepted.issued_at.count() << " ms  A submits " << name << " to " << point(target)
            << ", eligible from " << orders.players.at(PLAYER_A).pending->deadline.count() << " ms\n";
  return next;
}

app::State elapse(app::State state, int ms) {
  auto [next, observations] = app::apply(cornerRandom(), std::move(state), app::TimeElapsed{milliseconds{ms}});
  printSteps(observations, next);
  return next;
}

// F collects two foods while G waits; when G is submitted decides whether F's +20 counts
void runHistory(const std::string& title, int submit_g_at_ms, int until_ms) {
  std::cout << "\n" << title << "\n";
  app::State state = std::get<0>(app::startWithSetup(cornerRandom(), app::initial(), board()));
  state = submit(std::move(state), Point{10, 2}, "F");
  state = elapse(std::move(state), submit_g_at_ms);
  state = submit(std::move(state), Point{10, 6}, "G");
  state = elapse(std::move(state), until_ms - submit_g_at_ms);
}

}  // namespace

int main() {
  std::cout << "Orbital Orders - Domain Application traces\n"
               "Steps every 200 ms; orbital delay 0.4-3 s; food +10 during an order stays unbanked\n"
               "until the order completes, and is lost if a replacement activates first.\n";

  runHistory("History 1: F completes before G activates - F's +20 is banked", 1200, 2000);
  runHistory("History 2: G activates before F completes - F's +20 is lost", 600, 1200);
  return 0;
}
