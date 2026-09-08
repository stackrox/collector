# Container plugin replay tests

Use this suite to check container attribution, host filtering and selected network
handling without starting Collector against a live kernel or Kubernetes cluster.
It supplies synthetic process events to the real Falco parser, loads the compiled
container plugin, and checks the answers through Collector's production code.

## Build and run

Run inside a Linux Collector builder environment, with the repository at `/src`.
Initialize the required submodules first. The source tree must be writable because
Falco generates some headers there during the build.

```sh
cd /src
git submodule update --init falcosecurity-libs collector/proto/third_party/stackrox
cmake -S /src -B /build \
  -DBUILD_PLUGIN_REPLAY_TESTS=ON \
  -DCMAKE_BUILD_TYPE=Debug -DDISABLE_PROFILING=ON
cmake --build /build --target ContainerPluginReplayTest -j2
bash /src/collector/test/plugin-replay/run-corpus.sh /build /tmp/plugin-results
```

The runner writes `run.log` and `results.xml` and returns nonzero if any assertion
fails. It sets the plugin path automatically. To select or list cases, pass GTest
arguments after the two directory arguments:

```sh
bash /src/collector/test/plugin-replay/run-corpus.sh /build /tmp/plugin-results \
  --gtest_filter='*MatchingCgroup*'
bash /src/collector/test/plugin-replay/run-corpus.sh /build /tmp/plugin-results \
  --gtest_list_tests
```

CTest also registers the target and supplies its plugin path:

```sh
ctest --test-dir /build -R '^ContainerPluginReplayTest$' --output-on-failure
```

For order-dependent failures, set `REPLAY_REPEAT=100 REPLAY_SEED=3939` before the
runner command. This repeats and shuffles the same cases, not their event contents.
The log retains every iteration; the XML describes only the last iteration.
`ROX_COLLECTOR_CONTAINER_PLUGIN_PATH` can select a compatible plugin module, but
does not change the linked Falco or Collector version.

## How a test works

Each case starts with a fresh inspector and plugin. `SeedThread` supplies the
initial process inventory and cgroups. `Open` starts the inspector, letting Falco
invoke the plugin's capture callback. Event helpers then feed synthetic events
through Falco's TEST_INPUT engine; Falco performs parsing, process-table updates
and plugin callbacks. Tests do not call those callbacks directly or populate the
plugin's cached container-ID field.

`ExpectAttribution` checks Collector's container ID and the plugin-backed filter.
Network-focused cases additionally exercise `NetworkSignalHandler` and
`ConnectionTracker`. Expected IDs are explicit test data, never calculated with
the plugin's extraction function.

Process IDs simply connect events to their parent or child. For example, Falco
may learn about a child from the parent's fork event before observing the child's
event. Keeping those inputs separate lets a test check attribution when events
arrive in a different order or one is missing.

## Extend the corpus

The corpus has three groups:

- Startup layouts in `Corpus.h`: Docker, CRI-O, containerd and Podman cgroups,
  host/conmon exclusion, malformed IDs and cgroup ordering.
- Process-creation combinations in `Corpus.h`: fork/clone/clone3, host or container
  origins, and parent/child event ordering. The parameterized test supplies events.
- Focused `TEST_F` cases in `ContainerPluginReplayTest.cpp`: exec refresh, process-ID
  reuse, thread clone, vfork, late discovery, filtering and connection tracking.

### Add a cgroup layout

Add a named `StartupCase` to `StartupCases()` with controller-prefixed cgroup
strings and an explicit expected short ID (or `""` for host/excluded activity).
For example, a new ordering case could be:

```cpp
{"HostThenDocker", {"cpuset=/", "memory=/docker/" + kA}, "aaaaaaaaaaaa"},
```

The existing parameterized test seeds the process and checks attribution and
filtering. Use a unique descriptive name so the case is easy to select in GTest.

### Add a lifecycle scenario

Add a `TEST_F(ContainerPluginReplayTest, DescriptiveName)` in the replay test file:

1. Seed only the processes known before capture, then call `Open()`.
2. Generate the smallest valid event sequence needed for the scenario, using the
   existing Falco helpers. Keep timestamps increasing and parent/child IDs coherent.
3. Assert prerequisites such as the child's existence and cgroups before checking
   attribution. A missing parser-created process is different from a plugin bug.
4. Check the expected ID and filtering decision; use the network-handler helper
   when the requirement is that a connection actually reaches the tracker.
5. Include a nearby positive control when omitting or reordering an event, and
   run the focused case followed by the full corpus.

Extend `ForkCases()` only when an event has the same encoding and expectations as
the existing parameterized test. Use a focused case for a different lifecycle.
Do not remove its parent-only PID-namespace exclusion: that event supplies a
namespace-local child ID, insufficient to create the global child entry. Similarly,
vfork sequences must respect the child's exit before the parent's return.

## CI and maintenance

Main CI calls `.github/workflows/plugin-validator.yml` alongside unit tests using
the same builder-tag output. It builds the standard checkout and pinned submodules
on AMD64 and ARM64, runs the corpus once, and uploads logs, XML and build/revision
information even on failure. Assertions are not skipped or converted to success.

`BUILD_PLUGIN_REPLAY_TESTS` is opt-in. Its CMake target compiles the pinned Falco
test helpers and enables TEST_INPUT without enabling the entire upstream suite.
Normal builds are unchanged when the option is off. When updating Falco, check
helper signatures and event semantics as well as whether the target still builds.

## Boundaries to preserve

- Synthetic input does not test live kernel capture, actual event loss, runtime
  discovery, deployment or Sensor delivery. Add live integration tests for those.
- `ImportLateThread` models the result of a successful `/proc` lookup by inserting
  a valid process into Falco's thread manager. TEST_INPUT has no live lookup callback;
  keep those tests clearly distinguished from event-only reproductions.
- Filter construction is currently copied from `Service.cpp`. Keep it aligned
  with production configuration until a shared helper replaces the duplication.
- Replays are deterministic correctness tests, not random fuzzing or CPU benchmarks.
  If adding structured mutation, retain reproducible seeds and promote minimized
  failures to named cases. Measure performance separately with controlled workloads.

## Next steps

Planned extensions, not currently supported:

- Expand the corpus with cgroup changes after startup, plugin restart/reinitialization,
  and callback read/write failures. Keep failure injection separate from valid
  event-sequence tests.
- Add bounded structured fuzzing of valid event sequences. Save reproducible seeds
  and promote minimized failures into named regression cases.
- Extend replay first to the post-upgrade Collector without the plugin, then to
  the pre-upgrade Collector. Reuse scenario expectations, but build each revision
  with its own pinned Falco dependency and event/attribution adapter; older output
  is a comparison, not the correctness oracle.
- Add packaged-image integration tests for live discovery, event loss and signal
  delivery. Keep controlled CPU benchmarks separate from correctness replay.

Link these items to tracking issues as the work is scoped.
