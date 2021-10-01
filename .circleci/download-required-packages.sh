#!/usr/bin/env bash

downloadBundles() {
  bucket=$1
  awk -v bucket="$bucket" '{print bucket "/bundle-" $1 ".tgz"}' <~/kobuild-tmp/all-kernel-versions \
    | gsutil -m cp -I ~/kobuild-tmp/bundles/ || true

  #If the download failed try again multiple times
  max_attempts=5 #If you can't download it after 5 attempts you probably can't download it
  for ((i=0;i<$max_attempts;i=i+1))
  do
    for bundle in `ls ~/kobuild-tmp/bundles/*.gstmp` #Backticks are better here as otherwise this loop is executed even if there are no matching files
    do
      rm $bundle
      kernel="$(echo $bundle | sed 's|^.*bundle-||')"
      kernel="$(echo $kernel | sed 's|.tgz_.gstmp||')"
      src="$(echo $kernel | awk -v bucket="$bucket" '{print bucket "/bundle-" $1 ".tgz"}')"
      echo $src | gsutil -m cp -I ~/kobuild-tmp/bundles/ || true
    done
  done

  num_failed_downloads="$(ls ~/kobuild-tmp/bundles/*.gstmp | wc -l)"
  echo "There are $num_failed_downloads failed downloads"

  if (($num_failed_downloads > 0)); then
    echo
    echo "There are $num_failed_downloads failed downloads"
    mv ~/kobuild-tmp/bundles/*.gstmp ~/kobuild-tmp/bundles/failed-downloads
    ls ~/kobuild-tmp/bundles/failed-downloads
    echo
  fi
}

mkdir -p ~/kobuild-tmp/bundles
downloadBundles $KERNEL_BUNDLES_BUCKET

if [[ -z "$CIRCLE_TAG" && "$CIRCLE_BRANCH" != "master" && ! -z "$KERNEL_BUNDLES_STAGING_BUCKET" ]]; then
  downloadBundles $KERNEL_BUNDLES_STAGING_BUCKET
fi

ls ~/kobuild-tmp/bundles/
