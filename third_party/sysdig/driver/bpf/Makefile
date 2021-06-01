#
# Copyright (c) 2013-2018 Draios Inc. dba Sysdig.
#
# This file is dual licensed under either the MIT or GPL 2. See
# MIT.txt or GPL.txt for full copies of the license.
#

always += probe.o

LLC ?= llc
CLANG ?= clang

KERNELDIR ?= /lib/modules/$(shell uname -r)/build

# DEBUG = -DBPF_DEBUG

# Begin StackRox
ifeq ($(shell grep -sq "^\s*struct\s\+audit_task_info\s\+\*audit;\s*$$" $(KERNELDIR)/include/linux/sched.h ; echo $$? ), 0)
	KBUILD_CPPFLAGS+= -DCOS_73_WORKAROUND
endif
# End StackRox

all:
	$(MAKE) -C $(KERNELDIR) M=$$PWD

clean:
	$(MAKE) -C $(KERNELDIR) M=$$PWD clean
	@rm -f *~

$(obj)/probe.o: $(src)/probe.c \
		$(src)/bpf_helpers.h \
		$(src)/filler_helpers.h \
		$(src)/fillers.h \
		$(src)/maps.h \
		$(src)/plumbing_helpers.h \
		$(src)/quirks.h \
		$(src)/ring_helpers.h \
		$(src)/types.h
	$(CLANG) $(LINUXINCLUDE) \
		$(KBUILD_CPPFLAGS) \
		$(KBUILD_EXTRA_CPPFLAGS) \
		$(DEBUG) \
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
