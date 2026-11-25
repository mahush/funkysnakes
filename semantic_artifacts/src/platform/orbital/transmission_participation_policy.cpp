#include "platform/orbital/transmission_participation_policy.hpp"

#include "domain/orbital/transmission_schedule_policy.hpp"

namespace snake {
namespace orbital {
namespace platform {
namespace transmission_participation_policy {

namespace {

using transmission_schedule_policy::scheduledStepOf;

bool isDue(const OrderTransmitted& transmitted, StepId step) {
  return scheduledStepOf(transmitted.transmitted_at) <= step;
}

bool isLate(const OrderTransmitted& transmitted, StepId step) {
  return scheduledStepOf(transmitted.transmitted_at) < step;
}

// The newest due order of the candidate's player, the candidate being due
OrderNumber newestDueOrder(const OrderTransmitted& candidate,
                           const std::vector<OrderTransmitted>& waiting,
                           StepId step) {
  OrderNumber newest = candidate.order;
  for (const OrderTransmitted& other : waiting) {
    if (other.player == candidate.player && isDue(other, step) && newest < other.order) {
      newest = other.order;
    }
  }
  return newest;
}

}  // namespace

Participation participationOf(const OrderTransmitted& candidate,
                              const std::vector<OrderTransmitted>& waiting,
                              StepId step) {
  if (!isDue(candidate, step)) {
    return Waits{};
  }
  if (const OrderNumber newest = newestDueOrder(candidate, waiting, step); newest != candidate.order) {
    return Superseded{newest};
  }
  if (isLate(candidate, step)) {
    return TakesPartLate{};
  }
  return TakesPart{};
}

}  // namespace transmission_participation_policy
}  // namespace platform
}  // namespace orbital
}  // namespace snake
