FSTAR_EXE = opam exec -- fstar.exe
FSTAR_FLAGS = --z3rlimit 80 --include fstar --include /home/ghutton/.opam/default/lib/fstar/pulse
KRML_EXE = $(HOME)/code/go/src/github.com/FStarLang/FStar/karamel/krml
KRML_BASE_FLAGS = -skip-linking -backend c -warn-error -2-15 -minimal -tmpdir extracted

MODULES = NetworkOps NRadixPulse
SPEC_FILES = fstar/NRadixSpec.fst
FST_FILES = fstar/NetworkTypes.fst $(SPEC_FILES) $(foreach m,$(MODULES),fstar/$(m).fst)
EXTRACTED_FILES = $(foreach m,$(MODULES),extracted/$(m).c extracted/$(m).h)

.PHONY: verify extract clean all $(foreach m,$(MODULES),verify-$(m) extract-$(m))

all: extract

verify:
	@for f in $(FST_FILES); do \
		echo "=== Verifying $$f ===" && \
		$(FSTAR_EXE) $(FSTAR_FLAGS) --cache_checked_modules $$f || exit 1; \
	done

verify-NetworkOps:
	$(FSTAR_EXE) $(FSTAR_FLAGS) --cache_checked_modules fstar/NetworkTypes.fst
	$(FSTAR_EXE) $(FSTAR_FLAGS) --cache_checked_modules fstar/NetworkOps.fst

extract-NetworkOps: verify-NetworkOps
	$(FSTAR_EXE) $(FSTAR_FLAGS) --codegen krml --extract 'NetworkTypes,NetworkOps' fstar/NetworkOps.fst
	mv out.krml out_NetworkOps.krml
	rm -f extracted/NetworkOps.c extracted/NetworkOps.h extracted/Prims.h extracted/Makefile.include
	$(KRML_EXE) $(KRML_BASE_FLAGS) \
		-bundle 'NetworkTypes' \
		-bundle 'NetworkOps=NetworkOps' \
		out_NetworkOps.krml
	@echo "=== NetworkOps extraction complete ==="

verify-NRadixPulse:
	$(FSTAR_EXE) $(FSTAR_FLAGS) --cache_checked_modules fstar/NetworkTypes.fst
	$(FSTAR_EXE) $(FSTAR_FLAGS) --cache_checked_modules fstar/NRadixSpec.fst
	$(FSTAR_EXE) $(FSTAR_FLAGS) --cache_checked_modules fstar/NRadixPulse.fst

extract-NRadixPulse: verify-NRadixPulse
	$(FSTAR_EXE) $(FSTAR_FLAGS) --codegen krml \
		--extract 'NetworkTypes,NRadixSpec,NRadixPulse,FStar.Pervasives.Native' \
		fstar/NRadixPulse.fst
	mv out.krml out_NRadixPulse.krml
	rm -f extracted/NRadixPulse.c extracted/NRadixPulse.h extracted/Prims.h extracted/Makefile.include
	$(KRML_EXE) $(KRML_BASE_FLAGS) \
		-bundle 'NetworkTypes' \
		-bundle 'NRadixPulse=NRadixPulse,NRadixSpec,FStar.Pervasives.Native' \
		out_NRadixPulse.krml
	@echo "=== NRadixPulse extraction complete ==="

extract: verify
	$(FSTAR_EXE) $(FSTAR_FLAGS) --codegen krml --extract 'NetworkTypes,NetworkOps' fstar/NetworkOps.fst
	mv out.krml out_NetworkOps.krml
	$(FSTAR_EXE) $(FSTAR_FLAGS) --codegen krml \
		--extract 'NetworkTypes,NRadixSpec,NRadixPulse,FStar.Pervasives.Native' \
		fstar/NRadixPulse.fst
	mv out.krml out_NRadixPulse.krml
	rm -f $(EXTRACTED_FILES) extracted/Prims.h extracted/Makefile.include
	$(KRML_EXE) $(KRML_BASE_FLAGS) \
		-bundle 'NetworkTypes' \
		-bundle 'NetworkOps=NetworkOps' \
		-bundle 'NRadixPulse=NRadixPulse,NRadixSpec,FStar.Pervasives.Native' \
		out_NetworkOps.krml out_NRadixPulse.krml
	@echo "=== Extraction complete ==="
	@grep -c "krml_checked_int" $(EXTRACTED_FILES) 2>/dev/null \
		&& echo "WARNING: mathematical integers leaked into extracted code" || true

clean:
	rm -f $(EXTRACTED_FILES) extracted/Prims.h extracted/Makefile.include
	rm -f fstar/*.checked out.krml $(foreach m,$(MODULES),out_$(m).krml)
