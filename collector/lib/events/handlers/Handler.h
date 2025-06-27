#pragma once

namespace collector::events::handler {

template <typename Derived, typename EventType>
class EventHandler {
 public:
  void Handle(const EventType& event) const {
    static_cast<const Derived*>(this)->HandleImpl(event);
  }
};

}  // namespace collector::events::handler
