# Release Process


## Considerations

- All tags created during a release should *not* be annotated. The ref to an
  annotated tag (e.g. `refs/tags/tagname`) does not refer to the tagged commit,
  instead referring to a non-commit object representing the annoation. This can
  cause complications in CI builds on remote VMs.

## Automated release

A workflow for automated releases can be found in the 'Actions' tab of
GitHub. Once in said tab, look for the `Tag a new release` workflow in
the side bar, select it and use the `Run workflow` button on the far
right to trigger the tagging process, setting the `<Major>.<minor>`
version for the release in the menu that pops up or leaving it as 0.0.
The workflow will check the version input and adjust the major, minor
and patch versions to be used before creating any necessary branches
and tags. If left with the default value of 0.0, the workflow will
create a new minor release.

The recommended workflow is to first run in dry-mode and check the tags
and branches that will be used are correct in the `Summary` section of
the triggered workflow, then run it again without dry-mode to create
the actual release. With the tag pushed, the workflow for creating the
new version of collector should be triggered on its own.

## Manual release

**Note**: This release process should only be used if the automated
process fails.
---

### Create the collector image release branch

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
git tag "${COLLECTOR_RELEASE}.${COLLECTOR_PATCH_NUMBER}"
git push origin "${COLLECTOR_RELEASE}.${COLLECTOR_PATCH_NUMBER}"
```

7. Create and push a tag to the falcosecurity-libs repository

```sh
git submodule update --init falcosecurity-libs
cd falcosecurity-libs
git tag "${COLLECTOR_RELEASE}.${COLLECTOR_PATCH_NUMBER}"
git push origin "${COLLECTOR_RELEASE}.${COLLECTOR_PATCH_NUMBER}"
```

### Patch releases

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
git tag "${COLLECTOR_RELEASE}.${COLLECTOR_PATCH_NUMBER}"
git push --follow-tags
```

5. Create a pull request to update the `COLLECTOR_VERSION` file in the
   [stackrox/stackrox](https://github.com/stackrox/stackrox/) repo with the
   newly create release after CI images have been built.
