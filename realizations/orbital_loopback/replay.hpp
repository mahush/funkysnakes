#pragma once

#include <string>
#include <vector>

#include "orbital_loopback/history.hpp"

namespace snake {
namespace orbital {
namespace loopback {

/**
 * Record → replay → compare: conformance of a realization's history with the canonical Domain Application
 *
 * Platform code without meaning. The host's record is replayed into the Domain Application: the same round and
 * seed, each recorded submission submitted in its own step, and time elapsed step by step, so the Domain
 * Application transmits by itself at the opportunities. The two histories are compared at semantic checkpoints:
 * submissions, transmissions, every step's activations and events, the handovers, the end and the conclusion,
 * and the views between every two steps. A late transmission is a divergence, never a tolerated alternative.
 *
 * Replay cannot see a submission classified into the wrong step, since it uses the device's own steps.
 */

/**
 * @brief Replay the host's record into the Domain Application
 */
CanonicalHistory replayInDomainApplication(const HostRecord& record);

/**
 * @brief Where two histories differ
 */
struct Comparison {
  std::vector<std::string> divergences;  // One line per difference, naming the checkpoint; empty if they agree

  bool agrees() const { return divergences.empty(); }
};

/**
 * @brief Compare the host's record with the canonical history of its replay
 */
Comparison compare(const HostRecord& record, const CanonicalHistory& canonical);

}  // namespace loopback
}  // namespace orbital
}  // namespace snake
