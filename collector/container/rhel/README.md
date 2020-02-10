# RedHat Based collector image

The RedHat based collector image is currently used for the RedHat marketplace as well as for DoD customers.

This image is built in an opinionated way based on the DoD Centralized Artifacts Repository (DCAR) requirements outlined [here](https://dccscr.dsop.io/dsop/dccscr/tree/master/contributor-onboarding).

## Adding new files to the rhel based images

To add a new file to the rhel image, include it in `create-bundle.sh` script, do not add it to the Dockerfile in this directory.
