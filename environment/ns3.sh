#!/usr/bin/env bash
# Invoke the pinned ns-3 checkout with the isolated build tools.
set -euo pipefail
ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
export PATH="$ROOT/.deps/tools/bin:$PATH"
test -x "$ROOT/.deps/tools/bin/cmake" || { echo "Run bootstrap first" >&2; exit 1; }
cd "$ROOT/.deps/ns-3"
exec ./ns3 "$@"
