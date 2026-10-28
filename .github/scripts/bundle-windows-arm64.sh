#!/usr/bin/env bash
# Collect crow.exe and the MinGW/Qt DLLs it needs into a portable zip.
set -euo pipefail

root="$(pwd)"
bin="${root}/install/bin"
if [[ ! -f "${bin}/crow.exe" ]]; then
  echo "crow.exe was not installed under install/bin" >&2
  find "${root}/install" -name 'crow.exe' -print >&2 || true
  exit 1
fi

deployer=""
for candidate in windeployqt6 windeployqt; do
  if command -v "${candidate}" >/dev/null 2>&1; then
    deployer="${candidate}"
    break
  fi
done
if [[ -z "${deployer}" ]]; then
  echo "windeployqt was not found" >&2
  exit 1
fi

echo "Using ${deployer}"
(
  cd "${bin}"
  "${deployer}" --release --compiler-runtime --no-translations crow.exe
)

# ntldd -R can recurse forever on circular imports, so walk direct imports
# once per binary and only pull DLLs out of the MSYS2 clangarm64 prefix.
declare -A seen=()
queue=()
while IFS= read -r binary; do
  queue+=("${binary}")
done < <(find "${bin}" -type f \( -name '*.exe' -o -name '*.dll' \))

echo "Scanning ${#queue[@]} binaries for extra DLLs"
index=0
copied=0
while [[ "${index}" -lt "${#queue[@]}" ]]; do
  binary="${queue[${index}]}"
  index=$((index + 1))
  base="$(basename "${binary}")"
  if [[ -n "${seen[${base}]:-}" ]]; then
    continue
  fi
  seen["${base}"]=1

  while IFS= read -r line; do
    dll="$(printf '%s\n' "${line}" | sed -n 's/.*=>[[:space:]]*//p' | awk '{print $1}' | tr '\\' '/')"
    [[ -z "${dll}" || "${dll}" == "not" ]] && continue
    case "${dll}" in
      *[Ww]indows/[Ss]ystem32/*|*[Ww]indows/SYSTEM32/*) continue ;;
      *clangarm64/bin/*) ;;
      *) continue ;;
    esac
    [[ -f "${dll}" ]] || continue
    name="$(basename "${dll}")"
    if [[ -z "${seen[${name}]:-}" && ! -f "${bin}/${name}" ]]; then
      cp -n "${dll}" "${bin}/"
      queue+=("${bin}/${name}")
      copied=$((copied + 1))
    fi
  done < <(ntldd "${binary}" || true)
done
echo "Copied ${copied} non-Qt DLLs"

dumper="objdump"
if ! command -v objdump >/dev/null 2>&1; then
  dumper="llvm-objdump"
fi
"${dumper}" -f "${bin}/crow.exe"
"${dumper}" -f "${bin}/crow.exe" | grep -q 'aarch64'

mkdir -p "${root}/dist"
archive="${root}/dist/crow-translate-4.1.0-windows-arm64.zip"
rm -f "${archive}"
(
  cd "${bin}"
  tar -a -cf "${archive}" .
)
ls -lh "${archive}"
