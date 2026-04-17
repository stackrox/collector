#include <cstdio>
#include <cstdint>
#include <endian.h>

#include <rapidcheck.h>

#include "NetworkConnection.h"
#include "NRadix.h"

extern "C" {
#include "extracted/NRadixPulse.h"
#include "extracted/internal/NetworkTypes.h"
}

// --- Conversion helpers ---

static NetworkTypes_address to_verified_addr(const collector::Address& addr) {
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

static NetworkTypes_ipnet to_verified_ipnet(const collector::IPNet& net) {
    auto va = to_verified_addr(net.address());
    return NetworkTypes_mk_ipnet(va, static_cast<uint8_t>(net.bits()));
}

// --- RapidCheck generators ---

namespace rc {

template<>
struct Arbitrary<collector::Address> {
    static Gen<collector::Address> arbitrary() {
        return gen::oneOf(
            gen::map(gen::arbitrary<uint32_t>(), [](uint32_t v) {
                return collector::Address(htonl(v));
            }),
            gen::map(
                gen::pair(gen::arbitrary<uint64_t>(), gen::arbitrary<uint64_t>()),
                [](std::pair<uint64_t, uint64_t> v) {
                    return collector::Address(v.first, v.second);
                }
            ),
            gen::just(collector::Address(127, 0, 0, 1)),
            gen::just(collector::Address(10, 0, 0, 1)),
            gen::just(collector::Address(192, 168, 1, 1)),
            gen::just(collector::Address(172, 16, 0, 1)),
            gen::just(collector::Address(8, 8, 8, 8)),
            gen::just(collector::Address(255, 255, 255, 255))
        );
    }
};

template<>
struct Arbitrary<collector::IPNet> {
    static Gen<collector::IPNet> arbitrary() {
        return gen::oneOf(
            gen::map(
                gen::pair(gen::arbitrary<uint32_t>(), gen::inRange<size_t>(1, 33)),
                [](std::pair<uint32_t, size_t> v) {
                    return collector::IPNet(collector::Address(htonl(v.first)), v.second);
                }
            ),
            gen::map(
                gen::tuple(gen::arbitrary<uint64_t>(), gen::arbitrary<uint64_t>(),
                           gen::inRange<size_t>(1, 129)),
                [](std::tuple<uint64_t, uint64_t, size_t> v) {
                    return collector::IPNet(
                        collector::Address(std::get<0>(v), std::get<1>(v)),
                        std::get<2>(v));
                }
            ),
            gen::just(collector::IPNet(collector::Address(10, 0, 0, 0), 8)),
            gen::just(collector::IPNet(collector::Address(192, 168, 0, 0), 16)),
            gen::just(collector::IPNet(collector::Address(172, 16, 0, 0), 12))
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

    // --- Public API property tests ---

    check("Empty tree is empty", []() {
        collector::NRadixTree tree;
        RC_ASSERT(tree.IsEmpty());
    });

    check("Insert makes tree non-empty", [](collector::IPNet net) {
        RC_PRE(!net.IsNull() && net.bits() >= 1u && net.bits() <= 128u);
        collector::NRadixTree tree;
        tree.Insert(net);
        RC_ASSERT(!tree.IsEmpty());
    });

    check("Find after insert returns a containing network", [](collector::IPNet net) {
        RC_PRE(!net.IsNull() && net.bits() >= 1u && net.bits() <= 128u);
        collector::NRadixTree tree;
        tree.Insert(net);
        auto result = tree.Find(net.address());
        RC_ASSERT(!result.IsNull());
        RC_ASSERT(result.Contains(net.address()));
    });

    check("Find on empty tree returns null", [](collector::Address addr) {
        collector::NRadixTree tree;
        auto result = tree.Find(addr);
        RC_ASSERT(result.IsNull());
    });

    check("GetAll returns all inserted networks", []() {
        auto nets = *rc::gen::container<std::vector<collector::IPNet>>(
            rc::gen::arbitrary<collector::IPNet>());
        collector::NRadixTree tree;
        size_t count = 0;
        for (const auto& net : nets) {
            if (!net.IsNull() && net.bits() >= 1u && net.bits() <= 128u) {
                if (tree.Insert(net)) count++;
            }
        }
        auto all = tree.GetAll();
        RC_ASSERT(all.size() == count);
    });

    // --- Raw verified API tests ---

    check("Verified: empty tree find returns not-found", [](collector::Address addr) {
        NRadixPulse_nradix_tree tree = NRadixPulse_create();
        auto va = to_verified_addr(addr);
        auto result = NRadixPulse_find_addr(tree, va);
        RC_ASSERT(!result.found);
        NRadixPulse_destroy(tree);
    });

    check("Verified: insert then find succeeds", [](collector::IPNet net) {
        RC_PRE(!net.IsNull() && net.bits() >= 1u && net.bits() <= 128u);
        NRadixPulse_nradix_tree tree = NRadixPulse_create();
        tree = NRadixPulse_insert(tree, to_verified_ipnet(net));
        auto va = to_verified_addr(net.address());
        auto result = NRadixPulse_find_addr(tree, va);
        RC_ASSERT(result.found);
        RC_ASSERT(result.net.prefix == static_cast<uint8_t>(net.bits()));
        NRadixPulse_destroy(tree);
    });

    check("Verified: is_empty on fresh tree", []() {
        NRadixPulse_nradix_tree tree = NRadixPulse_create();
        RC_ASSERT(NRadixPulse_is_empty(tree));
        NRadixPulse_destroy(tree);
    });

    printf("\n=== Results: %d failures ===\n", failures);
    return failures;
}
