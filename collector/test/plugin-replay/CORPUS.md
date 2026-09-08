# Plugin correctness corpus

This is the first target of a Collector stability validator: real Falco,
production plugin callbacks, Collector attribution, and selected downstream
network handling. It is deterministic scenario replay, not random fuzzing yet.

## Encoding and coverage

`Corpus.h` contains typed C++ scenario data, with explicit expected IDs rather
than an oracle computed using the plugin's own extraction function. GTest
parameterization gives every generated case a stable readable name.

- 16 startup cases: supported Docker/CRI-O/containerd/Podman layouts, host and
  conmon exclusion, malformed IDs, and controller ordering.
- 33 fork cases: fork/clone/clone3; host, hostPID container, PID-namespace
  container; parent-first, child-first, missing parent, and missing child events.
- 18 focused cases: original smoke tests; late imports; actual installed filter;
  child-event recovery; exec/execveat and failed exec; TID reuse across containers
  and from container to host; thread clone; valid vfork exit ordering; and two
  production NetworkSignalHandler/ConnectionTracker tests.

Every case constructs a fresh inspector and loads the plugin. Falco's real
TEST_INPUT engine performs parsing, thread creation, and plugin callbacks. No
test writes the cached container-ID field. Callback order is not mocked.

The generated parent-only cases deliberately exclude PID-namespace parents:
their return value is a namespace-local TID, and Falco explicitly cannot create
the global child entry from that event alone. Parent-only host/hostPID cases
assert that Falco has a valid child and the expected cgroup before testing the
plugin's attribution. Their XML includes the cached ID and actual child cgroup.

The vfork test observes the child, its exit, then the parent's return. Arbitrary
invalid vfork orderings are not treated as production defects.

Late-import tests explicitly insert a valid thread through the real thread
manager after capture starts. This models the state resulting from a successful
late /proc lookup; TEST_INPUT does not implement live proc_get. Treat these two
cases as supporting evidence, not an end-to-end reproduction of live discovery.

## Run

Build using the commands in README.md. Then, inside the Linux builder:

```sh
bash /src/collector/test/plugin-replay/run-corpus.sh /build /tmp/corpus-results
```

The runner writes `run.log` and GTest/JUnit-style `results.xml`, and propagates
test failures as a nonzero exit status. It does not mark known defects as passing
or skip them. XML from a repeated GTest run describes its final iteration; the
log retains all iterations.

Select a minimal reproduction with an ordinary GTest filter:

```sh
bash /src/collector/test/plugin-replay/run-corpus.sh /build /tmp/repro \
  --gtest_filter=ContainerPluginReplayTest.HostPIDConnectionTrackedWithoutChildForkEvent
```

For repeated sanitizer validation, build `/build-asan` using README.md, then:

```sh
REPLAY_REPEAT=100 REPLAY_SEED=3939 \
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  bash /src/collector/test/plugin-replay/run-corpus.sh /build-asan /tmp/corpus-asan
```

An alternative plugin can be selected with
`ROX_COLLECTOR_CONTAINER_PLUGIN_PATH`. Use a compatible module from the same
Falco API build; this does not switch the linked Falco or Collector revision.

## Observed results on original PR head 249fe750e

67 cases: 54 pass, 13 fail. The failures represent two root causes, not thirteen
independent defects:

1. Three cases reproduce a later cgroup overwriting an already found ID.
2. Eight cases reproduce missing cache initialization for a child created by
   Falco's parent-side parser: six parameterized cases, installed host filtering,
   and downstream connection tracking. Two additional late-import cases show
   the same empty-cache behavior through explicitly modeled state insertion.

The cgroup-only diagnostic patch fixes exactly three failures (57 pass, 10 fail),
confirming that cache initialization is independent of cgroup iteration order.

The original plugin was run for 100 shuffled sanitizer iterations (6,700 test
executions); every iteration had the same 54/13 split, with no ASan/UBSan/leak
diagnostics. These are reproducible behavioral failures, not sanitizer crashes.

TID reuse, exec/execveat refresh, failed exec, both observed fork orders, a missing
parent event, thread clone, valid vfork ordering, and the supported layout controls
passed. This does not establish exhaustive correctness or CPU performance.

The downstream positive control records one connection with the correct ID when
both fork events are present. Omitting only the child event makes the production
handler return IGNORED and leaves the tracker empty. No Sensor service or network
delivery is involved; host filter bypass is not proof of a host signal reaching
Sensor.
