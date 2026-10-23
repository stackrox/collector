---
name: add-verified-component
description: Add a new formally verified F*/Pulse component to the collector's verified library — replaces C++ internals with extracted verified C code
---

# Add Verified Component

Replace C++ component internals with a formally verified implementation written in F*/Pulse, extracted to C via Karamel. The verified code proves both memory safety and functional correctness. The C++ class becomes a thin wrapper around the extracted C API.

## Architecture

Each verified component follows the ghost-spec-as-invariant pattern:

1. **Spec module** (`XSpec.fst`) — pure functional implementation + correctness lemmas. Marked `[@@noextract_to "krml"]`. Never extracted. Serves as ground truth.
2. **Pulse module** (`XPulse.fst`) — heap implementation using `Pulse.Lib.Box` for allocation. Maintains a ghost invariant linking heap state to the spec. This IS the code that runs.
3. **C++ wrapper** — thin class forwarding to the extracted C API via `extern "C"`. Preserves the existing public API so callers don't change.

Proofs compose: each component's spec can reference other components' specs.

## Prerequisites

- F* 2026.03.24~dev with Pulse: `opam exec -- fstar.exe --version`
- Karamel: `~/code/go/src/github.com/FStarLang/FStar/karamel/krml`
- Collector builder container running: `DOCKER_HOST=unix:///run/podman/podman.sock docker ps | grep collector_builder`

## Process

### 1. Analyse the C++ component

Read the target header and implementation files in `collector/lib/`. Identify:

- **Operations** to verify (insert, find, delete, etc.)
- **Data types** used — which are value types vs heap-allocated structures
- **Key properties** that should hold (invariants, boundary conditions, correctness)
- **Byte-order concerns** — the collector stores IPv4 addresses in two `uint64_t`s in network byte order on a little-endian machine

### 2. Write the spec module (XSpec.fst)

Create `collector/verified/fstar/XSpec.fst` with:

