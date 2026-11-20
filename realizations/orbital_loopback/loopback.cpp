#include "orbital_loopback/loopback.hpp"

#include <algorithm>
#include <map>
#include <memory>
#include <queue>
#include <random>
#include <utility>

#include "orbital/board_setup_policy.hpp"
#include "orbital/pacing_policy.hpp"
#include "orbital_loopback/host_composition.hpp"
#include "orbital_loopback/phone_composition.hpp"

namespace snake {
namespace orbital {
namespace loopback {

namespace {

using std::chrono::milliseconds;

// What happens first among events at the same time
enum class Priority { HOST_STEP = 0, DELIVERY = 1, DEVICE_TIMER = 2, PLAYER = 3 };

struct Event {
  milliseconds at;
  Priority priority;
  long sequence;
  std::function<void()> action;

  bool operator>(const Event& other) const {
    if (at != other.at) {
      return at > other.at;
    }
    if (priority != other.priority) {
      return priority > other.priority;
    }
    return sequence > other.sequence;
  }
};

class Simulation : public HostOutbound {
 public:
  explicit Simulation(const LoopbackGame& game) : game_{game}, host_{*this, seededRandom(game.seed)} {
    record_.round = game.round;
    record_.seed = game.seed;
    for (const PlayerId& player : {PlayerId{PLAYER_A}, PlayerId{PLAYER_B}}) {
      auto uplink = std::make_unique<Uplink>(*this, player);
      phones_.emplace(player, std::make_unique<PhoneComposition>(player, *uplink));
      uplinks_.push_back(std::move(uplink));
    }
  }

  HostRecord run() {
    host_.start(game_.round, now_);
    schedule(now_ + pacing_policy::STEP_INTERVAL, Priority::HOST_STEP, [this] { hostStep(); });
    for (const ScriptedSubmission& submission : game_.script) {
      schedule(submission.at, Priority::PLAYER, [this, submission] {
        phones_.at(submission.player)->submit(submission.target, now_);
      });
    }
    while (!events_.empty()) {
      Event next = events_.top();
      events_.pop();
      now_ = next.at;
      next.action();
    }
    record_.final_views = last_views_;
    return std::move(record_);
  }

  // Host outbound: record, and send to the devices what they need
  void roundStarted(const RoundStarted& started) override { broadcast(started); }
  void stepReported(const game_boundary::StepReport& step) override {
    if (last_views_) {
      record_.checkpoints.insert_or_assign(StepId{step.step.value - 1}, *last_views_);
    }
    record_.steps.push_back(step);
    broadcast(step);
  }
  void ordersEndedWithRound(const ArenaEvents& events) override {
    record_.end_events.insert(record_.end_events.end(), events.begin(), events.end());
    broadcast(events);
  }
  void concluded(const game_authority::GameConcluded& concluded) override {
    record_.conclusion = concluded;
    broadcast(concluded);
  }
  void submissionReceived(const platform::ForwardedSubmission& submission) override {
    record_.submissions.push_back(submission);
  }
  void transmissionReceived(const platform::ForwardedTransmission& transmission,
                            const std::vector<game_authority::HandoverGranted>& handovers) override {
    record_.transmissions.push_back(transmission);
    record_.transmission_handovers.push_back(handovers);
  }
  void lateTransmission(const platform::order_replica_authority::LateTransmission& late) override {
    record_.deviations.push_back(late);
  }
  void viewsUpdated(const game_boundary::Views& views) override { last_views_ = views; }

 private:
  class Uplink : public PhoneUplink {
   public:
    Uplink(Simulation& simulation, PlayerId player) : simulation_{simulation}, player_{std::move(player)} {}
    void forward(const platform::ForwardedSubmission& submission) override {
      simulation_.send(LinkMessage{LinkMessage::Direction::UPLINK, player_, submission});
    }
    void forward(const platform::ForwardedTransmission& transmission) override {
      simulation_.send(LinkMessage{LinkMessage::Direction::UPLINK, player_, transmission});
    }

