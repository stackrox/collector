# BTF probes support

- Status: Accepted
- Contributors: Dmitrii Dolgov, Mauro Ezequiel Moltrasio, Robby Cochran, Giles Hutton
- Date: 2023-03-03

## Problem

Currently, eBPF programs Collector uses for gathering information are build for
every single supported Linux kernel to make sure their correctness. It comes
from the fact that the probe operates with data structures that could be
changed between different Linux kernel versions, and introduces too much of
maintenance burden and not scalable in long term. For example when a kernel
version is updated, Collector needs to fetch a new eBPF probe, creating a risk
of downtime. Another issue is expanding the support matrix requires a lot of
efforts, since new probes have to be built.

## Solution

To reduce number of eBPF programs they have to be built with BTF support. That
means each involved eBPF program:

* has to be built with BTF information about the program itself, and loaded
  together with this information.

* has to apply CO-RE approach and use exclusively relocatable functions to work
  with kernel data.

There are a couple of ways to do that, and the solution we decided to commit to
is the following:

* The implementation of BTF support is going to be a multistep project. The
  MVP step is going to cover immediate needs and be used for gathering
  operational experience. The subsequent steps will be about improving
  trade-offs made while implementing MVP.

* The first step is to adopt "modern" Falco probe engine for Collector as is,
  without any changes or customizations, and make it a new collection method of
  the same sort as "module" or "ebpf". This will allow us to benefit from
  existing functionality, giving an opportunity to iron out infrastructure
  about how to actually run those new probes.

* The next immediate step would be to apply our customizations that we do for
  regular eBPF probes.

* The following steps will be most likely about making our probes more
  flexible and utilize more BPF features. This will be defined in a separate
  [ADR][adr-probe-delivery].

## Pros & cons

Modern Falco [probe](modern_bpf) is a recently developed alternative eBPF
engine for the Falco project. It uses `libbpf`, including [CO-RE
helpers](co-re-helpers), and manages the probe lifecycle via `libpman`, which
is a wrapper around `libbpf`.

The obvious advantage of using it -- BTF support by default, answering all our
initial functionality requirements. By leveraging it we could concentrate on
other part needed to support BTF in Collector.

There are few disadvantages as well:

* Modern probe targets only reasonably new kernels (>= 5.8), not carrying over
  any compatibility baggage. It’s implemented via simple version checking,
  which means the vanilla modern probe is not going to work on older kernels
  even if they have necessary stuff back patched. More, some parts of the
  modern probe are build for "newish" syscalls that are not back patched at
  all, and there is no way to disable those parts. So it will require some
  efforts to make it work on older kernels.

* The probe is relatively new, and there are concerns about how well it’s
  tested. Especially since it comes with quite a bunch of changes in userspace
  libraries. Falco 0.34 released a couple of days ago, and it is the first
  release that ships the modern probe as an experimental opt-in feature, so
  battle testing has just started.

* It still uses a single entry/exit point for all syscalls, which means the
  performance will be lower than with the custom probe. On the other hand they
  use a bit different architecture, e.g. bpf ring buffers instead of perf ring
  buffers, which could also affect performance. The probe is attached to BTF
  aware tracepoints `tp_btf` rather than regular tracepoints (no functional
  difference).

## Considered (and rejected) alternatives

Another way of achieve BTF aware probes would be to modify custom eBPF probes.
The custom probes we use now relies on certain [bits of Falco][regular_bpf]:
eBPF fillers and maps, utilities to work with them, etc. But instead of
utilizing a single point of entry/exit for every event we attach the custom
probe only to a subset of syscalls of interest to ACS.

It's possible to modify the custom probe to make it BTF aware and loadable via
`libbpf`. This will require:

* Building the probe and Collector with `libbpf`

* Modifying the probe to comply with `libbpf` restrictions, e.g. using proper
  name of sections, dealing with auto-load/auto-attach etc.

* Replacing all direct memory fiddling with CO-RE helpers.

* Figuring out what still makes sense to keep from Falco utilities that the
  original probe was using (some parts will become obsolete using CO-RE
  helpers).

* Testing that the rest of bits we use from Falco (including all quirks needed
  for compatibility with older kernels) works as expected. Those bits were not
  designed to be used this way, which means some risks are applied.

