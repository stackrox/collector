#!/usr/bin/env bash
# Creates a tgz bundle of all binary artifacts needed for collector-rhel

set -euo pipefail

die() {
    echo >&2 "$@"
    exit 1
}

image_exists() {
  if ! docker image inspect "$1" > /dev/null ; then
     die "Image file $1 not found."
  fi
}

INPUT_ROOT="$1"
BUILDER_IMAGE="$2"
MODULE_DIR="$3"
OUTPUT_BUNDLE="$4"

[[ -n "$INPUT_ROOT" && -n "$BUILDER_IMAGE" &&
   -n "$MODULE_DIR" && -n "$OUTPUT_BUNDLE" ]] \
   || die "Usage: $0 <input-root> <builder-image> <module-dir> <output-bundle>"
[[ -d "$INPUT_ROOT" ]] \
   || die "Input root directory doesn't exist or is not a directory."
[[ -d "$MODULE_DIR" ]] \
   || die "Module directory doesn't exist or is not a directory."

# Verify builder image exists
image_exists "${BUILDER_IMAGE}"

# Create tmp directory
bundle_root="$(mktemp -d)"
mkdir -p "${bundle_root}/usr/"{bin,lib64,local/bin,local/lib}
mkdir -p "${bundle_root}/kernel-modules"
chmod -R 755 "${bundle_root}"

echo "Bundle root dir: ${bundle_root}"

# Each line is equivalent to Dockerfile COPY
cp -p "${INPUT_ROOT}/libs/libsinsp-wrapper.so.rhel" "${bundle_root}/usr/local/lib/libsinsp-wrapper.so"
cp -p "${INPUT_ROOT}/scripts/bootstrap.sh" "${bundle_root}/bootstrap.sh"
cp -p "${INPUT_ROOT}/scripts/collector-wrapper.sh" "${bundle_root}/usr/local/bin/"
cp -p "${INPUT_ROOT}/NOTICE-collector.txt" "${bundle_root}/COPYING.txt"
cp -p "${INPUT_ROOT}/bin/collector.rhel" "${bundle_root}/usr/local/bin/collector"
cp -pr "${MODULE_DIR}" "${bundle_root}/kernel-modules"

# Files needed from the collector-builder image and associated
# destination path in collector-rhel image.
builder_filelist_src=(
    "usr/local/bin/curl"
    "usr/local/lib/libcurl.so.4"
    "usr/local/bin/gzip"
)
builder_filelist_dst=(
    "usr/bin"
    "usr/lib64"
    "usr/bin"
)
builder_tmp="$(mktemp)"
docker run -ii --rm --entrypoint /bin/sh "${BUILDER_IMAGE}" /dev/stdin \
> "${builder_tmp}" <<EOF
set -e ; tar -cpz ${builder_filelist_src[@]}
EOF

[[ -s "${builder_tmp}" ]] || die "Empty builder extracted file"

for ((i=0; i<${#builder_filelist_src[@]}; ++i)); do
  tar xz -C "${bundle_root}/${builder_filelist_dst[$i]}" \
      -f "${builder_tmp}" "${builder_filelist_src[$i]}"
done

# Remove intermediate file
rm "${builder_tmp}"

# Create artifact bundle
tar czv --file "$OUTPUT_BUNDLE" --directory "${bundle_root}" .

# Create checksum
sha512sum "${OUTPUT_BUNDLE}" > "${OUTPUT_BUNDLE}.sha512"
sha512sum --check "${OUTPUT_BUNDLE}.sha512"

# Clean up after success
rm -r "${bundle_root}"
