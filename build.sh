#!/usr/bin/env bash
# ==============================================================================
# KJD Build & Setup Entry Point (Root Wrapper)
# ==============================================================================
set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "${REPO_DIR}/scripts/build.sh" "$@"
