# Design overview

## Data gathering methods
### Kernel drivers
Collector leverages eBPF probes and kernel modules for gathering data directly
from the kernel.

#### Driver candidates
When collector starts up, it will automatically try to download a driver
compatible with the running kernel. For most distributions this will be
determined with the `uname -r` command, but some special cases exist. In such
cases, collector will create a set of potential candidates and try to find them
sequentially, first looking for them in the filesystem and then trying to
download them.

In cases where the auto-generated list of candidates are failing to create a
working option, the `KERNEL_CANDIDATES` environment variable can be used to
bypass this mechanism altogether.

## Falco libraries integration

### Chisel

## Resource consumption breakdown

### Afterglow

## Place in the bigger scheme of things
