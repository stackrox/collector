# Plugin correctness validator

The container plugin caches which container a process belongs to. Collector uses
that answer to exclude host activity and attribute container connections. A wrong
or missing answer can silently drop a legitimate connection or let host activity
through the filter. This validator checks those decisions before we assess speed.

It is a 67-case deterministic regression suite, not a random fuzzer. This PR adds
tests and CI only; plugin fixes belong in the parent PR.

## How it works

Each test gives a fresh Falco inspector a small process inventory and a sequence
of synthetic events, such as a process starting a child and that child connecting
to a socket. Falco's real TEST_INPUT engine parses those events and invokes the
actual compiled plugin. We check the resulting container ID, filtering decision,
and, in two tests, Collector's network handler and connection tracker.

The path under test is:

`scenario -> real Falco parser -> real plugin -> Collector attribution/filter -> selected network checks`

We synthesize the input, not the plugin's answer. Tests never write the plugin's
cached container-ID field. Expected IDs are explicit scenario data, not values
computed with the plugin's own extraction logic. There is no live kernel capture,
container runtime socket, Kubernetes cluster, or Sensor service.

Why simulate process IDs? A fork can be observed from both the parent and child.
Falco can create a child's process-table entry while parsing the parent's event,
before seeing anything from the child. The plugin must handle that lifecycle.
The numeric thread IDs (TIDs) merely let the test describe these relationships;
the important question is whether a real parser-created child gets the right
container identity, including when one of the events is missing.

## What's in the corpus

| Cases | Scenarios | Decision being checked |
| --- | --- | --- |
| 16 startup | Docker, CRI-O, containerd, Podman paths; host/conmon; malformed IDs; cgroup ordering | Correct identity regardless of unrelated cgroup entries; exclude host activity |
| 33 process creation | fork/clone/clone3; host, hostPID and PID-namespace containers; parent-first, child-first, missing parent or child | Identity follows a valid child through supported event orders |
| 18 focused | Exec/execveat, failed exec, ID reuse, thread clone, vfork, late discovery, installed filters, recovery and network handling | Refresh stale state and preserve downstream attribution |

The process-creation matrix deliberately excludes parent-only PID-namespace
cases: the parent reports a namespace-local child ID, insufficient for Falco to
create the global entry. For parent-only host/hostPID cases we first assert that
Falco created a valid child with the expected cgroup, then check plugin identity.
The vfork case uses child creation, child exit, then parent return, not an
arbitrary ordering that would report an impossible lifecycle as a defect.

`Corpus.h` holds typed startup data; `ContainerPluginReplayTest.cpp` defines the
event sequences and parameterized cases. GTest gives cases readable names so a
failure can be selected and reproduced individually.

## What we found locally

On parent commit `249fe750e`, **54 cases pass and 13 fail**. These failures reduce
to two root causes, not thirteen independent bugs:

- Three cases show a later nonmatching cgroup erasing an already resolved ID.
  The resulting host attribution rejects legitimate container events.
- Eight cases show missing identity for a child created from the parent's event:
  six process-creation cases, one installed-filter check and one network check.
  Two more cases model late-discovered processes with the same empty-cache gap.

The network reproduction matters beyond a field assertion: with both fork events,
the production handler records one correctly attributed connection. Omit the
child event and the handler returns `IGNORED`, leaving the tracker empty.
Supplying the child event restores attribution. Separately, a host child with an
empty cached ID passes the installed filter; this does not prove delivery to Sensor.

Across 100 shuffled local ASan/UBSan iterations (6,700 executions), every iteration
had the same 54/13 split and no sanitizer/leak diagnostics. These are behavioral
failures, not demonstrated memory-safety crashes. Supported layout controls,
complete fork orders, exec refresh, ID reuse and valid vfork ordering passed.

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

To reproduce just the lost-connection case:

```sh
bash /src/collector/test/plugin-replay/run-corpus.sh /build /tmp/repro \
  --gtest_filter=ContainerPluginReplayTest.HostPIDConnectionTrackedWithoutChildForkEvent
```

`ROX_COLLECTOR_CONTAINER_PLUGIN_PATH` selects another compatible plugin module;
it does not change the linked Falco or Collector version.

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
The log contains all iterations; GTest's XML describes only the final iteration.
The validator needs only the builder job, not the Collector image or integration
tests, and uses only read access to repository contents.

The expected original-PR result is red: 54/67 cases pass and 13 fail. Known
regressions are not skipped or converted to success. Fixes belong in the parent
plugin branch; updating this stacked branch and rerunning provides validation.

## Gaps and tradeoffs

- **Repeatable parser tests, not live capture.** Synthetic input makes ordering
  failures small and reproducible, but does not validate kernel event encoding,
  real event loss, runtime discovery, deployment or Sensor delivery. Those need
  separate live integration tests.
- **Real implementation, coupled fixtures.** Linking production Collector and
  pinned Falco catches integration errors that a fake plugin API would miss.
  It also requires a Linux builder and may need fixture changes on Falco upgrades.
  Swapping a plugin file is not a comparison with an older Collector release.
- **Late discovery is modeled.** Two tests insert a valid process into Falco's
  real thread manager after capture starts. TEST_INPUT has no live `/proc` lookup
  callback. These support the cache-lifecycle finding, not a live-discovery claim.
- **Filter setup can drift.** The short filter construction is copied from
  `Service.cpp`, rather than shared with service startup. Changes to production
  filter configuration must be reflected here until a shared helper is extracted.
- **Repetition is not exploration.** Shuffling repeats the same 67 scenarios;
  it does not mutate event contents, inject callback failures, explore concurrency,
  or cover every process lifecycle. Bounded structured mutation is a next step,
  with minimized failures promoted into named regression cases.
- **Sanitizers are not a performance test.** They cover instrumented code, not
  prebuilt external libraries, and a clean run is not proof of memory safety.
  Recovery of the reported CPU regression requires optimized, controlled workload
  comparisons that also verify signal correctness.

First use this corpus to validate the parent's fixes without weakening assertions.
Then extend it with structured mutation and live integration checks; keep CPU
benchmarks separate so correctness and performance results remain interpretable.
