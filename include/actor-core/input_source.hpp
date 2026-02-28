#ifndef ACTOR_CORE_INPUT_SOURCE_HPP
#define ACTOR_CORE_INPUT_SOURCE_HPP

#include <optional>

namespace actor_core {

/**
 * @brief Abstract interface for pull-based input sources
 *
 * Provides a unified interface for any source of input to an actor.
 * All input sources (Subscription, Timer, StdinReader, etc.) implement this contract.
 *
 * Pattern:
 * - Pull-based: actor calls tryTake() to retrieve items
 * - Non-blocking: returns std::nullopt if no items available
 * - Notification: source notifies processor when items become available
 *
 * @tparam T The type of items this source provides
 */
template <typename T>
class InputSource {
 public:
  virtual ~InputSource() = default;

  /**
   * @brief Try to take the next item (non-blocking)
   * @return The next item if available, std::nullopt otherwise
   *
   * This is the unified pull interface - all sources use the same method name.
   */
  virtual std::optional<T> tryTake() = 0;

  /**
   * @brief Check if input items are available
   * @return true if tryTake() would return an item, false otherwise
   */
  virtual bool hasInputItems() const = 0;
};

}  // namespace actor_core

#endif  // ACTOR_CORE_INPUT_SOURCE_HPP
