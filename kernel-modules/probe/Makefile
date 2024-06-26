# This file is largely duplicated from the Falco version, adjusted slightly for our specific
# source file and includes.
# See here for the original:
#   https://github.com/falcosecurity/libs/blob/2c8335ae64f1e775def9f9f7c97e5340319c85f0/driver/bpf/Makefile

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
# BPF_DEBUG_SYMBOLS = -g

# Get the path of the module sources
#
FIRST_MAKEFILE := $(firstword $(MAKEFILE_LIST))
FIRST_MAKEFILE_FILENAME := $(notdir $(FIRST_MAKEFILE))
FIRST_MAKEFILE_DIRNAME := $(shell basename $(dir $(FIRST_MAKEFILE)))

ifeq ($(FIRST_MAKEFILE_DIRNAME)/$(FIRST_MAKEFILE_FILENAME), scripts/Makefile.build)

# Build phase
MODULE_MAKEFILE_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
MAKEFILE_INC_FILES := $(shell find $(FALCO_DIR) -type f -name Makefile.inc)
$(info [configure-bpf] Including $(MAKEFILE_INC_FILES))
include $(MAKEFILE_INC_FILES)

endif

#
# https://chromium.googlesource.com/chromiumos/third_party/kernel/+/096925a44076ba5c52faa84d255a847130ff341e%5E%21/#F2
# This commit diverged the ChromiumOS kernel from stock in the area of audit information, which this probe accesses.
#
# This enables the workaround for this divergence.
#
NEEDS_COS_73_WORKAROUND = $(shell expr `grep -sc "^\s*struct\s\+audit_task_info\s\+\*audit;\s*$$" $(KERNELDIR)/include/linux/sched.h` = 1)
ifeq ($(NEEDS_COS_73_WORKAROUND), 1)
	KBUILD_CPPFLAGS += -DCOS_73_WORKAROUND
endif

# -fmacro-prefix-map is not supported on version of clang older than 10
# so remove it if necessary.
IS_CLANG_OLDER_THAN_10 := $(shell expr `$(CLANG) -dumpversion | cut -f1 -d.` \<= 10)
ifeq ($(IS_CLANG_OLDER_THAN_10), 1)
	KBUILD_CPPFLAGS := $(filter-out -fmacro-prefix-map=%,$(KBUILD_CPPFLAGS))
endif

# Optimization, pass only cpu & mem cgroups from the kernel side via bpf
KBUILD_CPPFLAGS += -DBPF_SKIP_CPUSETS
KBUILD_CPPFLAGS += -DBPF_SKIP_CPUACCT
KBUILD_CPPFLAGS += -DBPF_SKIP_BLK_CGROUP

all:
	$(MAKE) -C $(KERNELDIR) M=$$PWD

clean:
	$(MAKE) -C $(KERNELDIR) M=$$PWD clean
	$(MAKE) -C $(FALCO_DIR) clean
	@rm -f *~

$(obj)/probe.o: $(src)/collector_probe.c $(FALCO_DIR)/*.h
	$(CLANG) $(LINUXINCLUDE) \
		$(KBUILD_CPPFLAGS) \
		$(KBUILD_EXTRA_CPPFLAGS) \
		$(DEBUG) \
		-I $(FALCO_DIR) \
		-I $(FALCO_DIR)/../ \
		-D__KERNEL__ \
		-DKBUILD_MODNAME=\"collector\" \
		-D__BPF_TRACING__ \
		-Wno-gnu-variable-sized-type-not-at-end \
		-Wno-address-of-packed-member \
		-fno-jump-tables \
		-fno-stack-protector \
		-Wno-tautological-compare \
		-O2 $(BPF_DEBUG_SYMBOLS) -emit-llvm -c $< -o $(patsubst %.o,%.ll,$@)
	$(LLC) -march=bpf -filetype=obj -o $@ $(patsubst %.o,%.ll,$@)