   private:
    Simulation& simulation_;
    PlayerId player_;
  };

  template <typename Message>
  void broadcast(const Message& message) {
    for (const auto& [player, phone] : phones_) {
      send(LinkMessage{LinkMessage::Direction::DOWNLINK, player, message});
    }
  }

  // Deliver after the hook's delay, but never before an earlier message of the same link
  void send(LinkMessage message) {
    const milliseconds delay = game_.delay ? game_.delay(message) : milliseconds{0};
    milliseconds& link_free = link_free_[{message.direction, message.player}];
    const milliseconds arrival = std::max(now_ + delay, link_free);
    link_free = arrival;
    schedule(arrival, Priority::DELIVERY, [this, message = std::move(message)] { deliver(message); });
  }

  void deliver(const LinkMessage& message) {
    if (message.direction == LinkMessage::Direction::UPLINK) {
      if (auto submission = std::get_if<platform::ForwardedSubmission>(&message.message)) {
        host_.receive(*submission);
      } else if (auto transmission = std::get_if<platform::ForwardedTransmission>(&message.message)) {
        host_.receive(*transmission);
      }
      return;
    }
    PhoneComposition& phone = *phones_.at(message.player);
    if (auto started = std::get_if<RoundStarted>(&message.message)) {
      phone.roundStarted(*started);
      for (StepId opportunity : started->schedule) {
        schedule(started->t0 + started->step_interval * opportunity.value,
                 Priority::DEVICE_TIMER,
                 [&phone, opportunity] { phone.fireOpportunity(opportunity); });
      }
    } else if (auto step = std::get_if<game_boundary::StepReport>(&message.message)) {
      phone.stepReported(*step);
    } else if (auto events = std::get_if<ArenaEvents>(&message.message)) {
      phone.ordersEndedWithRound(*events);
    } else if (auto concluded = std::get_if<game_authority::GameConcluded>(&message.message)) {
      phone.concluded(*concluded);
    }
  }

  void hostStep() {
    host_.runStep();
    if (host_.isRunning()) {
      schedule(now_ + pacing_policy::STEP_INTERVAL, Priority::HOST_STEP, [this] { hostStep(); });
    }
  }

  void schedule(milliseconds at, Priority priority, std::function<void()> action) {
    events_.push(Event{at, priority, next_sequence_++, std::move(action)});
  }

  const LoopbackGame& game_;
  HostComposition host_;
  std::map<PlayerId, std::unique_ptr<PhoneComposition>> phones_;
  std::vector<std::unique_ptr<Uplink>> uplinks_;
  std::priority_queue<Event, std::vector<Event>, std::greater<Event>> events_;
  std::map<std::pair<LinkMessage::Direction, PlayerId>, milliseconds> link_free_;
  milliseconds now_{0};
  long next_sequence_{0};
  HostRecord record_;
  std::optional<game_boundary::Views> last_views_;
};

}  // namespace

std::vector<ScriptedSubmission> randomScript(unsigned seed, std::chrono::milliseconds max_gap) {
  std::mt19937 engine{seed};
  std::uniform_int_distribution<int> coordinate{-1, board_setup_policy::BOARD.width};
  std::uniform_int_distribution<milliseconds::rep> gap{0, max_gap.count()};
  std::bernoulli_distribution on_boundary{0.2};
  std::vector<ScriptedSubmission> script;
  for (const PlayerId& player : {PlayerId{PLAYER_A}, PlayerId{PLAYER_B}}) {
    milliseconds at{0};
    while (true) {
      at += milliseconds{gap(engine)};
      if (on_boundary(engine)) {
        at -= at % pacing_policy::STEP_INTERVAL;
      }
      if (at > pacing_policy::GAME_DURATION) {
        break;
      }
      script.push_back(ScriptedSubmission{at, player, Point{coordinate(engine), coordinate(engine)}});
    }
  }
  return script;
}

HostRecord playOverLoopback(const LoopbackGame& game) { return Simulation{game}.run(); }

}  // namespace loopback
}  // namespace orbital
}  // namespace snake
