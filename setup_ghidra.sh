#!/usr/bin/env bash

set -euo pipefail

OPT_DIR="/opt"
REQUIRED_JAVA_MAJOR=21
GHIDRA_VERSION=12.0
GHIDRA_DATE=20251205
GHIDRA_FIRM_VERSION=12.0
GHIDRA_FIRM_DATE=20260114
GHIDRA_FIRM_DATE_LONG=2026.01.14

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

ensure_jdk
ensure_uefiextract

GHIDRA_DIR="${OPT_DIR}/ghidra"
if [[ -d ${GHIDRA_DIR} ]]; then
	rm -rf "${GHIDRA_DIR}"/*
else
	mkdir -p "${GHIDRA_DIR}" || exit 1
fi

GHIDRA_TEMP_ARCHIVE="$(mktemp --suffix=".zip")" || exit 1
GHIDRA_TEMP_EXTRACT="$(mktemp -d)" || exit 1
GHIDRA_FIRM_TEMP_ARCHIVE="$(mktemp --suffix=".zip")" || exit 1
GHIDRA_FIRM_TEMP_EXTRACT="$(mktemp -d)" || exit 1

trap 'cleanup_ghidra' EXIT SIGINT SIGTERM

cleanup_ghidra() {
	rm -f "${GHIDRA_TEMP_ARCHIVE}"
	rm -rf "${GHIDRA_TEMP_EXTRACT}"
	rm -f "${GHIDRA_FIRM_TEMP_ARCHIVE}"
	rm -rf "${GHIDRA_FIRM_TEMP_EXTRACT}"
}


echo "Downloading ghidra"
curl -Lo "${GHIDRA_TEMP_ARCHIVE}" "https://github.com/NationalSecurityAgency/ghidra/releases/download/Ghidra_${GHIDRA_VERSION}_build/ghidra_${GHIDRA_VERSION}_PUBLIC_${GHIDRA_DATE}.zip" || exit 1

echo "Extracting ghidra"
unzip -q "${GHIDRA_TEMP_ARCHIVE}" -d "${GHIDRA_TEMP_EXTRACT}" || exit 1
cp -r "${GHIDRA_TEMP_EXTRACT}/ghidra_${GHIDRA_VERSION}_PUBLIC/"* "${GHIDRA_DIR}" || exit 1

echo "Downloading ghidra-firmware-utils"
curl -Lo "${GHIDRA_FIRM_TEMP_ARCHIVE}" "https://github.com/al3xtjames/ghidra-firmware-utils/releases/download/${GHIDRA_FIRM_DATE_LONG}/ghidra_${GHIDRA_FIRM_VERSION}_PUBLIC_${GHIDRA_FIRM_DATE}_ghidra-firmware-utils.zip"

echo "Extracting ghidra-firmware-utils"
unzip -q "${GHIDRA_FIRM_TEMP_ARCHIVE}" -d "${GHIDRA_FIRM_TEMP_EXTRACT}" || exit 1
cp -r "${GHIDRA_FIRM_TEMP_EXTRACT}/"* "${GHIDRA_DIR}/Ghidra/Extensions/" || exit 1

echo ""
echo "Done. Ghidra is installed to ${GHIDRA_DIR}"
