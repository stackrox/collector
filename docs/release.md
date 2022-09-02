# Release Process

There is a script at utilities/release.py which performs the steps in the first section and the first two steps of the second section.
Please keep in mind that this document and that script may have some differences.

**Create the collector image release branch**

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

**Tag and create the collector image release**

1. Increment or set the patch number and release environment variables (if not set).
  - `export COLLECTOR_PATCH_NUMBER=0`
  - `export COLLECTOR_RELEASE=3.8`
2. Tag and push the release.
  - `git tag "${COLLECTOR_RELEASE}.${COLLECTOR_PATCH_NUMBER}"`
  - `git push origin "${COLLECTOR_RELEASE}.${COLLECTOR_PATCH_NUMBER}"`
3. Create a corresponding PR in openshift/release. Go to a forked copy of the openshift/release repo
  - `git checkout master`
  - `git pull upstream master`
  - `git checkout -b "release-${COLLECTOR_RELEASE}"`
  - `cd ci-operator/config/stackrox/collector`
  - `cp stackrox-collector-master.yaml stackrox-collector-release-${COLLECTOR_RELEASE}.yaml`
  - Change "branch: master" to "branch: release-3.8" or whatever the release branch is in the new config
  - Remove the promotion stanza from the new config 
  - `cd ../../../..`
  - `make jobs`
  - `git add ...`
  - `git rm ...`
  - `git commit -m "Add config for release-${COLLECTOR_RELEASE}"`
  - `git push origin release-${COLLECTOR_RELEASE}` # Create the PR, get it approved and merged once the tests have passed.
4. Create a pull request to update the `COLLECTOR_VERSION` file in the [stackrox/stackrox](https://github.com/stackrox/stackrox/) repo with the newly create release after CI images have been built.

