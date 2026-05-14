#!/usr/bin/env bash

set -euo pipefail

OPT_DIR="/opt"
REQUIRED_JAVA_MAJOR=21

[[ $EUID -eq 0 ]] || {
	echo "Please run with root permissions"
	exit 1
}

have_command() {
  command -v "$1" >/dev/null 2>&1
}

get_java_major() {
  local binary="$1"
  local version_line version_token major

  if ! have_command "${binary}"; then
    return 1
  fi

  version_line="$("${binary}" -version 2>&1 | head -n 1)"
  version_token="$(printf '%s\n' "${version_line}" | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' | head -n 1)"
  major="$(printf '%s\n' "${version_token}" | sed -E 's/^([0-9]+).*/\1/')"

  if [[ -z "${major}" ]]; then
    return 1
  fi

  printf '%s\n' "${major}"
}

can_use_java() {
  local java_major javac_major

  java_major="$(get_java_major java || true)"
  javac_major="$(get_java_major javac || true)"

  [[ "${java_major}" == "${REQUIRED_JAVA_MAJOR}" && "${javac_major}" == "${REQUIRED_JAVA_MAJOR}" ]]
}

ensure_jdk() {
  if can_use_java; then
    echo "Found JDK ${REQUIRED_JAVA_MAJOR}."
    return
  fi

  echo "Ghidra currently requires JDK ${REQUIRED_JAVA_MAJOR}."
  echo "Install JDK ${REQUIRED_JAVA_MAJOR} and ensure both java and javac are on PATH."
  echo "Then re-run this script."
  return 1
}

ensure_uefiextract() {
  if ! have_command uefiextract; then
    echo "Missing required tool: uefiextract"
    echo "Add uefiextract, ensure it is on PATH, then re-run this script."
    return 1
  fi

  echo "Found uefiextract."
}

clone_or_update_repo() {
  local repo_url="$1"
  local target_dir="$2"

  if [[ -d "${target_dir}/.git" ]]; then
    echo "$(basename "${target_dir}") already exists in ${target_dir}"
    echo "Updating repository..."
    git -C "${target_dir}" pull --ff-only
  else
    echo "Cloning $(basename "${target_dir}") into ${target_dir}..."
    git clone "${repo_url}" "${target_dir}"
  fi
}

ensure_jdk
ensure_uefiextract

if [[ ! -d "${OPT_DIR}" ]]; then
  echo "${OPT_DIR} does not exist."
  echo "Create it first."
  exit 1
fi

if [[ ! -w "${OPT_DIR}" ]]; then
  echo "${OPT_DIR} is not writable by the current user."
  echo "Run this script with permissions that can write to ${OPT_DIR}."
  exit 1
fi

clone_or_update_repo "https://github.com/NationalSecurityAgency/ghidra.git" \
  "${OPT_DIR}/ghidra"
clone_or_update_repo "https://github.com/al3xtjames/ghidra-firmware-utils.git" \
  "${OPT_DIR}/ghidra-firmware-utils"

echo "Done. dependencies are available in ${OPT_DIR}"
