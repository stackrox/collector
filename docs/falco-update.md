## Updating The Falco Fork

### Useful Links:

- Fork: https://github.com/stackrox/falcosecurity-libs
- Upstream: https://github.com/falcosecurity/libs

### Update Process

The process to update the Falco fork is a branching strategy that essentially
rebases our changes onto the latest Falco tag (not to the tip of master) in
order to retain versions of released code and avoid maintenance hell.

The process, at a high level is:

- Update the `upstream-main` branch to the tip of master
- Pick the new upstream tag we will be based on
- Create a new branch from the previous version
- Rebase the new branch to the newer tag on `upstream-main`
    - This is likely to require conflict resolution and testing
- Push the new branch to origin

Or, described in Git commands:

First you must identify the latest version branch in the repository:

```sh
$ git branch -a | grep '.*-stackrox$'
# e.g.
0.17.1-stackrox
0.17.2-stackrox
```

... As well as the upstream tag that will be used for the new version:

```sh
$ git tag -l
# e.g.
0.17.0
0.17.0-rc1
0.17.0-rc2
0.17.1
0.17.2
0.17.3
0.17.3-rc1
0.17.3-rc2
```

With the versions in hand, you can proceed with the update:
```sh
$ git remote add falco git@github.com:falcosecurity/libs
$ git fetch falco
$ git switch upstream-main

# Make sure to use --ff-only (fast forward) rather than introducing
# a merge commit
$ git merge --ff-only falco master
$ git push origin upstream-main
$ git push --tags origin upstream-main

# Now update the most recent branch
$ git switch 0.17.2-stackrox

# Perform the update, fixing any conflicts
$ git switch -c 0.17.3-stackrox
$ git rebase 0.17.3
$ git push -u origin 0.17.3-stackrox
```

In order to review an updated Falco branch, any PRs should target the original
`upstream-main` branch and _not_ the previous module version branch.
