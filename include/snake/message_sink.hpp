#ifndef SNAKE_MESSAGE_SINK_HPP
#define SNAKE_MESSAGE_SINK_HPP

namespace snake {

// Interface for actors that can receive messages of type Msg
template<typename Msg>
class MessageSink {
 public:
  virtual ~MessageSink() = default;
  virtual void post(Msg msg) = 0;
};

}  // namespace snake

#endif  // SNAKE_MESSAGE_SINK_HPP
