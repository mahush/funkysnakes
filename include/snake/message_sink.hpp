#ifndef SNAKE_MESSAGE_SINK_HPP
#define SNAKE_MESSAGE_SINK_HPP

namespace snake {

// Interface for actors that can receive messages of type Msg
template<typename Msg>
class MessageSink {
 public:
  virtual ~MessageSink() = default;

  // Event handler called when a message is received
  // This is called on the actor's strand by Topic::publish()
  virtual void onEvent(Msg msg) = 0;
};

}  // namespace snake

#endif  // SNAKE_MESSAGE_SINK_HPP
