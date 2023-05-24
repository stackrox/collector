#include <cstdint>

namespace collector {

namespace self_checks {

extern const char* kSelfChecksExePath;
extern const char* kSelfChecksName;
extern const uint16_t kSelfCheckServerPort;

/**
 * @brief Starts the self-check process to trigger
 *        certain events that can be used to verify
 *        the driver is working correctly.
 */
void start_self_check_process();

}  // namespace self_checks

}  // namespace collector
