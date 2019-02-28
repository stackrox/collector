function activate_env_srbcache {
  TOPCACHE="$HOME/.srbcache"
  DISTCACHE="$TOPCACHE/dist"
}

function check_dir_home_srbcache {
  activate_env_srbcache
  test -d "$TOPCACHE"
}

function check_dir_srbcache_dist {
  activate_env_srbcache
  test -d "$DISTCACHE"
}

function ensure_dir_home_srbcache {
  if test "x$UID" = "x0" ; then
    echo "ERROR: Cannot create srbcache directory as root"
    exit 1
  fi
  activate_env_srbcache
  check_dir_home_srbcache || mkdir -p "$TOPCACHE" -m 777
  sudo chown -R $(id -un) "$TOPCACHE" && chmod -R a+rwX "$TOPCACHE"
}

function ensure_dir_srbcache_dist {
  activate_env_srbcache
  check_dir_srbcache_dist || mkdir -p "$DISTCACHE" -m 777
}

function load_srbcache_from_google_storage_bucket {
  ensure_dir_home_srbcache
  cd
  rm -f srbcache.tar srbcache.tar.gz
  gsutil -m cp gs://collector-build-cache/colfb/v1/srbcache.tar.gz ~/srbcache.tar.gz || gsutil -m cp gs://collector-build-cache/colfb/v1/srbcache.tar ~/srbcache.tar || true
  if test -f srbcache.tar -o -f srbcache.tar.gz ; then
    echo "extracting tar from storage bucket"
    tar xafz srbcache.tar.gz || tar xaf srbcache.tar || echo "error restoring tar"
    rm -f srbcache.tar srbcache.tar.gz
  fi
  ls -F  ~/.srbcache
  touch  ~/.srbcache/o
  du -sh ~/.srbcache/*
  rm     ~/.srbcache/o
}

function save_srbcache_to_google_storage_bucket {
  ensure_dir_home_srbcache
  cd
  tar cafz srbcache.tar.gz .srbcache
  gsutil -m cp ~/srbcache.tar.gz gs://collector-build-cache/colfb/v1/srbcache.tar.gz
  rm srbcache.tar.gz
}
