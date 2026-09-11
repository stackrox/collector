# Release Process


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

* **if performing a new minor release**: keep the default settings. The
  workflow will calculate the next minor version.
* **if performing a patch release**: set the release version to the
  `<Major>.<Minor>` of the release you're patching, e.g. `3.21`. The
  workflow will calculate the next patch version.
* **if performing a new major release**: set the release version to the
  next major version, e.g `4.0`.

The recommended workflow is to first run in dry-mode and check the tags
and branches are correct in the `Summary` section of the triggered run,
then run it again without dry-mode to create the actual release.

With the tag pushed, two workflows are triggered automatically:
`main.yml`, which runs the full CI suite (build, unit, integration and
k8s integration tests) against the new tag, and `konflux.yml`, which
waits for the Konflux/Red Hat image to appear in Quay and then runs
the integration and k8s integration tests against it.

## Manual release

**Note**: This release process should only be used if the automated
process fails.
---

### Create the collector image release branch

These steps are only needed when creating a new major or minor version
of collector, skip to the next section if you are releasing a patch
version.

1. Navigate to the local stackrox/collector git repository directory
   on the master branch and ensure the local checked out version is up
   to date.

    ```sh
    git checkout master
    git pull
    ```

1. Set the release environment variable, which should be incremented
   from the previous released version.

    ```sh
    export COLLECTOR_RELEASE=3.22
    ```

1. Create an internal release tag to mark on the master branch where we
   forked for the release.

    ```sh
    git tag "${COLLECTOR_RELEASE}.x"
    git push origin "${COLLECTOR_RELEASE}.x"
    ```

1. Create the release branch with an empty commit.

    ```sh
    git checkout -b "release-${COLLECTOR_RELEASE}"
    git commit -m "Empty commit to diverge ${COLLECTOR_RELEASE} from master"
    git push --set-upstream origin "release-${COLLECTOR_RELEASE}"
    ```

### Tag the new version

1. Set the patch number and release environment variables (if not set).

    ```sh
    export COLLECTOR_PATCH_NUMBER=0
    export COLLECTOR_RELEASE=3.22
    ```

1. Navigate to your local stackrox/collector repo and run:

    ```sh
    git checkout release-"${COLLECTOR_RELEASE}"
    ```

1. **if performing a patch release**: pull the latest changes on the
   release branch before tagging. This is not needed for a new major
   or minor release, since the branch was just created in the previous
   section.

    ```sh
    git pull --ff-only
    ```

1. Tag and push the release.

    ```sh
    git tag -a -m \
        "Collector v${COLLECTOR_RELEASE}.${COLLECTOR_PATCH_NUMBER} release" \
        "${COLLECTOR_RELEASE}.${COLLECTOR_PATCH_NUMBER}"
    git push origin "${COLLECTOR_RELEASE}.${COLLECTOR_PATCH_NUMBER}"
    ```

1. Create and push a tag to the falcosecurity-libs repository

    ```sh
    git submodule update --init falcosecurity-libs
    cd falcosecurity-libs
    git tag "${COLLECTOR_RELEASE}.${COLLECTOR_PATCH_NUMBER}"
    git push origin "${COLLECTOR_RELEASE}.${COLLECTOR_PATCH_NUMBER}"
    ```

1. Create a pull request to update the `COLLECTOR_VERSION` file in the
   [stackrox/stackrox](https://github.com/stackrox/stackrox/) repo with the
   newly created release after CI images have been built.
