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
- Create a new module version branch from the previous version
- Rebase the new module version branch to a newer tag on `upstream-main`
    - This is likely to require conflict resolution and testing
- Push the new module version branch to origin

Or, described in Git commands:

First you must identify the latest version branch in the repository:

```
$ git branch -a | grep 'module-version-'
# e.g.
module-version-2.9
module-version-2.10
```

With a latest version in hand, you can proceed with the update:

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
$ git switch module-version-2.9

# Perform the update, fixing any conflicts
$ git switch -c module-version-2.10
$ git rebase <tag> # NOT upstream-main, we should only update to stable tags
$ git push -u origin module-version-2.10
```

In order to review an updated Falco branch, any PRs should target the original
`upstream-main` branch and _not_ the previous module version branch.
