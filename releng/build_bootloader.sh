#!/bin/bash

set -exo pipefail

SELFDIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)
cd "$SELFDIR/.."
make clean
make BOOT2="$SELFDIR/duet_relocated.elf" amalgamation
