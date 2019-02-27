function do_target_deps {
  test -f Gopkg.toml
  test -f Gopkg.lock
  if test ! -f deps ; then
    echo "+ deps"
    # `dep check` exits with a nonzero code if there is a toml->lock mismatch.
    dep check -skip-vendor
    # `dep ensure` can be flaky sometimes, so try rerunning it if it fails.
    dep ensure || (rm -rf .vendor-new && dep ensure)
    touch deps
  fi
}

function do_target_clean_deps {
  echo "+ clean-deps"
  rm -f deps
  sudo rm -f /tmp/collector-test.db
}
