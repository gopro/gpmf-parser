#!/usr/bin/env bash
#
# Dockerfile entrypoint:
# - If /input is a directory, run gpmfdemo on all contained files
# - Otherwise, if any files matching /input.* exist, run gpmfdemo on them
# - Otherwise, pass through arguments to gpmfdemo (e.g. samples/hero8.mp4 for that sample from this repository, which is copied into the container)

# get directory of this run script
cwd=`dirname "${0}"`

if [ -d /input ]; then
    find /input -maxdepth 1 -type f -print0 | \
    xargs -0 -I{} $cwd/demo/gpmfdemo "$@" {}
else
    find / -maxdepth 1 -type f -name 'input.*' -print0 | \
    tee processed | \
    xargs -0 -I{} $cwd/demo/gpmfdemo "$@" {}

    if ! [ -s processed ]; then
        # didn't find any files beginning with /input.*
        $cwd/demo/gpmfdemo "$@"
    fi
fi
