#pragma once

#include <optional>
#include <string>
#include <vector>

#include "orbital/game_boundary.hpp"

namespace snake {
namespace orbital {

/**
 * @brief The players' side for tests: records everything the game reports, in order of occurrence
 */
struct RecordingPlayers : game_boundary::PlayersEndOutbound {
  void startRejected(const game_boundary::StartRejected& rejected) override {
    calls.push_back("startRejected");
    start_rejections.push_back(rejected);
  }

  void submissionOutcome(const game_boundary::SubmissionOutcome& outcome) override {
    calls.push_back("submissionOutcome");
    submissions.push_back(outcome);
  }

  void orderCommitted(const OrderCommitted& committed) override {
    calls.push_back("orderCommitted");
    commitments.push_back(committed);
  }

  void stepReported(const game_boundary::StepReport& step) override {
    calls.push_back("stepReported");
    steps.push_back(step);
  }

  void ordersEndedWithRound(const ArenaEvents& events) override {
    calls.push_back("ordersEndedWithRound");
    ended_with_round.push_back(events);
  }

  void concluded(const game_authority::GameConcluded& concluded) override {
    calls.push_back("concluded");
    conclusions.push_back(concluded);
  }

  void viewsUpdated(const game_boundary::Views& updated) override {
    calls.push_back("viewsUpdated");
    views = updated;
  }

  /**
   * @brief Forget everything recorded so far, so that only what follows is observed
   */
  void clear() {
    calls.clear();
    start_rejections.clear();
    submissions.clear();
    commitments.clear();
    steps.clear();
    ended_with_round.clear();
    conclusions.clear();
    views.reset();
  }

  std::vector<std::string> calls;  // Names of the outbound calls, in the order they were made
  std::vector<game_boundary::StartRejected> start_rejections;
  std::vector<game_boundary::SubmissionOutcome> submissions;
  std::vector<OrderCommitted> commitments;
  std::vector<game_boundary::StepReport> steps;
  std::vector<ArenaEvents> ended_with_round;
  std::vector<game_authority::GameConcluded> conclusions;
  std::optional<game_boundary::Views> views;  // The latest views
};

}  // namespace orbital
}  // namespace snake
