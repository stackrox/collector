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

3. Create an internal release tag to mark on the master branch where we forked for the release.

```sh
git tag "${COLLECTOR_RELEASE}.x"
git push origin "${COLLECTOR_RELEASE}.x"
```

4. Create the release branch with an empty commit and push.

```sh
git checkout -b "release-${COLLECTOR_RELEASE}"
git commit --allow-empty -m "Empty commit to diverge ${COLLECTOR_RELEASE} from master"
git push --set-upstream origin "release-${COLLECTOR_RELEASE}"
```

5. Set the patch number and release environment variables (if not set).
   See the section "Patch releases" for patch releases

```sh
export COLLECTOR_PATCH_NUMBER=0
export COLLECTOR_RELEASE=3.8
```

6. Tag and push the release.

```sh
git tag -a "${COLLECTOR_RELEASE}.${COLLECTOR_PATCH_NUMBER}"
git push origin "${COLLECTOR_RELEASE}.${COLLECTOR_PATCH_NUMBER}"
```

7. Create and push a tag to the falcosecurity-libs repository

```sh
git submodule update --init falcosecurity-libs
cd falcosecurity-libs
git tag -a "${COLLECTOR_RELEASE}.${COLLECTOR_PATCH_NUMBER}"
git push origin "${COLLECTOR_RELEASE}.${COLLECTOR_PATCH_NUMBER}"
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
