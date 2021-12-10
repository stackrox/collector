Build tools for kernel drivers
==============================

Building of kernel modules and eBPF probes is handled by the dockerfiles found in this directory.
Different flavors of builders exist to handle kernels with varying requirements

build-kos
---------
This script handles compilation of drivers provided in a task file.
