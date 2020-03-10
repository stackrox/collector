#!/bin/bash
# Creates an incomplete proc entry for a fake process

fake_pid="8989"
fake_proc_name="short"
fake_proc_dir="/tmp/fake-proc"
fake_proc="${fake_proc_dir}/${fake_pid}"

rm -rf "${fake_proc_dir}"

mkdir -p "${fake_proc}"

ln -s "/bin/sh" "${fake_proc}/exe"

echo "Name: ${fake_proc_name}" > "${fake_proc}/status"
echo "/bin/${fake_proc_name}" > "${fake_proc}/cmdline"
env > "${fake_proc}/environ"
