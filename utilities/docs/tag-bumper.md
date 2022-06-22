## Tag bumper!
The script held in this directory is a small utility for bumping tag versions with an empty commit.
This is mostly done when we need to rebuild the collector image to get rid of some pesky vulnerabilities.

### Usage
Simply determine the major (M) and minor (m) version of collector needing a bump and run:
```sh
./tag-bumper M.m
```

The script will:
- Fetch all tags and branches.
- Determine the latest patch associated to the `M.m` version.
- Checkout the corresponding `release/M.m.x` branch and create an empty commit on it.
- Create a new tag on that empty commit.
- If you trust the script, using the `-p` flag will also push the new commit and tag to the remote.
