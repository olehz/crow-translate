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

copy_non_system_dlls() {
  local binary="$1"
  local dest="$2"
  ntldd -R "${binary}" | while IFS= read -r line; do
    local dll
    dll="$(printf '%s\n' "${line}" | sed -n 's/.*=>[[:space:]]*//p' | awk '{print $1}' | tr '\\' '/')"
    [[ -z "${dll}" || "${dll}" == "not" ]] && continue
    case "${dll}" in
      *[Ww]indows/[Ss]ystem32/*|*[Ww]indows/SYSTEM32/*) continue ;;
    esac
    case "${dll}" in
      *clangarm64/bin/*)
        if [[ -f "${dll}" ]]; then
          cp -n "${dll}" "${dest}/" || true
        fi
        ;;
    esac
  done
}

for _pass in 1 2 3 4; do
  while IFS= read -r binary; do
    copy_non_system_dlls "${binary}" "${bin}"
  done < <(find "${bin}" -type f \( -name '*.exe' -o -name '*.dll' \))
done

objdump -f "${bin}/crow.exe"
objdump -f "${bin}/crow.exe" | grep -q 'aarch64'

mkdir -p "${root}/dist"
archive="${root}/dist/crow-translate-4.1.0-windows-arm64.zip"
rm -f "${archive}"
(
  cd "${bin}"
  # bsdtar writes zip archives and is available in the MSYS2 environment.
  tar -a -cf "${archive}" .
)
ls -lh "${archive}"
