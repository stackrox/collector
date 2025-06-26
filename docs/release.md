# Release Process


## Considerations

- All tags created during a release should *not* be annotated. The ref to an
  annotated tag (e.g. `refs/tags/tagname`) does not refer to the tagged commit,
  instead referring to a non-commit object representing the annoation. This can
  cause complications in CI builds on remote VMs.

## Automated release

**Note**: If stackrox is doing a major version bump, do not use the
automated release workflow!! Follow the manual instructions below
instead.
---

A workflow for automated releases can be found in the 'Actions' tab of
GitHub. Once in said tab, look for the `Tag a new release` workflow in
the side bar, select it and use the `Run workflow` button on the far
right to trigger the release process.

| Parameter | Description |
| -------- | ----------- |
| Branch | Determines where to find the workflow file. Use `master` unless testing the automation. |
| Release version | A version of the form `<Major>.<Minor>`. |
| Do not push anything | Whether or not to actually create new branches/tags. |

* **if performing a new minor release**: keep the default settings. The workflow will
caclulate the next minor version.
* **if performing a patch release**: set the release version to the
`<Major>.<Minor>` of the release you're patching, e.g. `3.21`. The workflow will calculate
the next patch version.
* **if performing a new major release**: set the release version to the next
major version, e.g `4.0`.

The recommended workflow is to first run in dry-mode and check the tags
and branches are correct in the `Summary` section of the triggered run,
then run it again without dry-mode to create the actual release.

With the tag pushed, the workflow for creating the
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
export COLLECTOR_RELEASE=3.22
```

3. Create an internal release tag to mark on the master branch where we forked for the release.

```sh
git tag "${COLLECTOR_RELEASE}.x"
git push origin "${COLLECTOR_RELEASE}.x"
```

4. Set the ACS version suffix to be used by konflux, this should be the major and minor versions of ACS that will use the collector version being tagged.

```sh
export STACKROX_SUFFIX=4-8
```

4. Create the release branch with the required konflux suffixes.

```sh
git checkout -b "release-${COLLECTOR_RELEASE}"
sed -i \
    -e "/appstudio.openshift.io\/application: / s/$/-${STACKROX_SUFFIX}/" \
    -e "/appstudio.openshift.io\/component: / s/$/-${STACKROX_SUFFIX}/" \
    -e "/serviceAccountName: / s/$/-${STACKROX_SUFFIX}/" \
    .tekton/collector-build.yaml
git commit -m "Empty commit to diverge ${COLLECTOR_RELEASE} from master"
git push --set-upstream origin "release-${COLLECTOR_RELEASE}"
```

5. Set the patch number and release environment variables (if not set).
   See the section "Patch releases" for patch releases

```sh
export COLLECTOR_PATCH_NUMBER=0
export COLLECTOR_RELEASE=3.22
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
