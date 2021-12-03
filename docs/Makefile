# Minimal makefile for Sphinx documentation
#

# You can set these variables from the command line, and also
# from the environment for the first two.
SPHINXOPTS    ?=
SPHINXBUILD   ?= sphinx-build
SOURCEDIR     = source
BUILDDIR      = build

# Put it first so that "make" without argument is like "make help".
help: sphynx-builder
	docker run --rm \
	-v "$(CURDIR):/docs" \
	sphinxdoc/sphinx:stackrox-collector \
	$(SPHINXBUILD) -M help "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)

.PHONY: help Makefile

# Catch-all target: route all unknown targets to Sphinx using the new
# "make mode" option.  $(O) is meant as a shortcut for $(SPHINXOPTS).
%: Makefile sphynx-builder doxygen
	docker run --rm \
	-v "$(CURDIR):/docs" \
	sphinxdoc/sphinx:stackrox-collector \
	$(SPHINXBUILD) -M $@ "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) -Dbreathe_projects.Collector=/docs/build-doxygen/xml $(O)

PHONY: doxygen-builder
doxygen-builder:
	docker build \
		--tag doxygen \
		-f $(CURDIR)/Dockerfile.doxygen \
		$(CURDIR)/

PHONY: doxygen
doxygen: doxygen-builder
	docker run --rm -it \
		-v "$(CURDIR)/..:/collector" \
		doxygen /collector/docs/Doxyfile

PHONY: clean
clean:
	rm -rf build/
	rm -rf build-doxygen/

PHONY: sphynx-builder
sphynx-builder:
	docker build \
		--tag sphinxdoc/sphinx:stackrox-collector \
		$(CURDIR)
