#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
set -euo pipefail
dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
exec "$dir/truckaudiosim" "$@"