- Pure functional data types (algebraic types are fine here — they're never extracted)
- Pure functions implementing the algorithm
- Correctness lemmas (use `admit()` initially, discharge later)
- Everything marked `[@@noextract_to "krml"]` EXCEPT pure helper functions that the Pulse module will call directly (those need `inline_for_extraction`)

```fstar
module XSpec

[@@noextract_to "krml"]
noeq type my_tree =
  | Leaf : my_tree
  | Node : value:my_type -> left:my_tree -> right:my_tree -> my_tree

[@@noextract_to "krml"]
let rec find (tree:my_tree) (key:key_t) : option my_type = ...

[@@noextract_to "krml"]
let lemma_find_correct (tree:my_tree) (key:key_t)
  : Lemma (ensures ...) = admit ()  // discharge later
```

Verify:
```bash
cd collector/verified
opam exec -- fstar.exe --z3rlimit 80 --include fstar fstar/XSpec.fst
```

### 3. Write the Pulse module (XPulse.fst)

Create `collector/verified/fstar/XPulse.fst` with `#lang-pulse`.

**Key conventions:**

- Use `Pulse.Lib.Box` for heap-allocated pointers (`box a`, `alloc`, `free`, `!`, `:=`)
- Use `option (box node_t)` for nullable pointers (NOT raw `box` with `is_null` — the option pattern works better with Karamel extraction)
- Define a recursive `slprop` invariant linking heap to ghost spec
- Write ghost helper functions for `fold`/`unfold` of the invariant
- Each operation proves its postcondition matches the spec
- Use `fn rec` with `decreases` for recursive traversals

```fstar
module XPulse
#lang-pulse

open Pulse.Lib.Pervasives
module Box = Pulse.Lib.Box
open Pulse.Lib.Box { box, (:=), (!) }
open XSpec

// Heap node type
noeq type node_t = {
  value: my_type;
  left:  tree_ptr;
  right: tree_ptr;
}
and node_box = box node_t
and tree_ptr = option node_box

// Ghost invariant: links heap to spec
let rec is_tree (ct: tree_ptr) (ft: XSpec.my_tree)
  : Tot slprop (decreases ft) = ...

// Operations with proof obligations
fn find (t: handle) (#ft: erased XSpec.my_tree) (key: key_t)
  requires is_tree t.root ft
  returns result: find_result
  ensures is_tree t.root ft **
          pure (result == XSpec.find (reveal ft) key)
{ ... }
```

Verify:
```bash
cd collector/verified
opam exec -- fstar.exe --z3rlimit 80 --include fstar \
  --include /home/ghutton/.opam/default/lib/fstar/pulse \
  fstar/XPulse.fst
```

**Important Karamel constraints:**
- Recursive algebraic types (like `noeq type tree = Leaf | Node ...`) cause Karamel to hang. Use `option (box node_t)` instead — Pulse's Box extracts to C malloc/free pointers which Karamel handles.
- Avoid `FStar.Pervasives.Native.option` in types unless you bundle `FStar.Pervasives.Native` in the extraction.
- Use `bool` flags instead of `option` for return types where possible.
- Machine integers only (`U8.t`, `U32.t`, `U64.t`) in extractable code — `nat`/`int` produce `krml_checked_int_t`.

### 4. Update the Makefile and extract to C

Add your modules to `collector/verified/Makefile`:

- Add spec file to `SPEC_FILES`
- Add Pulse module to `MODULES`
- Add explicit `verify-X` and `extract-X` targets

For the extraction command, include dependent modules in `--extract` and `-bundle`:
```makefile
extract-XPulse: verify-XPulse
	$(FSTAR_EXE) $(FSTAR_FLAGS) --codegen krml \
		--extract 'NetworkTypes,XSpec,XPulse,FStar.Pervasives.Native' \
		fstar/XPulse.fst
	mv out.krml out_XPulse.krml
	rm -f extracted/XPulse.c extracted/XPulse.h extracted/Prims.h extracted/Makefile.include
	$(KRML_EXE) $(KRML_BASE_FLAGS) \
		-bundle 'NetworkTypes' \
		-bundle 'XPulse=XPulse,XSpec,FStar.Pervasives.Native' \
		out_XPulse.krml
```

Run extraction:
```bash
cd collector/verified && make extract-XPulse
```

Check for clean extraction — no `krml_checked_int_t` in the generated `.c`/`.h` files.

### 5. Add to the verified static library

Add the new `.c` file to `collector/verified/CMakeLists.txt`:

```cmake
add_library(verified STATIC
    extracted/NetworkTypes.c
    extracted/NetworkOps.c
    extracted/XPulse.c        # new
)
```

The library is already linked to `collector_lib` — no other CMake changes needed.

### 6. Write the C++ wrapper

Replace the C++ class internals to forward to the extracted C API:

```cpp
extern "C" {
#include "extracted/XPulse.h"
#include "extracted/NetworkTypes.h"
}

class MyClass {
  XPulse_handle handle_;
public:
  MyClass() : handle_(XPulse_create()) {}
  ~MyClass() { XPulse_destroy(handle_); }
  Result Find(Key k) const {
    auto result = XPulse_find(handle_, to_verified(k));
    return from_verified(result);
  }
};
```

Conversion functions (`to_verified`/`from_verified`) handle the type boundary. Keep them in an anonymous namespace in the `.cpp` file.

### 7. Write property tests

Add a test file in `collector/verified/tests/`. Two categories:

**Public API tests** — exercise the C++ wrapper to validate integration:
```cpp
check("Find after insert returns containing network", [](IPNet net) {
    RC_PRE(!net.IsNull() && net.bits() >= 1u);
    MyClass obj;
    obj.Insert(net);
    auto result = obj.Find(net.address());
    RC_ASSERT(!result.IsNull());
    RC_ASSERT(result.Contains(net.address()));
});
```

**Raw verified API tests** — exercise the extracted C directly:
```cpp
check("Verified: insert then find succeeds", [](IPNet net) {
    RC_PRE(!net.IsNull() && net.bits() >= 1u);
    auto tree = XPulse_create();
    tree = XPulse_insert(tree, to_verified(net));
    auto result = XPulse_find(tree, to_verified(net.address()));
    RC_ASSERT(result.found);
    XPulse_destroy(tree);
});
```

Register in `collector/verified/tests/CMakeLists.txt`:
```cmake
add_executable(test_verified_x test_x.cpp)
target_link_libraries(test_verified_x collector_lib verified rapidcheck)
add_test(NAME test_verified_x COMMAND test_verified_x)
```

### 8. Build and test

```bash
# Build everything
make -C collector collector

# Run all tests
DOCKER_HOST=unix:///run/podman/podman.sock docker exec collector_builder_amd64 \
  ctest -V --test-dir <worktree>/cmake-build

# Run just verified tests
DOCKER_HOST=unix:///run/podman/podman.sock docker exec collector_builder_amd64 \
  ctest -V -R test_verified --test-dir <worktree>/cmake-build
```

All existing tests must pass unchanged — the public API is preserved.

### 9. Discharge admitted proofs

Go back and replace `admit()` in spec lemmas with actual proofs. Work iteratively — verify after each lemma.

## Existing verified components

| Component | Spec | Pulse | C++ Wrapper | Properties |
|-----------|------|-------|-------------|------------|
| NetworkConnection | NetworkTypes.fst | — (pure functions only) | — (differential tests) | IsLocal, IsPublic, IsEphemeralPort, IsCanonicalExternal, CIDR containment |
| NRadix tree | NRadixSpec.fst | NRadixPulse.fst | NRadix.h/cpp | create, destroy, insert (with duplicate detection), find, is_empty. Ghost invariant links heap tree to functional spec. |

## File layout

```
collector/verified/
  .gitignore              — Ignores .checked and .krml files
  CMakeLists.txt          — Builds libverified.a static library
  Makefile                — F* verify + Karamel extract pipeline
  fstar/
    NetworkTypes.fst      — Address, IPNet, Endpoint types (shared)
    NetworkOps.fst        — Classification functions (pure)
    NRadixSpec.fst        — Functional tree spec + correctness lemmas (not extracted)
    NRadixPulse.fst       — Pulse heap tree implementation (extracted)
  extracted/
    krml_compat.h         — Standard includes + KRML macros (hand-written, stable)
    NetworkTypes.c/h      — Shared type definitions (generated)
    NetworkOps.c/h        — Classification functions (generated)
    NRadixPulse.c/h       — Heap tree operations (generated)
  tests/
    CMakeLists.txt        — Test targets, links collector_lib + verified + rapidcheck
    test_network.cpp      — Property tests for NetworkOps
    test_nradix.cpp       — Property tests for NRadix tree
```

## Lessons learned

- **Karamel cannot extract recursive algebraic types** (its inlining phase hangs). Use `option (box node_t)` for tree structures — Pulse's Box extracts to C pointers which Karamel handles.
- **Pulse's `option (box _)` pattern** is the idiomatic way to model nullable pointers (used by Pulse.Lib.LinkedList and Pulse.Lib.AVLTree).
- **Ghost helpers are essential.** Write `intro_`/`elim_` ghost functions for fold/unfold of the recursive `slprop` invariant. Every operation needs them.
- **Use `fn rec` with `decreases`** for recursive traversals. Decrease on the ghost spec tree for read-only operations, on a counter for mutating operations.
- **`remove_definitions()` in CMakeLists.txt** — the parent CMake scope may have `add_definitions` with problematic values (e.g. commas). Use `remove_definitions()` to isolate the verified library.
- **Name verified modules to avoid header clashes** — e.g. `NRadixPulse.h` not `NRadix.h` (which is the C++ header). Include with `extracted/` prefix for clarity.
