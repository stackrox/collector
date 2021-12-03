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
