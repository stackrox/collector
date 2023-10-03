<<<<<<< HEAD
#!/usr/bin/env bash

set -ex

cd third_party/uthash
cp LICENSE "${LICENSE_DIR}/uthash-${UTHASH_VERSION}"

# uthash is a header only library, we just copy the one file we need
# into /usr/local/include and call it a day
cp src/uthash.h /usr/local/include
||||||| parent of 1df1465a2 (Cleanups + use falco 0.13.1)
=======
#!/usr/bin/env bash

set -e

cd third_party/uthash
cp LICENSE "${LICENSE_DIR}/uthash-${UTHASH_VERSION}"

# uthash is a header only library, we just copy the one file we need
# into /usr/include and call it a day
cp src/uthash.h /usr/include
>>>>>>> 1df1465a2 (Cleanups + use falco 0.13.1)
