# Plugin correctness validator

An opt-in validator that links production `collector_lib`, loads the actual
container plugin, and replays synthetic events through the pinned Falco library.
No live capture, container runtime socket, or Kubernetes cluster is needed.
See [CORPUS.md](CORPUS.md) for the 67 scenarios and expected outcomes.

## Build and run locally

In a Linux Collector builder environment with source at `/src`:

```sh
cmake -S /src -B /build \
  -DBUILD_PLUGIN_REPLAY_TESTS=ON \
  -DCMAKE_BUILD_TYPE=Debug -DDISABLE_PROFILING=ON
cmake --build /build --target ContainerPluginReplayTest -j4
bash /src/collector/test/plugin-replay/run-corpus.sh /build /tmp/plugin-results
```

Initialize the pinned `falcosecurity-libs` and
`collector/proto/third_party/stackrox` submodules first. Falco sources must be
writable because its build generates some source-tree headers.

The runner retains logs and XML and returns nonzero on test failure. CTest also
registers `ContainerPluginReplayTest` and supplies the plugin path automatically:

```sh
ctest --test-dir /build -R '^ContainerPluginReplayTest$' --output-on-failure
```

The build reuses upstream fixture sources without enabling the entire upstream
test suite. It enables Falco's TEST_INPUT engine only with this build option.
Normal Collector test builds remain unchanged when the option is off.

## Sanitizers

Instrument C as well as C++ so libscap participates:

```sh
cmake -S /src -B /build-asan \
  -DBUILD_PLUGIN_REPLAY_TESTS=ON -DADDRESS_SANITIZER=ON \
  -DCMAKE_BUILD_TYPE=Debug -DDISABLE_PROFILING=ON \
  '-DCMAKE_C_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer'
cmake --build /build-asan --target ContainerPluginReplayTest -j2
REPLAY_REPEAT=100 REPLAY_SEED=3939 \
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  bash /src/collector/test/plugin-replay/run-corpus.sh /build-asan /tmp/plugin-asan
```

External prebuilt builder libraries are not rebuilt with instrumentation. If a
Debug build already generated the BPF skeleton, the sanitizer build can reuse it
with `-DMODERN_BPF_SKEL_DIR=/build/skel_dir`. No BPF program is loaded by replay.

## CI

Main CI calls `.github/workflows/plugin-validator.yml` alongside unit tests,
using the same `build-builder-image` output tag and standard checkout (the merge
commit for pull requests). It builds against that revision's pinned submodules.
AMD64 and ARM64 each run 100 shuffled ASan/UBSan
iterations. Logs, XML, and revision information are uploaded even if replay fails.
The validator needs only the builder job, not the Collector image or integration
tests, and uses only read access to repository contents.

The expected original-PR result is red: 54/67 cases pass and 13 fail. Known
regressions are not skipped or converted to success. Fixes belong in the parent
plugin branch; updating this stacked branch and rerunning provides validation.

## Limitations

This contribution contains tests and CI only; plugin fixes belong in the parent PR.

The tests use real Falco callbacks and production ID lookup; two also exercise
the network handler and connection tracker. The short filter-construction code
is currently duplicated from Service.cpp, not shared with service startup.
Late-import cases model the result of a successful /proc lookup because TEST_INPUT
has no live proc_get callback. There is no Sensor delivery, random mutation, or
performance benchmark here. These boundaries are detailed in CORPUS.md.
