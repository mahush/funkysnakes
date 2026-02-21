# Actor-Core Framework

A minimal, type-safe actor framework built on standalone Asio for in-process concurrent programming.

## Overview

Actor-Core implements a **pull-based, typed actor model**:
- Actors communicate through typed Topics (pub-sub channels)
- Messages are plain C++ structs with zero serialization overhead
- Each actor runs on its own strand for serialized execution
- Actors control when to process messages (pull-based)
- Automatic memory management with proper cleanup without user intervention
- Built-in support for single-shot and periodic timers
- Timer abstraction supports mocking for tests

## Core Concepts

### Topic
A typed pub-sub channel that delivers messages from publishers to subscribers.

```cpp
auto tick_topic = std::make_shared<Topic<TickMsg>>();
```

### Subscription
An actor's message queue for a specific topic. Actors pull messages from subscriptions.

```cpp
// In actor constructor
tick_sub_ = create_sub(tick_topic);

// In processMessages()
while (auto msg = tick_sub_->tryTakeMessage()) {
    handleTick(*msg);
}
```

### Publisher
A lightweight wrapper for publishing messages to a topic.

```cpp
// In actor constructor
tick_pub_ = create_pub(tick_topic);

// Publish a message
tick_pub_->publish(TickMsg{timestamp});
```

## Creating an Actor

Actors inherit from `Actor<Derived>` using the CRTP pattern and must use the factory method:

```cpp
#include "actor-core/actor.hpp"

// Define your message types
struct InputMsg {
    std::string data;
};

struct OutputMsg {
    int result;
};

// Create actor class
class MyActor : public Actor<MyActor> {
public:
    // Constructor - takes ActorContext as first parameter
    MyActor(Actor<MyActor>::ActorContext ctx,
            TopicPtr<InputMsg> input_topic,
            TopicPtr<OutputMsg> output_topic)
        : Actor(ctx),
          input_sub_(create_sub(input_topic)),
          output_pub_(create_pub(output_topic)) {}

    // Required: implement message processing
    void processMessages() override {
        // Pull and process all pending messages
        while (auto msg = input_sub_->tryTakeMessage()) {
            handleInput(*msg);
        }
    }

private:
    void handleInput(const InputMsg& msg) {
        // Process the message
        int result = msg.data.length();

        // Publish result
        output_pub_->publish(OutputMsg{result});
    }

    SubscriptionPtr<InputMsg> input_sub_;
    PublisherPtr<OutputMsg> output_pub_;
};

// Factory method is inherited from Actor<T>
// Use: auto actor = MyActor::create(io, input_topic, output_topic);
```

## Message Flow

1. **Publishing**: Any actor publishes a message via `Publisher::publish()`
2. **Topic Distribution**: Topic enqueues message in all subscriber queues
3. **Notification**: Topic posts `processMessages()` call to subscriber's strand (only if queue was empty)
4. **Pull Processing**: Actor wakes up, pulls messages via `Subscription::tryTakeMessage()` in desired order
5. **Processing**: Actor handles messages, may publish new messages in response

## Complete Example

```cpp
#include <asio.hpp>
#include "actor-core/actor.hpp"
#include "actor-core/topic.hpp"
#include "actor-core/timer/timer.hpp"

// Message definitions
struct Request { int id; };
struct Response { int id; std::string result; };

// Timer types for producer
struct ProducerTag {};
using ProducerCommand = TimerCommand<ProducerTag>;
using ProducerElapsed = TimerElapsedEvent<ProducerTag>;
using ProducerTimer = Timer<ProducerElapsed, ProducerCommand>;

// Producer actor (timer-driven)
class Producer : public Actor<Producer> {
public:
    Producer(Actor<Producer>::ActorContext ctx,
             TopicPtr<Request> request_topic,
             TimerFactoryPtr timer_factory)
        : Actor(ctx),
          request_pub_(create_pub(request_topic)),
          timer_(create_timer<ProducerTimer>(timer_factory)) {
        // Start periodic timer (1 second intervals)
        timer_->execute_command(make_periodic_command<ProducerTag>(
            std::chrono::seconds(1)
        ));
    }

    void processMessages() override {
        // Process timer ticks
        for (const auto& event : timer_->take_all_elapsed_events()) {
            produce();
        }
    }

private:
    void produce() {
        request_pub_->publish(Request{next_id_++});
    }

    PublisherPtr<Request> request_pub_;
    std::shared_ptr<ProducerTimer> timer_;
    int next_id_ = 0;
};

// Worker actor
class Worker : public Actor<Worker> {
public:
    Worker(Actor<Worker>::ActorContext ctx,
           TopicPtr<Request> request_topic,
           TopicPtr<Response> response_topic)
        : Actor(ctx),
          request_sub_(create_sub(request_topic)),
          response_pub_(create_pub(response_topic)) {}

    void processMessages() override {
        while (auto msg = request_sub_->tryTakeMessage()) {
            // Process request
            std::string result = "Processed " + std::to_string(msg->id);
            response_pub_->publish(Response{msg->id, result});
        }
    }

private:
    SubscriptionPtr<Request> request_sub_;
    PublisherPtr<Response> response_pub_;
};

// Main
int main() {
    asio::io_context io;

    // Create shared resources
    auto request_topic = std::make_shared<Topic<Request>>();
    auto response_topic = std::make_shared<Topic<Response>>();
    auto timer_factory = std::make_shared<TimerFactory>(io);

    // Create actors
    auto producer = Producer::create(io, request_topic, timer_factory);
    auto worker = Worker::create(io, request_topic, response_topic);

    // Run event loop
    io.run();

    return 0;
}
```

## Usage Pattern

Actors must be created using the factory method and follow this structure:

```cpp
class MyActor : public Actor<MyActor> {
public:
    MyActor(Actor<MyActor>::ActorContext ctx, /* your dependencies */)
        : Actor(ctx) { /* initialize subscriptions/publishers */ }

    void processMessages() override { /* handle messages */ }

private:
    // Your private members
};

// Create with factory method
auto actor = MyActor::create(io, dependencies...);
```

## Thread Safety

Actor code runs serialized on its strand - no manual locking needed. Topics are thread-safe for publishing from any thread. Never call actor methods directly - always communicate via topics.

## Best Practices

- **Drain message queues**: Use `while (auto msg = sub->tryTakeMessage())` to process all pending messages
- **Keep handlers fast**: Avoid blocking operations in `processMessages()`
- **Inject dependencies**: Pass `TopicPtr` to constructors for better testability
- **Let actors die naturally**: Just drop the `shared_ptr` - cleanup happens automatically

## Architecture Notes

### Why Pull-Based?

- Actors control message processing order
- Simpler backpressure handling (bounded queues)
- No callback spaghetti
- Easy to prioritize different message types

### Why Strands?

- Eliminates data races within actors
- No need for actor-level mutexes
- Asio handles scheduling automatically
- Clean separation between actors

### Why Automatic Memory Management?

- Topics hold weak references to subscribers
- Actors are automatically removed from topics when destroyed
- No manual cleanup or unsubscribe needed
- Simple actor lifecycle - just let shared_ptr go out of scope

## Requirements

- C++17 or later
- Standalone Asio 1.30.2+ (header-only)
- CMake 3.12+ (for building examples)

## License

See project root for license information.
