#pragma once

#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include <driver/ppm_events_public.h>

namespace collector::replay_corpus {

// Explicit expectations: never derive the oracle with the plugin's extractor.
inline const std::string kA(64, 'a');
inline const std::string kB(64, 'b');
inline const std::string kContainer = "/kubepods/burstable/pod123/" + kA;

struct StartupCase {
  std::string name;
  std::vector<std::string> cgroups;
  std::string expected_id;
};

inline void PrintTo(const StartupCase& scenario, std::ostream* out) {
  *out << scenario.name;
}

inline std::vector<StartupCase> StartupCases() {
  return {
      {"Empty", {}, ""},
      {"HostRoot", {"memory=/", "cpuset=/"}, ""},
      {"HostSystemd", {"memory=/system.slice/kubelet.service"}, ""},
      {"DockerCgroupfs", {"memory=/docker/" + kA}, "aaaaaaaaaaaa"},
      {"DockerSystemd", {"memory=/system.slice/docker-" + kA + ".scope"}, "aaaaaaaaaaaa"},
      {"CrioSystemd", {"memory=/kubepods.slice/crio-" + kA + ".scope"}, "aaaaaaaaaaaa"},
      {"ContainerdSystemd", {"memory=/kubepods.slice/cri-containerd-" + kA + ".scope"}, "aaaaaaaaaaaa"},
      {"PodmanSystemd", {"memory=/machine.slice/libpod-" + kA + ".scope"}, "aaaaaaaaaaaa"},
      {"ConmonExcluded", {"memory=/machine.slice/libpod-conmon-" + kA + ".scope"}, ""},
      {"ShortIDRejected", {"memory=/docker/aaaaaaaaaaaa"}, ""},
      {"NonhexRejected", {"memory=/docker/" + std::string(64, 'z')}, ""},
      {"InvalidSeparatorRejected", {"memory=/docker_" + kA}, ""},
      {"SameIDAcrossControllers", {"memory=" + kContainer, "cpu=" + kContainer}, "aaaaaaaaaaaa"},
      {"MatchThenHost", {"memory=" + kContainer, "cpuset=/"}, "aaaaaaaaaaaa"},
      {"HostThenMatch", {"cpuset=/", "memory=" + kContainer}, "aaaaaaaaaaaa"},
      {"MatchBetweenHosts", {"cpu=/", "memory=" + kContainer, "cpuset=/"}, "aaaaaaaaaaaa"},
  };
}

enum class Origin { Host,
                    HostPIDContainer,
                    PIDNamespaceContainer };
enum class ForkOrder { ParentThenChild,
                       ChildThenParent,
                       ChildOnly,
                       ParentOnly };

struct ForkCase {
  std::string name;
  ppm_event_code event_type;
  Origin origin;
  ForkOrder order;
};

inline void PrintTo(const ForkCase& scenario, std::ostream* out) {
  *out << scenario.name;
}

inline std::vector<ForkCase> ForkCases() {
  std::vector<ForkCase> cases;
  const std::vector<std::pair<std::string, ppm_event_code>> events = {
      {"Fork", PPME_SYSCALL_FORK_20_X},
      {"Clone", PPME_SYSCALL_CLONE_20_X},
      {"Clone3", PPME_SYSCALL_CLONE3_X},
  };
  const std::vector<std::pair<std::string, Origin>> origins = {
      {"Host", Origin::Host},
      {"HostPID", Origin::HostPIDContainer},
      {"PIDNamespace", Origin::PIDNamespaceContainer},
  };
  const std::vector<std::pair<std::string, ForkOrder>> orders = {
      {"ParentThenChild", ForkOrder::ParentThenChild},
      {"ChildThenParent", ForkOrder::ChildThenParent},
      {"ChildOnly", ForkOrder::ChildOnly},
      {"ParentOnly", ForkOrder::ParentOnly},
  };
  for (const auto& [event_name, event] : events) {
    for (const auto& [origin_name, origin] : origins) {
      for (const auto& [order_name, order] : orders) {
        // A parent in a PID namespace cannot supply the global child TID.
        // Falco deliberately waits for the child event; no valid-ID oracle here.
        if (origin == Origin::PIDNamespaceContainer && order == ForkOrder::ParentOnly) {
          continue;
        }
        cases.push_back({event_name + "_" + origin_name + "_" + order_name,
                         event, origin, order});
      }
    }
  }
  return cases;
}

}  // namespace collector::replay_corpus
