#!/usr/bin/env bash
set -eo pipefail

downloadBundlesListedInFile() {
  bucket=$1
  downloads_file=$2

  awk -v bucket="$bucket" '{print bucket "/bundle-" $1 ".tgz"}' <"$downloads_file" \
    | gsutil -m cp -I "$bundles_dir" || true

}

getNumFailedDownloads() {
    echo "$(ls "$bundles_dir"/*.gstmp 2>/dev/null | wc -l || true)"
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
  for ((i=0;i<$max_attempts;i=i+1))
  do
    num_failed_downloads="$(getNumFailedDownloads)"
    if (( num_failed_downloads == 0 )); then
      break
    fi
    sleep 30

    failed_downloads_file="$kobuild_dir/failed_downloads.txt"
    ls "$bundles_dir"/*.gstmp > "$failed_downloads_file"
    sed -i 's|^.*bundle-||' "$failed_downloads_file"
    sed -i 's|.tgz_.gstmp||' "$failed_downloads_file"

    downloadBundlesListedInFile "$bucket" "$failed_downloads_file"
  done
  reportIfFailed
  }

downloadBundles() {
  bucket=$1

  downloadBundlesListedInFile "$bucket" "$kobuild_dir/all-kernel-versions"
  retryFailedDownloads "$bucket"

}

kobuild_dir=$1

bundles_dir="$kobuild_dir/bundles"

mkdir -p "$bundles_dir"
downloadBundles "$KERNEL_BUNDLES_BUCKET"

if [[ -z "$CIRCLE_TAG" && "$CIRCLE_BRANCH" != "master" && ! -z "$KERNEL_BUNDLES_STAGING_BUCKET" ]]; then
  downloadBundles "$KERNEL_BUNDLES_STAGING_BUCKET"
fi