This approach will give us much better performance, but will also require much
more efforts to implement and troubleshoot, leaving more risks for potential
bugs.

## Steps for implementation

To implement this we would need to:

* Sync our Falco fork with the latest state of the art and rebase all our
  changes on top of it.

* Enable the new engine and implement necessary customizations (ideally
  upstream) to run it on major platforms we support, e.g. OCP 4.12 . This will
  require lifting the lowest supported kernel limitation, and customize process
  of including eBPF programs into the final probe to avoid interacting with
  unsupported syscalls.

* Modify build pipeline and probe delivery mechanism. This is a topic of a
  separate [ADR][adr-probe-delivery].

* Collector has to be backward compatible in the sense that if it failed to
  load the new probe, it has to fallback to loading the legacy probes. This
  comes hand in hand with the necessity for Collector to have some heuristic to
  identify if BTF is supported or not. On top of simple heuristics we would
  need to implement a set of smoke tests at runtime to make sure the probe is
  functioning, e.g. to run a canary process and verify its impact is captured
  via the probe.

* Since the new probe is going to be loaded via `libbpf`, the Collector container
  has to have BTF for `vmlinux` mounted from the host. The usual path is
  `/sys/kernel/btf/vmlinux`, and `libbpf` will be able to pick it up
  automatically using this path. It's already present inside the Collector
  container, because of it's privileged nature, but still has to be proven if
  it's the same `vmlinux` we get by mounting the host `sysfs`. Most likely would
  have to tell `libbpf` exactly where to find it, which will require extending
  modern probe to pass loading options to `libbpf`.

* Ideally the loading logic has to be refactored to make it more flexible and
  potentially allow delegating probe loading to an external component (e.g.
  [bpfd][bpfd]).

* Report any fallbacks in the Collector pod status. Now it reports only missing
  probes, it makes sense to say something like “was able to get legacy probes
  running, although I was trying new ones first”. Eventually we would like to
  deprecate older approaches, so it might be worthwhile in the future  to wire
  this status to Central and into diagnostic bundles as well.

* Central Helm configuration has to be updated to include: new mount to make
  BTF `vmlinux` available, and new value for collection method option,
  “modern_bpf”

* The documentation references have to be updated to cover the major trade-offs
  that will happen in MVP about the performance.

## Compatibility considerations

BTF support implemented as proposed in this document has few dependencies: BTF
`vmlinux`, BPF ring buffer and basic CO-RE helpers. All three have to come
together, which means the following implication on oldest supported versions:

* The upstream Linux kernel features `/sys/kernel/btf/vmlinux` since 5.4. It
  means that without back patching Linux distros can provide BTF support since
  this version.

* Not all do, unfortunately. An overview since when some major distros provide
  BTF `vmlinux` could be found [here][vmlinux-btf-overview].

* BPF ring buffers were introduced upstream in the Linux kernel in
  [5.8][ring-buffers].

* RHEL seems to support BTF and BPF ring buffers since [8.7][rhel-backpatch]
  (?) (Linux kernel 4.18.0-425, with back patches).

To summarize, new probes would be supported: starting from RHEL 8.7 (?) thanks
for back patches for other distros less deviating from the kernel mainline,
starting from those featuring Linux kernel 5.8 for other important distros with
back patching we need to figure out the exact cut-off line. BTF aware probes
should work on various architectures as well. The only internal difference is
alignment requirements for BTF data, but it seems to be handled by the kernel
itself.

[modern_bpf]: https://github.com/falcosecurity/libs/tree/master/driver/modern_bpf
[co-re-helpers]: https://github.com/falcosecurity/libs/blob/master/driver/modern_bpf/helpers/base/read_from_task.h#L49
[regular_bpf]: https://github.com/falcosecurity/libs/tree/master/driver/bpf
[bpfd]: https://github.com/redhat-et/bpfd/
[vmlinux-btf-overview]: https://github.com/aquasecurity/btfhub/blob/main/docs/supported-distros.md
[ring-buffers]: https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/commit/?id=457f44363a8894135c85b7a9afd2bd8196db24ab
[rhel-backpatch]: https://access.redhat.com/documentation/en-us/red_hat_enterprise_linux/8/html/8.7_release_notes/available_bpf_features
[adr-probe-delivery]: 0002-modern-probe-delivery.md
