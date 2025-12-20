#include "snake/calculator_actor.hpp"

namespace snake {

CalculatorActor::CalculatorActor(asio::io_context& io, std::shared_ptr<ResultSink> result_sink)
    : strand_(asio::make_strand(io)), result_sink_(std::move(result_sink)) {}

void CalculatorActor::post(MsgAdd msg) {
  asio::post(strand_, [self = shared_from_this(), msg] { self->onAdd(msg); });
}

void CalculatorActor::onAdd(const MsgAdd& msg) { result_sink_->post(MsgResult{msg.a + msg.b}); }

}  // namespace snake
