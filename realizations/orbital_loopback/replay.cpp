#include "orbital_loopback/replay.hpp"

#include <algorithm>
#include <map>

#include "orbital/orbital_game_domain_application.hpp"
#include "orbital/pacing_policy.hpp"

namespace snake {
namespace orbital {
namespace loopback {

namespace {

using game_boundary::StepReport;

// The canonical players' side: keeps what the Domain Application reports
class CanonicalRecorder : public game_boundary::PlayersEndOutbound {
 public:
  void startRejected(const game_boundary::StartRejected& /*rejected*/) override {}
  void submissionOutcome(const game_boundary::SubmissionOutcome& /*outcome*/) override {}
  void orderSubmitted(const OrderSubmitted& submitted) override { history.submissions.push_back(submitted); }
  void stepReported(const StepReport& step) override { history.steps.push_back(step); }
  void ordersEndedWithRound(const ArenaEvents& events) override {
    history.end_events.insert(history.end_events.end(), events.begin(), events.end());
  }
  void concluded(const game_authority::GameConcluded& concluded) override { history.conclusion = concluded; }
  void viewsUpdated(const game_boundary::Views& views) override { history.final_views = views; }

  CanonicalHistory history;
};

// Lines of one section, compared line by line; the first difference is reported
void compareSection(const std::string& section,
                    const std::vector<std::string>& realization,
                    const std::vector<std::string>& canonical,
                    Comparison& comparison) {
  const std::size_t common = std::min(realization.size(), canonical.size());
  for (std::size_t i = 0; i < common; ++i) {
    if (realization[i] != canonical[i]) {
      comparison.divergences.push_back(section + ": realization \"" + realization[i] + "\", canonical \"" +
                                       canonical[i] + "\"");
      return;
    }
  }
  if (realization.size() > common) {
    comparison.divergences.push_back(section + ": realization has more, from \"" + realization[common] + "\"");
  } else if (canonical.size() > common) {
    comparison.divergences.push_back(section + ": canonical has more, from \"" + canonical[common] + "\"");
  }
}

// Items concerning one player keep their order; across players the order carries no meaning
template <typename Item, typename PlayerOf>
std::vector<std::string> byPlayer(std::vector<Item> items, PlayerOf player_of) {
  std::stable_sort(
      items.begin(), items.end(), [&](const Item& a, const Item& b) { return player_of(a) < player_of(b); });
  std::vector<std::string> lines;
  for (const Item& item : items) {
    lines.push_back(describe(item));
  }
  return lines;
}

std::vector<std::string> stepLines(const std::vector<StepReport>& steps) {
  std::vector<std::string> lines;
  for (const StepReport& step : steps) {
    const std::string prefix = "step " + std::to_string(step.step.value) + ": ";
    for (const ActivationOutcome& outcome : step.activations) {
      lines.push_back(prefix + describe(outcome));
    }
    for (const ArenaEvent& event : step.events) {
      lines.push_back(prefix + describe(event));
    }
    lines.push_back(prefix + "done");
  }
  return lines;
}

std::vector<std::string> handoverLines(const std::map<StepId, std::vector<game_authority::HandoverGranted>>& by_step) {
  std::vector<std::string> lines;
  for (const auto& [step, handovers] : by_step) {
    for (const std::string& line :
         byPlayer(handovers, [](const game_authority::HandoverGranted& handover) { return handover.player; })) {
      lines.push_back("step " + std::to_string(step.value) + ": " + line);
    }
  }
  return lines;
}

std::vector<std::string> endLines(const ArenaEvents& end_events,
                                  const std::optional<game_authority::GameConcluded>& conclusion) {
  std::vector<std::string> lines;
  for (const ArenaEvent& event : end_events) {
    lines.push_back(describe(event));
  }
  lines.push_back(conclusion ? describe(*conclusion) : "not concluded");
  return lines;
}

}  // namespace

CanonicalHistory replayInDomainApplication(const HostRecord& record) {
  CanonicalRecorder recorder;
  orbital_game_domain_application::DomainApplication game{recorder, seededRandom(record.seed)};
  game.start(game_boundary::Start{record.round});

  std::map<StepId, std::vector<const platform::ForwardedSubmission*>> submissions;
  for (const platform::ForwardedSubmission& submission : record.submissions) {
    submissions[submission.submitted.submitted_at].push_back(&submission);
  }
  for (StepId step{0}; !recorder.history.conclusion && step <= pacing_policy::LAST_STEP;
       step = StepId{step.value + 1}) {
    for (const platform::ForwardedSubmission* submission : submissions[step]) {
      game.submit(game_boundary::Submit{submission->order.player, submission->order.target});
    }
    if (recorder.history.final_views) {
      recorder.history.checkpoints.insert_or_assign(step, *recorder.history.final_views);
    }
    game.elapse(orbital_game_domain_application::TimeElapsed{pacing_policy::STEP_INTERVAL});
  }
  return std::move(recorder.history);
}

Comparison compare(const HostRecord& record, const CanonicalHistory& canonical) {
  Comparison comparison;

  std::vector<OrderSubmitted> submitted;
  for (const platform::ForwardedSubmission& submission : record.submissions) {
    submitted.push_back(submission.submitted);
  }
  compareSection("submissions",
                 byPlayer(submitted, [](const OrderSubmitted& item) { return item.player; }),
                 byPlayer(canonical.submissions, [](const OrderSubmitted& item) { return item.player; }),
                 comparison);

  // Transmissions and the handovers they granted belong to the step of their opportunity
  std::map<StepId, std::vector<OrderTransmitted>> realization_transmissions;
  std::map<StepId, std::vector<game_authority::HandoverGranted>> realization_handovers;
  for (std::size_t i = 0; i < record.transmissions.size(); ++i) {
    const OrderTransmitted& transmitted = record.transmissions[i].transmitted;
    realization_transmissions[transmitted.transmitted_at].push_back(transmitted);
  }
  for (const StepReport& step : record.steps) {
    auto& handovers = realization_handovers[step.step];
    handovers.insert(handovers.end(), step.handovers.begin(), step.handovers.end());
  }
  for (std::size_t i = 0; i < record.transmissions.size(); ++i) {
    auto& handovers = realization_handovers[record.transmissions[i].transmitted.transmitted_at];
    handovers.insert(handovers.end(), record.transmission_handovers[i].begin(), record.transmission_handovers[i].end());
  }
  std::map<StepId, std::vector<OrderTransmitted>> canonical_transmissions;
  std::map<StepId, std::vector<game_authority::HandoverGranted>> canonical_handovers;
  for (const StepReport& step : canonical.steps) {
    if (!step.transmissions.empty()) {
      canonical_transmissions[step.step] = step.transmissions;
    }
    if (!step.handovers.empty()) {
      canonical_handovers[step.step] = step.handovers;
    }
  }
  auto transmissionLines = [](const std::map<StepId, std::vector<OrderTransmitted>>& by_step) {
    std::vector<std::string> lines;
    for (const auto& [step, transmissions] : by_step) {
      for (const std::string& line :
           byPlayer(transmissions, [](const OrderTransmitted& item) { return item.player; })) {
        lines.push_back(line);
      }
    }
    return lines;
  };
  compareSection("transmissions",
                 transmissionLines(realization_transmissions),
                 transmissionLines(canonical_transmissions),
                 comparison);
  compareSection("steps", stepLines(record.steps), stepLines(canonical.steps), comparison);
  compareSection("handovers", handoverLines(realization_handovers), handoverLines(canonical_handovers), comparison);
  compareSection("end",
                 endLines(record.end_events, record.conclusion),
                 endLines(canonical.end_events, canonical.conclusion),
                 comparison);

  for (const auto& late : record.deviations) {
    comparison.divergences.push_back("deviation: " + describe(late));
  }

  for (const auto& [step, views] : record.checkpoints) {
    auto canonical_views = canonical.checkpoints.find(step);
    if (canonical_views == canonical.checkpoints.end()) {
      comparison.divergences.push_back("views after step " + std::to_string(step.value) + ": canonical has none");
      continue;
    }
    compareSection("views after step " + std::to_string(step.value),
                   describe(views),
                   describe(canonical_views->second),
                   comparison);
  }
  if (canonical.checkpoints.size() > record.checkpoints.size()) {
    comparison.divergences.push_back("views: canonical has " + std::to_string(canonical.checkpoints.size()) +
                                     " checkpoints, realization " + std::to_string(record.checkpoints.size()));
  }
  if (record.final_views && canonical.final_views) {
    compareSection("final views", describe(*record.final_views), describe(*canonical.final_views), comparison);
  }
  return comparison;
}

}  // namespace loopback
}  // namespace orbital
}  // namespace snake
