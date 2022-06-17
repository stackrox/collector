## Release Utility

`release.py` can be used to automate the creation of the various release branches
or tags for a collector release.

### Usage

Provide the major (M) and minor (m) version to the release tool:

```sh
./release.py M.m
```

It follows the release process outlined in the [release process](../../docs/release.md)

By default, nothing is pushed to the remote repository, but this can be overridden by adding `--push`
