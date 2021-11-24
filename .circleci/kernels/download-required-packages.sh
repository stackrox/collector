#!/usr/bin/env bash
set -eo pipefail

downloadBundlesListedInFile() {
  bucket=$1
  downloads_file=$2

  awk -v bucket="$bucket" '{print bucket "/bundle-" $1 ".tgz"}' <"$downloads_file" \
    | gsutil -m cp -I "$bundles_dir" || true

}

getNumFailedDownloads() {
    find "$bundles_dir"/*.gstmp 2>/dev/null | wc -l || true
}

reportIfFailed() {
  num_failed_downloads="$(getNumFailedDownloads)"
  if (( num_failed_downloads > 0 )); then
    echo
    echo "There are $num_failed_downloads failed downloads"
    failed_downloads_dir="$bundles_dir/failed-downloads/"
    mkdir "$failed_downloads_dir"
    mv "$bundles_dir"/*.gstmp "$failed_downloads_dir"
    ls "$failed_downloads_dir"/
    echo
  fi
}

retryFailedDownloads() {
  bucket=$1

  max_attempts=5 #If you can't download it after 5 attempts you probably can't download it
  for ((i=0;i<max_attempts;i=i+1))
  do
    num_failed_downloads="$(getNumFailedDownloads)"
    if (( num_failed_downloads == 0 )); then
      break
    fi
    sleep 30

    failed_downloads_file="$KOBUILD_DIR/failed_downloads.txt"
    ls "$bundles_dir"/*.gstmp > "$failed_downloads_file"
    sed -i 's|^.*bundle-||' "$failed_downloads_file"
    sed -i 's|\.tgz_\.gstmp$||' "$failed_downloads_file"

    downloadBundlesListedInFile "$bucket" "$failed_downloads_file"
  done
  reportIfFailed
  }

downloadBundles() {
  bucket=$1

  downloadBundlesListedInFile "$bucket" "$KERNELS_FILE"
  retryFailedDownloads "$bucket"

}

TAG=$1
BRANCH=$2
KOBUILD_DIR="${3:-~/kobuild-tmp}"
KERNELS_FILE="${4:-"$KOBUILD_DIR/all-kernel-versions"}"

bundles_dir="$KOBUILD_DIR/bundles"

mkdir -p "$bundles_dir"
downloadBundles "$KERNEL_BUNDLES_BUCKET"

if [[ -z "$TAG" && "$BRANCH" != "master" && -n "$KERNEL_BUNDLES_STAGING_BUCKET" ]]; then
  downloadBundles "$KERNEL_BUNDLES_STAGING_BUCKET"
fi
