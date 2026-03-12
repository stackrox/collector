#pragma once

namespace collector::events::handler {

// An EventHandler is the owner of a processing pipeline
// for a given event type.
//
// This base class is intended to formalize the API between
// event dispatchers and event handlers, using CRTP.
//
// Note: C++20 would allow us to use concepts for this API
// definition, and this should be changed if/when we move to
// the newer standard.
//
// example derived handler:
//
// ```cpp
// class MyEventHandler : public EventHandler<MyEventHandler, MyEventType> {
//   public:
//     void HandleImpl(const MyEventType& event) const {
//        std::cout << event << std::endl;
//     }
// }
// ```
template <typename Derived, typename EventType>
class EventHandler {
 public:
  void Handle(const EventType& event) const {
    static_cast<const Derived*>(this)->HandleImpl(event);
  }
};

}  // namespace collector::events::handler
