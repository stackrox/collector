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

static NRadixPulse_nradix_tree verified_tree_from(const std::vector<collector::IPNet>& nets) {
    NRadixPulse_nradix_tree tree = NRadixPulse_create();
    for (const auto& net : nets) {
        auto vn = to_verified_ipnet(net);
        tree = NRadixPulse_insert(tree, vn);
    }
    return tree;
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
            gen::just(collector::Address()),
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

    // --- Tree operation tests ---

    check("Find on empty tree returns not-found", [](collector::Address addr) {
        NRadixPulse_nradix_tree tree = NRadixPulse_create();
        auto va = to_verified_addr(addr);
        auto result = NRadixPulse_find_addr(tree, va);
        RC_ASSERT(!result.found);
        NRadixPulse_destroy(tree);
    });

    check("Insert then Find agrees with C++ for single network", [](collector::IPNet net) {
        RC_PRE(!net.IsNull());
        RC_PRE(net.bits() >= 1 && net.bits() <= 128);

        collector::NRadixTree cpp_tree;
        cpp_tree.Insert(net);

        auto vn = to_verified_ipnet(net);
        NRadixPulse_nradix_tree vtree = NRadixPulse_create();
        vtree = NRadixPulse_insert(vtree, vn);

        auto cpp_result = cpp_tree.Find(net.address());
        auto va = to_verified_addr(net.address());
        auto v_result = NRadixPulse_find_addr(vtree, va);

        RC_ASSERT(!cpp_result.IsNull() == v_result.found);
        if (v_result.found) {
            RC_ASSERT(cpp_result.bits() == v_result.net.prefix);
        }
        NRadixPulse_destroy(vtree);
    });

    check("Find agrees with C++ for multiple networks", []() {
        auto nets = *rc::gen::container<std::vector<collector::IPNet>>(
            rc::gen::arbitrary<collector::IPNet>());
        RC_PRE(nets.size() >= 1 && nets.size() <= 20);

        collector::NRadixTree cpp_tree;
        std::vector<collector::IPNet> inserted;
        for (const auto& net : nets) {
            if (net.IsNull() || net.bits() < 1 || net.bits() > 128) continue;
            if (cpp_tree.Insert(net)) {
                inserted.push_back(net);
            }
        }
        RC_PRE(!inserted.empty());

        NRadixPulse_nradix_tree vtree = verified_tree_from(inserted);

        auto queries = *rc::gen::container<std::vector<collector::Address>>(
            5, rc::gen::arbitrary<collector::Address>());

        for (const auto& addr : queries) {
            auto cpp_result = cpp_tree.Find(addr);
            auto va = to_verified_addr(addr);
            auto v_result = NRadixPulse_find_addr(vtree, va);

            RC_ASSERT(!cpp_result.IsNull() == v_result.found);
            if (v_result.found) {
                RC_ASSERT(cpp_result.bits() == v_result.net.prefix);
                RC_ASSERT(cpp_result.family() == v_result.net.addr.family);
            }
        }

        for (const auto& net : inserted) {
            auto cpp_result = cpp_tree.Find(net.address());
            auto va = to_verified_addr(net.address());
            auto v_result = NRadixPulse_find_addr(vtree, va);

            RC_ASSERT(!cpp_result.IsNull() == v_result.found);
            if (v_result.found) {
                RC_ASSERT(cpp_result.bits() == v_result.net.prefix);
            }
        }
        NRadixPulse_destroy(vtree);
    });

    check("IsEmpty agrees", []() {
        auto nets = *rc::gen::container<std::vector<collector::IPNet>>(
            rc::gen::arbitrary<collector::IPNet>());

        collector::NRadixTree cpp_tree;
        NRadixPulse_nradix_tree vtree = NRadixPulse_create();

        RC_ASSERT(cpp_tree.IsEmpty() == NRadixPulse_is_empty(vtree));

        for (const auto& net : nets) {
            if (net.IsNull() || net.bits() < 1 || net.bits() > 128) continue;
            cpp_tree.Insert(net);
            vtree = NRadixPulse_insert(vtree, to_verified_ipnet(net));
        }

        RC_ASSERT(cpp_tree.IsEmpty() == NRadixPulse_is_empty(vtree));
        NRadixPulse_destroy(vtree);
    });

    printf("\n=== Results: %d failures ===\n", failures);
    return failures;
}
