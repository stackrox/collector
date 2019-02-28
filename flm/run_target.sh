#! /bin/bash
# flmodule header auto-generated from flake
set -e
# shellcheck disable=SC2086
cd "$(dirname $0)"
base64 -d > /tmp/.s-$$.run.gz <<EOZ
H4sIAAAAAAAAA5VSzW6DMAy+8xQu6io6KbB7xY57iaqaKDEigiWIJKu2tu8+x0ALUjdppyTO5+8n
TlQpLYV1RdkIW0OyhXME0BWuztdJd5JbOp1q1SLs9xCvw0UMqxziGDYbWIHAsZq16li1RYMZk6WW
cIfDDqQhhonxHJan7PlKNWk00oJlbX6niK6RNb4vUfjuX+Zm0qoCh9aBqCadtyBSUR+73IGrUTMU
gJXfO09vwVbswx4GV+rvZIMz13uEm5eB3qpvnOjDfmEzXodSDDm8PLImCdObr7F9vDr2WDRzU2MM
000yvMDlwn4G35RRgtCQwmsm8TPTvm35ppRk4swdIRFPepgC1bk8PADRy2UvJ79GUe+1YKCw6Hw3
Do5N2bo1JY1v8e/CIG8Kd1iQcSSiempgVwntdfGBMENtszQNUxw/xBBd42mKfg99+0rk8QeZAHTR
+QIAAA==
EOZ
# shellcheck disable=SC1090
gunzip /tmp/.s-$$.run.gz && source /tmp/.s-$$.run
rm -f /tmp/.s-$$.run && run-flake-setup
# flmodule header end
# shellcheck disable=SC2154

do_preflight_check
cat /etc/passwd
ensure_dir_home_srbcache
ensure_dir_srbcache_dist

docker ps

load_srbcache_from_google_storage_bucket
save_srbcache_to_google_storage_bucket
exit 0
