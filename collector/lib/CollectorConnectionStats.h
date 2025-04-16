#ifndef _COLLECTOR_CONNECTION_STATS_
#define _COLLECTOR_CONNECTION_STATS_

#include <algorithm>

#include "prometheus/gauge.h"
#include "prometheus/registry.h"
#include "prometheus/summary.h"

namespace collector {
template <typename T>
class CollectorConnectionStats {
 public:
  CollectorConnectionStats(
      prometheus::Registry* registry,
      const std::string& name,
      const std::string& help,
      std::chrono::milliseconds max_age,
      const std::vector<double>& quantiles,
      double error) {
    auto& family = prometheus::BuildSummary()
                       .Name(name)
                       .Help(help)
                       .Register(*registry);
    auto q = MakeQuantiles(quantiles, error);
    inbound_private_summary_ = &family.Add({{"dir", "in"}, {"peer", "private"}}, q, max_age);
    inbound_public_summary_ = &family.Add({{"dir", "in"}, {"peer", "public"}}, q, max_age);
    outbound_private_summary_ = &family.Add({{"dir", "out"}, {"peer", "private"}}, q, max_age);
    outbound_public_summary_ = &family.Add({{"dir", "out"}, {"peer", "public"}}, q, max_age);
  }

  void Observe(T inbound_private, T inbound_public, T outbound_private, T outbound_public) {
    inbound_private_summary_->Observe(inbound_private);
    inbound_public_summary_->Observe(inbound_public);
    outbound_private_summary_->Observe(outbound_private);
    outbound_public_summary_->Observe(outbound_public);
  }

 private:
  prometheus::Summary* inbound_private_summary_;
  prometheus::Summary* inbound_public_summary_;
  prometheus::Summary* outbound_private_summary_;
  prometheus::Summary* outbound_public_summary_;

  prometheus::Summary::Quantiles MakeQuantiles(std::vector<double> quantiles, double error) {
    prometheus::Summary::Quantiles result;

    result.reserve(quantiles.size());

    auto make_quantile = [error](double q) -> prometheus::detail::CKMSQuantiles::Quantile {
      return {q, error};
    };

    std::transform(quantiles.begin(), quantiles.end(), std::back_inserter(result), make_quantile);

    return result;
  }
};
}  // namespace collector

#endif
