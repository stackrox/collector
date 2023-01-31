# Release Process

**Create the collector image release branch**

1. Navigate to the local stackrox/collector git repository directory on the master branch and ensure the local checked out version is up to date.

```sh
git checkout master
git pull
```

2. Set the release environment variable, which should be incremented from the previous released version.

```sh
export COLLECTOR_RELEASE=3.8
```

3. Drop any release candidate versions from the kernel-modules/MODULE_VERSION file

```sh
git checkout -b <branch name>
vim kernel-modules/MODULE_VERSION
git add kernel-modules/MODULE_VERSION
git commit -m 'Drops MODULE_VERSION release candidate for release'
git push
```

Create a PR for this change, merge once approved.

4. Create an internal release tag to mark on the master branch where we forked for the release.

```sh
git tag "${COLLECTOR_RELEASE}.x"
git push origin "${COLLECTOR_RELEASE}.x"
```

5. Create the release branch with an empty commit and push.

```sh
git checkout -b "release-${COLLECTOR_RELEASE}"
git commit --allow-empty -m "Empty commit to diverge ${COLLECTOR_RELEASE} from master"
git push --set-upstream origin "release-${COLLECTOR_RELEASE}"
```

6. Set the patch number and release environment variables (if not set).
   See the section "Patch releases" for patch releases

```sh
export COLLECTOR_PATCH_NUMBER=0
export COLLECTOR_RELEASE=3.8
```

7. Tag and push the release.

```sh
git tag -a "${COLLECTOR_RELEASE}.${COLLECTOR_PATCH_NUMBER}"
git push origin "${COLLECTOR_RELEASE}.${COLLECTOR_PATCH_NUMBER}"
```

8. Create and push a tag to the falcosecurity-libs repository

```sh
export DRIVER_VERSION="v$(cat kernel-modules/MODULE_VERSION)"
git submodule update --init falcosecurity-libs
cd falcosecurity-libs
git tag "${DRIVER_VERSION}"
git push origin "${DRIVER_VERSION}"
```

**Patch releases**

There is a script at utilities/tag-bumper.py for creating new tags for patch releases.
That script is out of date and will be updated.

1. Navigate to your local stackrox/collector repo and run:

```sh
git checkout release-"${COLLECTOR_RELEASE}"
```

2. Make changes by cherry-picking or otherwise and commit changes.
3. Increment COLLECTOR_PATCH_NUMBER
4. Tag and push the patch

```sh
git tag -a "${COLLECTOR_RELEASE}.${COLLECTOR_PATCH_NUMBER}"
git push --follow-tags
```

5. Create a pull request to update the `COLLECTOR_VERSION` file in the
   [stackrox/stackrox](https://github.com/stackrox/stackrox/) repo with the
   newly create release after CI images have been built.
