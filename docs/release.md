# Release Process

**Create the collector image release branch**

There is a script at utilities/release.py which needs to be updated to reflect the use of OSCI.
The script creates the tags for the release as well as the release branch

1. Navigate to the local stackrox/collector git repository directory on the master branch and ensure the local checked out version is up to date.
  - `git checkout master`
  - `git pull`
2. Set the release environment variable, which should be incremented from the previous released version.
  - `export COLLECTOR_RELEASE=3.8`
3. Drop any release candidate versions from the kernel-modules/MODULE_VERSION file
  - `git checkout -b <branch name>`
  - `vim kernel-modules/MODULE_VERSION`
  - `git add kernel-modules/MODULE_VERSION`
  - `git commit -m 'Drops MODULE_VERSION release candidate for release'`
  - `git push`
  - Create a PR for this change, merge once approved.
4. Create an internal release tag to mark on the master branch where we forked for the release.
  - `git tag "${COLLECTOR_RELEASE}.x"`
  - `git push origin "${COLLECTOR_RELEASE}.x"`
5. Create the release branch with an empty commit and push.
  - `git checkout -b "release-${COLLECTOR_RELEASE}"`
  - `git commit --allow-empty -m "Empty commit to diverge ${COLLECTOR_RELEASE} from master"`
  - `git push --set-upstream origin "release-${COLLECTOR_RELEASE}"`
6. Set the patch number and release environment variables (if not set). See the section "Patch releases" for patch releases
  - `export COLLECTOR_PATCH_NUMBER=0`
  - `export COLLECTOR_RELEASE=3.8`
7. Tag and push the release.
  - `git tag "${COLLECTOR_RELEASE}.${COLLECTOR_PATCH_NUMBER}"`
  - `git push origin "${COLLECTOR_RELEASE}.${COLLECTOR_PATCH_NUMBER}"`
  - `git push release-"${COLLECTOR_RELEASE}"`

**Create the config in openshift/release**

1. Set the release environment variable, which should be incremented from the previous released version.
  - `export COLLECTOR_RELEASE=3.8`
2. Create a release branch in openshift/release. Go to a forked copy of the openshift/release repo
  - `git checkout master`
  - `git pull upstream master`
  - `git checkout -b "release-${COLLECTOR_RELEASE}"`
3. Create a config for the release
  - `cd ci-operator/config/stackrox/collector`
  - `cp stackrox-collector-master.yaml stackrox-collector-release-${COLLECTOR_RELEASE}.yaml`
  - Change "branch: master" to "branch: release-3.8" or whatever the release branch is in the new config
  - Remove the promotion stanza from the new config 
  - Remove the non postsubmit: true tests from the new config
  - `cd ../../../..`
  - `make jobs`
4. Commit and push the changes
  - `git add ...`
  - `git rm ...`
  - `git commit -m "Add config for release-${COLLECTOR_RELEASE}"`
  - `git push origin release-${COLLECTOR_RELEASE}` # Create the PR, get it approved and merged.
5. Create a pull request to update the `COLLECTOR_VERSION` file in the [stackrox/stackrox](https://github.com/stackrox/stackrox/) repo with the newly create release after CI images have been built.

**Patch releases**

There is a script at utilities/tag-bumper.py for creating new tags for patch releases.
That script is out of date and will be updated.

1. Navigate to your local stackrox/collector repo 
2. git checkout release-"${COLLECTOR_RELEASE}"
3. Create a branch off of the release branch.
4. Make changes by cherry-picking or otherwise and commit changes.
5. Increment COLLECTOR_PATCH_NUMBER
6. Tag and push the patch
  - `git tag "${COLLECTOR_RELEASE}.${COLLECTOR_PATCH_NUMBER}"`
  - `git push origin "${COLLECTOR_RELEASE}.${COLLECTOR_PATCH_NUMBER}"`
  - `git push release-"${COLLECTOR_RELEASE}"`
7. Create a PR and merge into the release branch once approved
8. Create a pull request to update the `COLLECTOR_VERSION` file in the [stackrox/stackrox](https://github.com/stackrox/stackrox/) repo with the newly create release after CI images have been built.
