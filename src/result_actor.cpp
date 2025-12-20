#include "snake/result_actor.hpp"

#include <iostream>

namespace snake {

ResultActor::ResultActor(asio::io_context& io) : strand_(asio::make_strand(io)) {}

void ResultActor::post(MsgResult msg) {
  asio::post(strand_, [self = shared_from_this(), msg] { self->onResult(msg); });
}

void ResultActor::onResult(const MsgResult& msg) { std::cout << "Result: " << msg.value << "\n"; }

}  // namespace snake
