#pragma once

#include <memory>
#include <tuple>

#include "IEvent.h"
#include "events/handlers/ProcessHandler.h"

namespace collector::events {
namespace {

// The idea here is compile time dispatching of events to the relevant handler(s)
// rather than having to do runtime lookups in maps or more complex (and slow)
// casting of handler types.
//
// This can be optimized by the compiler quite aggressively and this will form
// most of the hot path of event dispatching from the several event sources we
// can support
template <typename Tuple, typename Event, std::size_t I = 0>
void dispatch(const Tuple& handlers, const Event& event) {
  if constexpr (I < std::tuple_size_v<Tuple>) {
    if (auto* concrete_event = dynamic_cast<const typename std::tuple_element_t<I, Tuple>::EventType*>(&event)) {
      std::get<I>(handlers).Handle(*concrete_event);
    }
    dispatch<Tuple, Event, I + 1>(handlers, event);
  }
}
}  // namespace

template <typename... Handlers>
class StaticEventDispatcher {
 public:
  StaticEventDispatcher() {}

  void Dispatch(const IEvent& event) {
    dispatch(handlers_, event);
  }

 private:
  std::tuple<Handlers...> handlers_;
};

using EventDispatcher = StaticEventDispatcher<handler::ProcessHandler>;

}  // namespace collector::events
