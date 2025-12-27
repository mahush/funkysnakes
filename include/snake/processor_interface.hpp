#ifndef SNAKE_PROCESSOR_INTERFACE_HPP
#define SNAKE_PROCESSOR_INTERFACE_HPP

namespace snake {

// Interface for actors that process events from event sources (topics, timers, etc.)
// Event sources use weak_ptr to this interface to notify actors when events occur
class ProcessorInterface {
 public:
  virtual ~ProcessorInterface() = default;

  // Called when subscribed event sources have events available
  // Actor should pull events from its subscriptions/timers in desired order
  virtual void processMessages() = 0;
};

}  // namespace snake

#endif  // SNAKE_PROCESSOR_INTERFACE_HPP
