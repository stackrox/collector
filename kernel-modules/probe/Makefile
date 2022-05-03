#
# Copyright (C) 2021 The Falco Authors.
#
# This file is dual licensed under either the MIT or GPL 2. See
# MIT.txt or GPL.txt for full copies of the license.
#

always-y += probe.o
# kept for compatibility with kernels < 5.11
always = $(always-y)

LLC ?= llc
CLANG ?= clang

KERNELDIR ?= /lib/modules/$(shell uname -r)/build

ifndef FALCO_DIR
$(error FALCO_DIR must be set)
endif

# DEBUG = -DBPF_DEBUG

# Begin StackRox
ifeq ($(shell grep -sq "^\s*struct\s\+audit_task_info\s\+\*audit;\s*$$" $(KERNELDIR)/include/linux/sched.h ; echo $$? ), 0)
	KBUILD_CPPFLAGS+= -DCOS_73_WORKAROUND
endif
# clang-7 does not support -fmacro-prefix-map
KBUILD_CPPFLAGS:=$(filter-out -fmacro-prefix-map=%,$(KBUILD_CPPFLAGS))
# End StackRox

all:
	$(MAKE) -C $(KERNELDIR) M=$$PWD

clean:
	$(MAKE) -C $(KERNELDIR) M=$$PWD clean
	@rm -f *~

$(obj)/probe.o: $(src)/collector_probe.c
	$(CLANG) $(LINUXINCLUDE) \
		$(KBUILD_CPPFLAGS) \
		$(KBUILD_EXTRA_CPPFLAGS) \
		$(DEBUG) \
		-I $(FALCO_DIR) \
		-D__KERNEL__ \
		-DKBUILD_MODNAME=\"collector\" \
		-D__BPF_TRACING__ \
		-Wno-gnu-variable-sized-type-not-at-end \
		-Wno-address-of-packed-member \
		-fno-jump-tables \
		-fno-stack-protector \
		-Wno-tautological-compare \
		-O2 -g -emit-llvm -c $< -o $(patsubst %.o,%.ll,$@)
	$(LLC) -march=bpf -filetype=obj -o $@ $(patsubst %.o,%.ll,$@)
