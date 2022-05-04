# Release Process

**Create the collector image release branch**

1. Navigate to the local stackrox/collector git repository directory on the master branch and ensure the local checked out version is up to date.
  - `git checkout master`
  - `git pull`
2. Set the release environment variable, which should be incremented from the previous released version.
  - `export RELEASE=3.8`
3. Create an internal release tag to mark on the master branch where we forked for the release.
  - `git tag "${RELEASE}.x"`
  - `git push origin "${RELEASE}.x"`
4. Create the release branch with an empty commit and push.
  - `git checkout -b "release/${RELEASE}.x"`
  - `git commit --allow-empty -m "Empty commit to diverge ${RELEASE} from master"`
  - `git push --set-upstream origin "release/${RELEASE}.x"`

**Tag and create the collector image release**

1. Increment or set the patch number and release environment variables (if not set).
  - `export PATCH_NUMBER=0`
  - `export RELEASE=3.8`
2. Tag and push the release. Check CI to ensure the triggered builds complete.
  - `git tag "${RELEASE}.${PATCH_NUMBER}"`
  - `git push origin "${RELEASE}.${PATCH_NUMBER}"`
3. Create a pull request to update the `COLLECTOR_VERSION` file in the [stackrox/stackrox](https://github.com/stackrox/stackrox/) repo with the newly create release after CI images have been built.

