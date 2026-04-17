---
name: add-verified-component
description: Add a new formally verified F*/Pulse component to the collector's verified library with differential property tests against the C++ implementation
---

# Add Verified Component

Add a new formally verified component to `collector/verified/` that differential-tests against the existing C++ implementation in `collector/lib/`.

## Prerequisites

- F* 2026.03.24~dev with Pulse: `opam exec -- fstar.exe --version`
- Karamel: `~/code/go/src/github.com/FStarLang/FStar/karamel/krml`
- Collector builder container running: `docker ps | grep collector_builder`

## Process

### 1. Analyse the C++ component

Read the target header and implementation files in `collector/lib/`. Identify:

- **Pure functions** (no I/O, no sinsp, no gRPC) — these are candidates for verification
- **Data types** used by those functions
- **Key properties** that should hold (invariants, boundary conditions, classification correctness)
- **Byte-order concerns** — the collector stores IPv4 addresses in two `uint64_t`s in network byte order on a little-endian machine. Any address constants need careful LE byte layout. Use `debug_addr`-style tools to verify constants empirically.

### 2. Write the F* implementation

Add new `.fst` files to `collector/verified/fstar/`. Follow these conventions:

- **Machine integers only** in extractable code: `U8.t`, `U16.t`, `U32.t`, `U64.t`, `SZ.t`
- **Never use `U8.v`, `U16.v` etc. in extractable code** — these produce mathematical integers that Karamel cannot extract. Use `U8.gte`, `U8.eq`, `U16.gte` etc. for comparisons.
- **`inline_for_extraction`** for helper functions that should be inlined in the C output
- **`[@@noextract_to "krml"]`** for spec-only lemmas and proof helpers
- **Unroll rather than recurse** where possible — F* `list` extracts to heap-allocated linked lists with static initialisers. For small fixed sets (like private network ranges), use `||` chains instead.
- **z3rlimit 80** is needed for bit-manipulation proofs

Verify as you go:
```bash
cd collector/verified
opam exec -- fstar.exe --z3rlimit 80 --include fstar fstar/YourModule.fst
```

### 3. Add proved properties

Write lemmas that prove the security-critical properties. These don't extract — they catch logic errors at compile time.

```fstar
[@@noextract_to "krml"]
let lemma_name (x:input_type)
  : Lemma (requires precondition)
          (ensures postcondition) = ()
```

If a lemma fails, **fix the implementation, not the lemma**. The lemma represents the correct behaviour.

**Be careful about conforming to the C++ when the C++ might be wrong.** If the verified spec disagrees with C++, investigate whether the C++ has a bug before changing the spec.

### 4. Update the Makefile and extract to C

Add your new module to `collector/verified/Makefile`:

- Add the `.fst` file to `FST_FILES` (in dependency order)
- Add the module name to `EXTRACT_MODULES`
- Update `EXTRACT_ROOT` if your module is the new top-level
- Update the `-bundle` flag in `KRML_FLAGS` to include your module

Run extraction:
```bash
cd collector/verified && make clean && make all
```

Check for clean extraction — no `krml_checked_int_t` in the generated `.c`/`.h` files.

The generated C files must not be modified. If they need standard includes or KRML macros, those come from `extracted/krml_compat.h` which is force-included via CMake's `-include` flag.

### 5. Write differential property tests

Add a new test file in `collector/verified/tests/`. Follow the pattern in `test_network.cpp`:

```cpp
#include <rapidcheck.h>
#include "TheCollectorHeader.h"

extern "C" {
#include "YourExtractedHeader.h"
}

// 1. Conversion helper: C++ type -> verified C struct
// 2. RapidCheck generator for the C++ type (with edge cases)
// 3. Property tests: generate random input, run both, assert agreement

int main() {
    int failures = 0;
    auto check = [&](const char* name, auto prop) {
        auto result = rc::check(name, prop);
        if (!result) failures++;
    };

    check("PropertyName agrees", [](CppType input) {
        auto verified_input = to_verified(input);
        RC_ASSERT(cpp_function(input) == Verified_function(verified_input));
    });

    printf("\n=== Results: %d failures ===\n", failures);
    return failures;
}
```

### 6. Register the test in CMake

Add to `collector/verified/tests/CMakeLists.txt`:

```cmake
add_executable(test_verified_yourcomponent test_yourcomponent.cpp)
target_link_libraries(test_verified_yourcomponent collector_lib verified_yourlib rapidcheck)
add_test(NAME test_verified_yourcomponent COMMAND test_verified_yourcomponent)
```

The test links against `collector_lib` (the real C++ implementation), not stubs.

### 7. Run and iterate

```bash
# From collector root:
make -C collector verified-tests

# Or directly:
cd collector/verified && make all
docker exec collector_builder_amd64 \
  cmake --build cmake-build --target test_verified_yourcomponent -- -j 12
docker exec collector_builder_amd64 \
  ctest -V -R test_verified --test-dir cmake-build
```

If a property test fails, RapidCheck prints a shrunk counterexample. Possible causes:
1. **Byte-order mismatch** — the most common issue. Build a debug tool to print the raw uint64 values and compare.
2. **Spec bug** — fix the F* implementation.
3. **C++ bug** — investigate and fix. This is the whole point.

## Existing verified components

| Component | F* Module | C++ Source | Properties verified |
|-----------|-----------|------------|-------------------|
| NetworkConnection | NetworkTypes.fst, NetworkOps.fst | NetworkConnection.h/cpp | IsLocal, IsPublic, IsEphemeralPort, IsCanonicalExternal, CIDR containment |

## File layout

```
collector/verified/
  Makefile              — verify → extract pipeline
  fstar/
    NetworkTypes.fst    — Address, IPNet, Endpoint, L4Proto types
    NetworkOps.fst      — Classification functions + proved properties
  extracted/
    krml_compat.h       — Standard includes + KRML macros (hand-written, stable)
    NetworkOps.c        — Generated (do not modify)
    NetworkOps.h        — Generated (do not modify)
  tests/
    CMakeLists.txt      — Build config, links against collector_lib + rapidcheck
    test_network.cpp    — Differential property tests for NetworkConnection
```
