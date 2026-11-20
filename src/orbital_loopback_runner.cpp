// Plays Orbital Orders games over the loopback realization, replays each host record into the Domain Application
// and prints the timeline and the comparison.
//
// Usage: orbital_loopback_runner [seed] [late]
//   seed  Seed of the game and its scripted submissions (default 1)
//   late  Hold player A's first transmission back for one second, to show a deviation and its divergence

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <map>
#include <string>
#include <variant>

#include "orbital_loopback/loopback.hpp"
#include "orbital_loopback/replay.hpp"

namespace {

using namespace snake::orbital;
using namespace snake::orbital::loopback;
using std::chrono::milliseconds;

}  // namespace

int main(int argc, char* argv[]) {
  const unsigned seed = argc > 1 ? static_cast<unsigned>(std::strtoul(argv[1], nullptr, 10)) : 1u;
  const bool late = argc > 2 && std::string{argv[2]} == "late";

  LoopbackGame game{RoundId{1}, seed, randomScript(seed, milliseconds{3000}), {}};
  if (late) {
    game.delay = [held = false](const LinkMessage& message) mutable {
      if (!held && message.direction == LinkMessage::Direction::UPLINK && message.player == snake::PLAYER_A &&
          std::holds_alternative<snake::orbital::platform::ForwardedTransmission>(message.message)) {
        held = true;
        return milliseconds{1000};
      }
      return milliseconds{0};
    };
  }

  const HostRecord record = playOverLoopback(game);
  for (const std::string& line : describeTimeline(record)) {
    std::cout << line << "\n";
  }

  const CanonicalHistory canonical = replayInDomainApplication(record);
  const Comparison comparison = compare(record, canonical);
  std::cout << "\nreplayed into the Domain Application: " << record.steps.size() << " steps, "
            << record.submissions.size() << " submissions, " << record.transmissions.size() << " transmissions, "
            << record.checkpoints.size() << " view checkpoints\n";
  if (comparison.agrees()) {
    std::cout << "identical histories\n";
    return 0;
  }
  std::cout << comparison.divergences.size() << " divergences:\n";
  constexpr std::size_t SHOWN = 20;
  for (std::size_t i = 0; i < comparison.divergences.size() && i < SHOWN; ++i) {
    std::cout << "  " << comparison.divergences[i] << "\n";
  }
  if (comparison.divergences.size() > SHOWN) {
    std::cout << "  ... " << comparison.divergences.size() - SHOWN << " more\n";
  }
  return late ? 0 : 1;
}
