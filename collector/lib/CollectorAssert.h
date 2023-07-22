#pragma once

#ifdef COLLECTOR_ASSERT

#  define COLLECTOR_ASSERT(X)                                                                                                                                  \
    do {                                                                                                                                                       \
      if (!(X)) {                                                                                                                                              \
        collector::logging::LogMessage(__FILE__, __LINE__, false, collector::logging::LogLevel::FATAL) << "Assertion failed: " << #X << " evaluated to false"; \
      }                                                                                                                                                        \
    } while (false)

#else
#  define COLLECTOR_ASSERT(X)

#endif
