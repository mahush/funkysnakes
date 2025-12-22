#ifndef SNAKE_MESSAGE_PROCESSOR_INTERFACE_HPP
#define SNAKE_MESSAGE_PROCESSOR_INTERFACE_HPP

namespace snake {

// Interface for actors that process messages from topics
// Topic uses weak_ptr to this interface to notify actors when messages arrive
class MessageProcessorInterface {
 public:
  virtual ~MessageProcessorInterface() = default;

  // Called when subscribed topics have messages available
  // Actor should pull messages from its subscriptions in desired order
  virtual void processMessages() = 0;
};

}  // namespace snake

#endif  // SNAKE_MESSAGE_PROCESSOR_INTERFACE_HPP
