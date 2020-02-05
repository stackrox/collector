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

extract_from_image() {
  image=$1
  src=$2
  dst=$3

  [[ -n "$image" && -n "$src" && -n "$dst" ]] \
      || die "extract_from_image: <image> <src> <dst>"

  docker run -ii --rm --entrypoint /bin/sh "${image}" /dev/stdin \
  > "${dst}" <<EOF
set -e
cat < ${src}
EOF

  [[ -s $dst ]] || die "file extracted from image is empty: $dst"
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

# Verify image exists
image_exists "${BUILDER_IMAGE}"

# Create tmp directory
bundle_root="$(mktemp -d)"
mkdir -p "${bundle_root}/usr/"{bin,lib64,local/bin,local/lib}
mkdir -p "${bundle_root}/kernel-modules"
chmod -R 755 "${bundle_root}"

# =============================================================================

# Add files to be included in the Dockerfile here. This includes artifacts that
# would be otherwise downloaded or included via a COPY command in the
# Dockerfile.

cp -p "${INPUT_ROOT}/libs/libsinsp-wrapper.so.rhel" "${bundle_root}/usr/local/lib/libsinsp-wrapper.so"
cp -p "${INPUT_ROOT}/scripts/bootstrap.sh" "${bundle_root}/bootstrap.sh"
cp -p "${INPUT_ROOT}/scripts/collector-wrapper.sh" "${bundle_root}/usr/local/bin/"
cp -p "${INPUT_ROOT}/NOTICE-collector.txt" "${bundle_root}/COPYING.txt"
cp -p "${INPUT_ROOT}/bin/collector.rhel" "${bundle_root}/usr/local/bin/collector"
cp -pr "${MODULE_DIR}" "${bundle_root}/kernel-modules"

# Files needed from the collector-builder image and associated
# destination in collector-rhel image.
builder_src=(
    "usr/local/bin/curl"
    "usr/local/bin/gzip"
    "usr/local/lib/libcurl.so"
    "usr/local/lib/libcurl.so.4"
)
builder_dst=(
    "usr/bin/curl"
    "usr/bin/gzip"
    "usr/lib64/libcurl.so"
    "usr/lib64/libcurl.so.4"
)

for ((i=0; i<${#builder_src[@]}; ++i)); do
  extract_from_image "${BUILDER_IMAGE}" \
      "${builder_src[$i]}" "${bundle_root}/${builder_dst[$i]}"
  chmod 755 "${bundle_root}/${builder_dst[$i]}"
done

# =============================================================================

# Files should have owner/group equal to root:root
if tar --version | grep -q "gnu" ; then
  tar_chown_args=("--owner=root:0" "--group=root:0")
else
  tar_chown_args=("--uid=root:0" "--gid=root:0")
fi

# Create output bundle of all files in $bundle_root
tar cz "${tar_chown_args[@]}" --file "$OUTPUT_BUNDLE" --directory "${bundle_root}" .

# Create checksum
sha512sum "${OUTPUT_BUNDLE}" > "${OUTPUT_BUNDLE}.sha512"
sha512sum --check "${OUTPUT_BUNDLE}.sha512"

# Clean up after success
rm -r "${bundle_root}"
