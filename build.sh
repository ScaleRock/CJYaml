#!/usr/bin/env bash
# build.sh - improved
set -euo pipefail
IFS=$'\n\t'

CMAKE_BUILD_DIR="build"
OUT_DIR="out"
RESOURCES_DIR="src/main/resources"
LIB_REL_PATH="linux/libcjyaml.so"
CMAKE_OPTIONS=("-DENABLE_JNI=ON")
BUILD_TYPE="Release"
NUM_JOBS=""

info(){ printf '\033[1;34m[INFO]\033[0m %s\n' "$*"; }
error(){ printf '\033[1;31m[ERROR]\033[0m %s\n' "$*" >&2; }
success(){ printf '\033[1;32m[SUCCESS]\033[0m %s\n' "$*"; }

usage(){
  cat <<EOF
Usage: $0 [--clean] [--jobs N]
  --clean     Remove build/ and out/ before building.
  --jobs N    Number of parallel jobs.
EOF
  exit 1
}

CLEAN=false
while [[ $# -gt 0 ]]; do
  case "$1" in
    --clean) CLEAN=true; shift ;;
    --jobs)
      shift
      if [[ $# -eq 0 || ! "$1" =~ ^[0-9]+$ ]]; then
        error "--jobs requires a numeric argument"
        usage
      fi
      NUM_JOBS="$1"; shift
      ;;
    -h|--help) usage ;;
    *) error "Unknown option: $1"; usage ;;
  esac
done

if [[ -z "$NUM_JOBS" ]]; then
  if command -v nproc >/dev/null 2>&1; then
    NUM_JOBS="$(nproc)"
  elif [[ "$(uname)" == "Darwin" ]] && command -v sysctl >/dev/null 2>&1; then
    NUM_JOBS="$(sysctl -n hw.ncpu)"
  else
    NUM_JOBS=1
  fi
fi

for cmd in cmake mvn; do
  if ! command -v "$cmd" >/dev/null 2>&1; then
    error "Required command '$cmd' not found."
    exit 2
  fi
done

info "NUM_JOBS=$NUM_JOBS"
info "CLEAN=$CLEAN"

if [[ "$CLEAN" == true ]]; then
  info "Cleaning ${CMAKE_BUILD_DIR}/ and ${OUT_DIR}/ ..."
  rm -rf "${CMAKE_BUILD_DIR}" "${OUT_DIR}"
fi

mkdir -p "${CMAKE_BUILD_DIR}"
mkdir -p "${RESOURCES_DIR}"

info "Configuring CMake..."
cmake -S . -B "${CMAKE_BUILD_DIR}" -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" "${CMAKE_OPTIONS[@]}"

info "Building (parallel=${NUM_JOBS})..."
# prefer --parallel if CMake supports it
cmake --build "${CMAKE_BUILD_DIR}" --config "${BUILD_TYPE}" --parallel "${NUM_JOBS}"

# expected artifact
LIB_SOURCE_PATH="${OUT_DIR}/${LIB_REL_PATH}"
if [[ ! -f "${LIB_SOURCE_PATH}" ]]; then
  info "Artifact not found at ${LIB_SOURCE_PATH}, searching for probable outputs..."
  FOUND=$(find "${OUT_DIR}" "${CMAKE_BUILD_DIR}" -type f -name "libcjyaml.*" -o -name "cjyaml.*" 2>/dev/null | head -n1 || true)
  if [[ -n "$FOUND" ]]; then
    info "Found artifact: $FOUND"
    LIB_SOURCE_PATH="$FOUND"
  else
    error "Build artifact not found. Looked in out/ and build/."
    exit 3
  fi
fi

info "Copying ${LIB_SOURCE_PATH} -> ${RESOURCES_DIR}/"
cp -f "${LIB_SOURCE_PATH}" "${RESOURCES_DIR}/"
success "Library copied."

info "Running maven package..."
# -B for batch mode; avoid -q so errors are visible
mvn -B clean package
success "Maven package finished."

success "Done. Library at ${RESOURCES_DIR}/$(basename "${LIB_SOURCE_PATH}")"
