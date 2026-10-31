#include <cstdio>
#include <cstdint>
#include <endian.h>

#include <rapidcheck.h>

#include "NetworkConnection.h"

extern "C" {
#include "extracted/NetworkOps.h"
}

// --- Conversion helpers ---

NetworkTypes_address to_verified(const collector::Address& addr) {
    NetworkTypes_address va;
    va.hi = addr.array()[0];
    va.lo = addr.array()[1];
    switch (addr.family()) {
        case collector::Address::Family::IPV4:
            va.family = NetworkTypes_FamilyIPv4;
            break;
        case collector::Address::Family::IPV6:
            va.family = NetworkTypes_FamilyIPv6;
            break;
        default:
            va.family = NetworkTypes_FamilyUnknown;
            break;
    }
    return va;
}

// --- RapidCheck generators ---

namespace rc {

template<>
struct Arbitrary<collector::Address> {
    static Gen<collector::Address> arbitrary() {
        return gen::oneOf(
            // Random IPv4
            gen::map(gen::arbitrary<uint32_t>(), [](uint32_t v) {
                return collector::Address(htonl(v));
            }),
            // Random IPv6
            gen::map(
                gen::pair(gen::arbitrary<uint64_t>(), gen::arbitrary<uint64_t>()),
                [](std::pair<uint64_t, uint64_t> v) {
                    return collector::Address(v.first, v.second);
                }
            ),
            // Edge cases
            gen::just(collector::Address()),
            gen::just(collector::Address(127, 0, 0, 1)),
            gen::just(collector::Address(0ULL, htobe64(1ULL))),
            gen::just(collector::Address(10, 0, 0, 1)),
            gen::just(collector::Address(8, 8, 8, 8)),
            gen::just(collector::Address(255, 255, 255, 255)),
            gen::just(collector::Address(192, 168, 1, 1)),
            gen::just(collector::Address(172, 16, 0, 1))
        );
    }
};

} // namespace rc

// --- Tests ---

int main() {
    int failures = 0;

    auto check = [&](const char* name, auto prop) {
        auto result = rc::check(name, prop);
        if (!result) failures++;
    };

    check("IsLocal agrees", [](collector::Address addr) {
        auto va = to_verified(addr);
        RC_ASSERT(addr.IsLocal() == NetworkOps_address_is_local(va));
    });

    check("IsPublic agrees", [](collector::Address addr) {
        auto va = to_verified(addr);
        RC_ASSERT(addr.IsPublic() == NetworkOps_address_is_public(va));
    });

    check("IsEphemeralPort agrees", [](uint16_t port) {
        int cpp_result = collector::IsEphemeralPort(port);
        uint8_t verified_result = NetworkOps_is_ephemeral_port(port);
        RC_ASSERT(cpp_result == static_cast<int>(verified_result));
    });

    check("IsCanonicalExternalIp agrees", [](collector::Address addr) {
        auto va = to_verified(addr);
        RC_ASSERT(collector::Address::IsCanonicalExternalIp(addr) ==
                  NetworkOps_is_canonical_external(va));
    });

    printf("\n=== Results: %d failures ===\n", failures);
    return failures;
}
