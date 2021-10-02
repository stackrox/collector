#!/usr/bin/env bash

downloadBundles() {
  bucket=$1
  awk -v bucket="$bucket" '{print bucket "/bundle-" $1 ".tgz"}' <~/kobuild-tmp/all-kernel-versions \
    | gsutil -m cp -I ~/kobuild-tmp/bundles/ || true

  # If the download failed try again multiple times
  max_attempts=5 #If you can't download it after 5 attempts you probably can't download it
  for ((i=0;i<$max_attempts;i=i+1))
  do
    shopt -s nullglob
    for bundle in ~/kobuild-tmp/bundles/*.gstmp #Failed downloads end in .gstmp
    do
      rm $bundle
      kernel="$(echo $bundle | sed 's|^.*bundle-||')"
      kernel="$(echo $kernel | sed 's|.tgz_.gstmp||')"
      src="${bucket}/bundle-${kernel}.tgz"
      gsutil cp "$src" ~/kobuild-tmp/bundles/ || true
    done

    shopt -u nullglob
    num_failed_downloads="$(ls ~/kobuild-tmp/bundles/*.gstmp 2>/dev/null | wc -l || true)"
    if (( num_failed_downloads > 0 )); then
      sleep 30
    else
      break
    fi

  done

  if (( num_failed_downloads > 0 )); then
    echo
    echo "There are $num_failed_downloads failed downloads"
    mkdir ~/kobuild-tmp/bundles/failed-downloads/
    mv ~/kobuild-tmp/bundles/*.gstmp ~/kobuild-tmp/bundles/failed-downloads/
    ls ~/kobuild-tmp/bundles/failed-downloads
    echo
  fi
}

mkdir -p ~/kobuild-tmp/bundles
downloadBundles "$KERNEL_BUNDLES_BUCKET"

if [[ -z "$CIRCLE_TAG" && "$CIRCLE_BRANCH" != "master" && ! -z "$KERNEL_BUNDLES_STAGING_BUCKET" ]]; then
  downloadBundles "$KERNEL_BUNDLES_STAGING_BUCKET"
fi
