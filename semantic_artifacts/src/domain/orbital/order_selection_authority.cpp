#include "domain/orbital/order_selection_authority.hpp"

#include <utility>

#include "domain/orbital/transmission_schedule_policy.hpp"

namespace snake {
namespace orbital {
namespace order_selection_authority {

State startRound(RoundId round) {
  State state;
  state.round_ = round;
  return state;
}

std::tuple<State, OrderSubmitted> submit(State state, const Submission& submission) {
  const OrderNumber id{++state.submitted_count_[submission.player]};

  std::optional<OrderNumber> replaced;
  if (auto selected = state.selections_.find(submission.player); selected != state.selections_.end()) {
    replaced = selected->second.order.id;
  }

  state.selections_.insert_or_assign(
      submission.player,
      Selection{Order{state.round_, id, submission.player, submission.target}, submission.submitted_at, replaced});
  const OrderSubmitted submitted{
      state.round_, id, submission.player, submission.target, submission.submitted_at, replaced};
  return {std::move(state), submitted};
}

std::tuple<State, std::vector<OrderTransmitted>> transmit(State state, StepId at) {
  std::vector<OrderTransmitted> transmissions;
  std::map<PlayerId, Selection> still_selected;
  for (auto& [player, selection] : state.selections_) {
    // A selection waits for the opportunity of its submission (Transmission Schedule Policy)
    if (at < transmission_schedule_policy::opportunityOfSubmission(selection.submitted_at)) {
      still_selected.emplace(player, std::move(selection));
      continue;
    }
    transmissions.push_back(OrderTransmitted{state.round_,
                                             selection.order.id,
                                             player,
                                             selection.order.target,
                                             selection.submitted_at,
                                             at,
                                             selection.replaced});
  }
  state.selections_ = std::move(still_selected);
  return {std::move(state), std::move(transmissions)};
}

State cancel(State state) {
  state.selections_.clear();
  return state;
}

View view(const State& state) { return View{state.selections_}; }

}  // namespace order_selection_authority
}  // namespace orbital
}  // namespace snake
