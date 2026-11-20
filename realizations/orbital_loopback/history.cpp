#include "orbital_loopback/history.hpp"

#include <memory>
#include <random>
#include <sstream>
#include <variant>

#include "common/snake_model.hpp"

namespace snake {
namespace orbital {
namespace loopback {

namespace {

std::string pointText(Point point) { return "(" + std::to_string(point.x) + "," + std::to_string(point.y) + ")"; }

std::string orderText(const PlayerId& player, OrderNumber order) { return player + " #" + std::to_string(order.value); }

std::string optionalNumber(const std::optional<OrderNumber>& order) {
  return order ? "#" + std::to_string(order->value) : "-";
}

std::string optionalStep(const std::optional<StepId>& step) { return step ? std::to_string(step->value) : "-"; }

const char* directionText(Direction direction) {
  switch (direction) {
    case Direction::UP:
      return "U";
    case Direction::DOWN:
      return "D";
    case Direction::LEFT:
      return "L";
    case Direction::RIGHT:
      return "R";
  }
  return "?";
}

std::string routeText(const std::vector<Direction>& route) {
  std::string text;
  for (Direction direction : route) {
    text += directionText(direction);
  }
  return text.empty() ? "-" : text;
}

const char* statusText(order_authority::Status status) {
  using order_authority::Status;
  switch (status) {
    case Status::SELECTED:
      return "selected";
    case Status::REPLACED:
      return "replaced";
    case Status::TRANSMITTED:
      return "transmitted";
    case Status::CANCELLED:
      return "cancelled";
    case Status::REJECTED:
      return "rejected";
    case Status::EXECUTING:
      return "executing";
    case Status::COMPLETED:
      return "completed";
    case Status::INTERRUPTED:
      return "interrupted";
    case Status::FAILED:
      return "failed";
    case Status::ENDED_BY_ROUND:
      return "ended by round";
  }
  return "?";
}

std::string recordText(const order_authority::OrderRecord& record) {
  std::string text = describe(record.order) + " submitted " + std::to_string(record.submitted_at.value) +
                     " transmitted " + optionalStep(record.transmitted_at) + " replaced " +
                     optionalNumber(record.replaced) + " " + statusText(record.status);
  if (record.rejection) {
    text += " rejection " + std::to_string(static_cast<int>(*record.rejection));
  }
  return text;
}

std::string optionalRecordId(const std::optional<order_authority::OrderRecord>& record) {
  return record ? optionalNumber(record->order.id) : "-";
}

struct EventText {
  std::string operator()(const OrderEnded& ended) const {
    return "ended " + orderText(ended.player, ended.order) + " reason " +
           std::to_string(static_cast<int>(ended.reason)) + " replacement " + optionalNumber(ended.replacement);
  }
  std::string operator()(const FoodCollected& collected) const {
    return "food " + collected.player + " at " + pointText(collected.position) + " during " +
           optionalNumber(collected.during);
  }
  std::string operator()(const Bitten& bitten) const { return "bitten " + bitten.victim + " by " + bitten.biter; }
  std::string operator()(const SelfBitten& bitten) const { return "self-bitten " + bitten.player; }
  std::string operator()(const MutualBite& bite) const { return "mutual bite " + bite.first + " " + bite.second; }
};

struct OutcomeText {
  std::string operator()(const OrderActivated& activated) const {
    return "activated " + orderText(activated.player, activated.order) + " replacing " +
           optionalNumber(activated.replaced) + " route " + routeText(activated.route);
  }
  std::string operator()(const OrderRejected& rejected) const {
    return "rejected " + orderText(rejected.player, rejected.order) + " reason " +
           std::to_string(static_cast<int>(rejected.reason));
  }
};

}  // namespace

RandomIntGeneratorFn seededRandom(unsigned seed) {
  auto engine = std::make_shared<std::mt19937>(seed);
  return [engine](int min, int max) { return std::uniform_int_distribution<int>{min, max}(*engine); };
}

std::string describe(const Order& order) {
  return orderText(order.player, order.id) + " to " + pointText(order.target) + " round " +
         std::to_string(order.round.value);
}

std::string describe(const OrderSubmitted& submitted) {
  return "submitted " + orderText(submitted.player, submitted.order) + " in step " +
         std::to_string(submitted.submitted_at.value) + " replacing " + optionalNumber(submitted.replaced);
}

std::string describe(const OrderTransmitted& transmitted) {
  return "transmitted " + orderText(transmitted.player, transmitted.order) + " submitted " +
         std::to_string(transmitted.submitted_at.value) + " at " + std::to_string(transmitted.transmitted_at.value) +
         " replacing " + optionalNumber(transmitted.replaced);
}

std::string describe(const ActivationOutcome& outcome) { return std::visit(OutcomeText{}, outcome); }

std::string describe(const ArenaEvent& event) { return std::visit(EventText{}, event); }

std::string describe(const game_authority::HandoverGranted& handover) {
  return "handover " + orderText(handover.player, handover.order) + " chain " + std::to_string(handover.chain) +
         " bonus " + std::to_string(handover.bonus);
}

std::string describe(const game_authority::GameConcluded& concluded) {
  return "concluded round " + std::to_string(concluded.round.value) + " winner " +
         (concluded.result.winner ? *concluded.result.winner : std::string{"draw"});
}

std::string describe(const platform::order_replica_authority::LateTransmission& late) {
  return "late transmission " + orderText(late.player, late.order) + " at " +
         std::to_string(late.transmitted_at.value) + ": scheduled for step " +
         std::to_string(late.scheduled_step.value) + ", took part from step " + std::to_string(late.first_step.value);
}

std::vector<std::string> describe(const game_boundary::Views& views) {
  std::vector<std::string> lines;
  const arena_authority::View& arena = views.arena;
  lines.push_back("arena step " + std::to_string(arena.step.value) + (arena.ended ? " ended" : "") + " board " +
                  std::to_string(arena.board.width) + "x" + std::to_string(arena.board.height));
  for (const auto& [player, snake] : arena.snakes) {
    std::string body = pointText(snake_model::head(snake));
    for (Point point : snake_model::tail(snake)) {
      body += pointText(point);
    }
    lines.push_back("snake " + player + (snake_model::alive(snake) ? " alive " : " dead ") + body);
  }
  std::string food = "food";
  for (Point point : arena.food) {
    food += " " + pointText(point);
  }
  lines.push_back(food);
  for (const auto& [player, active] : arena.active) {
    lines.push_back("active " + orderText(player, active.id) + " to " + pointText(active.target) + " route " +
                    routeText(active.remaining_route));
  }
  for (const order_authority::OrderRecord& record : views.orders.orders) {
    lines.push_back("order " + recordText(record));
  }
  for (const auto& [player, situation] : views.orders.players) {
    lines.push_back("orders of " + player + ": selected " + optionalRecordId(situation.selected) + " transmitted " +
                    optionalRecordId(situation.transmitted) + " executing " + optionalRecordId(situation.executing));
  }
  for (const auto& [player, score] : views.game.players) {
    lines.push_back("score " + player + ": banked " + std::to_string(score.banked) + " held " +
                    std::to_string(score.held) + " chain " + std::to_string(score.chain) + " next handover " +
                    std::to_string(score.next_handover));
  }
  lines.push_back("remaining steps " + std::to_string(views.game.remaining_steps) +
                  (views.game.result
                       ? " result " + (views.game.result->winner ? *views.game.result->winner : std::string{"draw"})
                       : std::string{}));
  lines.push_back("step interval " + std::to_string(views.step_interval.count()) + " ms, last step " +
                  std::to_string(views.last_step.value) + ", opportunities " +
                  std::to_string(views.transmission_schedule.size()));
  for (const auto& [player, step] : views.expected_completions) {
    lines.push_back("expected completion " + player + " step " + std::to_string(step.value));
  }
  for (const auto& [player, route] : views.selected_routes) {
    lines.push_back("selected route " + player + " " + routeText(route));
  }
  for (const auto& [player, route] : views.transmitted_routes) {
    lines.push_back("transmitted route " + player + " " + routeText(route));
  }
  for (const auto& [player, prospect] : views.handover_prospects) {
    lines.push_back("handover prospect " + player + " " + std::to_string(static_cast<int>(prospect)));
  }
  return lines;
}

std::vector<std::string> describeTimeline(const HostRecord& record) {
  std::vector<std::string> lines;
  lines.push_back("round " + std::to_string(record.round.value) + " seed " + std::to_string(record.seed));
  for (const platform::ForwardedSubmission& submission : record.submissions) {
    lines.push_back("  " + describe(submission.submitted) + " to " + pointText(submission.order.target));
  }
  for (std::size_t i = 0; i < record.transmissions.size(); ++i) {
    lines.push_back("  " + describe(record.transmissions[i].transmitted));
    for (const game_authority::HandoverGranted& handover : record.transmission_handovers[i]) {
      lines.push_back("    " + describe(handover));
    }
  }
  for (const game_boundary::StepReport& step : record.steps) {
    if (step.activations.empty() && step.events.empty() && step.handovers.empty()) {
      continue;
    }
    lines.push_back("  step " + std::to_string(step.step.value));
    for (const ActivationOutcome& outcome : step.activations) {
      lines.push_back("    " + describe(outcome));
    }
    for (const ArenaEvent& event : step.events) {
      lines.push_back("    " + describe(event));
    }
    for (const game_authority::HandoverGranted& handover : step.handovers) {
      lines.push_back("    " + describe(handover));
    }
  }
  for (const ArenaEvent& event : record.end_events) {
    lines.push_back("  at the end: " + describe(event));
  }
  if (record.conclusion) {
    lines.push_back("  " + describe(*record.conclusion));
  }
  for (const auto& late : record.deviations) {
    lines.push_back("  deviation: " + describe(late));
  }
  if (record.final_views) {
    for (const auto& [player, score] : record.final_views->game.players) {
      lines.push_back("  final score " + player + ": " + std::to_string(score.banked));
    }
  }
  return lines;
}

}  // namespace loopback
}  // namespace orbital
}  // namespace snake
