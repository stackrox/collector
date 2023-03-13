# BTF probes delivery mechanism

- Status: Proposed
- Contributors: Dmitrii Dolgov, Mauro Ezequiel Moltrasio, Robby Cochran, Giles Hutton
- Date: 2023-03-13


## Problem

Current probes delivery mechanism can not fully benefit from BTF aware probes.
It is designed with large number of probes in mind and intrinsically rely on
storing them somewhere outside the node Collector running on (it could be
support packages or a GCP bucket). This approach, required for the regular BPF
probes and kernel modules, makes Collector startup process more fragile and
raises security concerns. At the same time there is only limited number of BTF
aware probes needed, and using the same mechanism means to carry on robustness
issues, but taking no benefits of it.

## Solution

To make startup process more robust, embed the BTW aware probes into Collector
images. That means:

* BTF aware probe has to be built as part of Collector build process. It's
  possible to build it separately and fetch during the Collector build, but
  doing everything together simplifies the process.

* BTF aware probe has to be a part of Collector image. Since Falco modern bpf
  engine uses `libbpf` skeletons to manage the probe, it means as the first
  step we go with the probe being a part of Collector binary.

* In the long term scenario we would like to have probes being separate from
  the Collector binary itself. This gives more flexibility about how to manage
  the probe lifecycle, how to do update and hot reload.

* Collector has to be flexible about how to get the probes. For compatibility
  reasons the regular BPF probes still have to be downloaded. Generally BTF
  aware probes would be present in the image, but there may be cases when we
  would like to download them as well, something like an option
  `COLLECTOR_DOWNLOAD_BTF`, e.g. for testing purposes or quick updates.

## Pros & cons

There are obvious advantages of using the described approach:

* BPF probes delivery is getting more transparent for security analysis,
  they're a part of the image.

* Risks of something going wrong during the Collector startup are significantly
  reduced.

* Working with Collector in development and testing environments will become
  easier.

Few disadvantages:

* This approach differs significantly from what is already in place, so it will
  require more efforts to implement.

* The probe release process will be tightly coupled with the Collector release
  process.

## Considered (and rejected) alternatives

Another way of doing this would be simply to use the same delivery mechanism as
before, making necessary adjustments to the probe location path structure (to
distinguish regular vs BTF aware probes).

The only few advantages of this approach are:

* Not much efforts to implement.

* Probes release process is independent of the Collector release process.

Both are pale in comparison with disadvantages of carrying on the legacy
baggage of regular BPF probes and kernel modules.

## Steps for implementation

* Modify the build pipeline to build the new probe together with the Collector
  binary using `libbpf` skeleton.

* Teach Collector to load it using Falco modern bpf engine.

* In the future separate BTF aware probe from the Collector binary, but still keep it embedded into the image.

About the last point, it looks like it's not that easy to achieve it. The point
is that the skeleton API `libbpf` exposes doesn't allow changing the way to the
BPF data is acquired. At the moment it's being embedded into the skeleton
header file, then mmap'ed into the memory and opened via
[bpf_object__open_mem][bpf_object__open_mem]. Quick experiments show, that
replacing `__open_mem` function with `bpf_object__open_file` with a specified
probe path actually does what we need. This could be done either by duplicating
part of `libbpf` functionality to replace skeleton load logic in `libpman`, or
try to reload `skel->obj` after the fact (which didn't work in my tests).

[bpf_object__open_mem]: https://github.com/libbpf/libbpf/blob/master/src/libbpf.c#L12345
