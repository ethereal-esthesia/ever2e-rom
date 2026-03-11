#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
JVM_DIR="${SCRIPT_DIR}/../ever2e-jvm"
EMU_FILE="../ever2e-rom/profiles/Apple2eEver2eBootLoopNoSlots.emu"

if [[ ! -x "${JVM_DIR}/gradlew" ]]; then
  echo "error: ${JVM_DIR}/gradlew not found or not executable" >&2
  exit 1
fi

# Default behavior: run the Ever2e boot-loop profile in headless mode.
# Any args you pass are appended after the profile path.
EMU_ARGS="${EMU_FILE}"
if [[ $# -gt 0 ]]; then
  EMU_ARGS+=" $*"
fi

cd "${JVM_DIR}"
./gradlew run --args="${EMU_ARGS}"
