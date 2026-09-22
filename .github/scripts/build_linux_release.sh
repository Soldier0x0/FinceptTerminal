#!/usr/bin/env bash
# Build Linux release artifacts (AppImage + .deb) for Fincept Terminal.
# Called from release-fork-linux.yml (and can be reused elsewhere).
# Expects: cwd = fincept-qt/, build/ already configured + compiled, env:
#   QT_ROOT_DIR, GITHUB_WORKSPACE, APP_NAME (default FinceptTerminal)
set -euo pipefail

APP_NAME="${APP_NAME:-FinceptTerminal}"
REPO_SLUG="${GITHUB_REPOSITORY:-Soldier0x0/FinceptTerminal}"

VERSION=$(grep -Po 'project\(FinceptTerminal VERSION \K[0-9]+\.[0-9]+\.[0-9]+' CMakeLists.txt)
if [ -z "${VERSION}" ]; then
  echo "::error::Could not parse project VERSION from CMakeLists.txt"
  exit 1
fi
echo "Package version: ${VERSION}"

# ── AppImage ─────────────────────────────────────────────────────────────────
APPDIR="build/AppDir"
mkdir -p "${APPDIR}/usr/bin"
mkdir -p "${APPDIR}/usr/share/applications"
mkdir -p "${APPDIR}/usr/share/icons/hicolor/256x256/apps"

cp "build/${APP_NAME}" "${APPDIR}/usr/bin/"
chmod +x "${APPDIR}/usr/bin/${APP_NAME}"

cat > "${APPDIR}/usr/share/applications/fincept-terminal.desktop" <<'EOF'
[Desktop Entry]
Name=Fincept Terminal
Exec=FinceptTerminal
Icon=fincept-terminal
Type=Application
Categories=Finance;
EOF

ICON_DEST="${APPDIR}/usr/share/icons/hicolor/256x256/apps/fincept-terminal.png"
if [ -f "resources/icons/app_icon_256.png" ]; then
  cp "resources/icons/app_icon_256.png" "${ICON_DEST}"
elif [ -f "resources/fincept.png" ]; then
  cp "resources/fincept.png" "${ICON_DEST}"
elif [ -f "resources/fincept.ico" ]; then
  convert "ico:resources/fincept.ico[0]" -resize 256x256 -background none -flatten "png:${ICON_DEST}"
else
  convert -size 256x256 xc:'#1a1a1a' "png:${ICON_DEST}"
fi

if [ ! -s "${ICON_DEST}" ] || ! file "${ICON_DEST}" | grep -q 'PNG image data'; then
  echo "::error::Icon at ${ICON_DEST} is missing or not a valid PNG"
  exit 1
fi

cp -r resources "${APPDIR}/usr/bin/resources" 2>/dev/null || true
bash "${GITHUB_WORKSPACE}/.github/scripts/sync_scripts.sh" scripts "${APPDIR}/usr/bin/scripts"

mkdir -p "${APPDIR}/usr/bin/resources"
cp resources/requirements-numpy1.txt "${APPDIR}/usr/bin/resources/" 2>/dev/null || true
cp resources/requirements-numpy2.txt "${APPDIR}/usr/bin/resources/" 2>/dev/null || true

wget -q "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage"
wget -q "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage"
chmod +x linuxdeploy-x86_64.AppImage linuxdeploy-plugin-qt-x86_64.AppImage

export PATH="${QT_ROOT_DIR}/bin:${PATH}"
export LD_LIBRARY_PATH="${QT_ROOT_DIR}/lib:${LD_LIBRARY_PATH:-}"

if [ -d "${QT_ROOT_DIR}/plugins/sqldrivers" ]; then
  rm -f "${QT_ROOT_DIR}/plugins/sqldrivers/libqsqlmimer.so" \
        "${QT_ROOT_DIR}/plugins/sqldrivers/libqsqlodbc.so" \
        "${QT_ROOT_DIR}/plugins/sqldrivers/libqsqlpsql.so" \
        "${QT_ROOT_DIR}/plugins/sqldrivers/libqsqlmysql.so"
fi

export QMAKE="${QT_ROOT_DIR}/bin/qmake"
export OUTPUT="FinceptTerminal-${VERSION}-linux-x64-setup.run"
OWNER="${REPO_SLUG%%/*}"
REPO="${REPO_SLUG##*/}"
export UPDATE_INFORMATION="gh-releases-zsync|${OWNER}|${REPO}|latest|FinceptTerminal-*-linux-x64-setup.run.zsync"

resolve_soname() {
  local soname="$1" path
  path=$(ldconfig -p | awk -v s="${soname}" '$1 == s && /x86-64/ { print $NF; exit }')
  if [ -z "${path}" ] || [ "$(basename "${path}")" != "${soname}" ]; then
    path="/usr/lib/x86_64-linux-gnu/${soname}"
  fi
  if [ ! -e "${path}" ]; then
    echo "::error::${soname} not found on the runner" >&2
    exit 1
  fi
  printf '%s' "${path}"
}

OPENSSL_ARGS=()
for _so in libssl.so.3 libcrypto.so.3; do
  _path=$(resolve_soname "${_so}")
  echo "Bundling OpenSSL library: ${_path}"
  OPENSSL_ARGS+=(--library "${_path}")
done

./linuxdeploy-x86_64.AppImage \
  --appdir "${APPDIR}" \
  --plugin qt \
  --output appimage \
  "${OPENSSL_ARGS[@]}" \
  --desktop-file "${APPDIR}/usr/share/applications/fincept-terminal.desktop" \
  --icon-file "${ICON_DEST}"

SSL_BUNDLED=$(find "${APPDIR}" -name 'libssl.so.3' -print -quit)
CRYPTO_BUNDLED=$(find "${APPDIR}" -name 'libcrypto.so.3' -print -quit)
if [ -z "${SSL_BUNDLED}" ] || [ -z "${CRYPTO_BUNDLED}" ]; then
  echo "::error::AppImage must bundle libssl.so.3 AND libcrypto.so.3"
  exit 1
fi

TLS_PLUGIN=$(find "${APPDIR}" -name 'libqopensslbackend.so' -print -quit)
if [ -z "${TLS_PLUGIN}" ]; then
  echo "::error::Qt TLS backend plugin missing from AppDir"
  exit 1
fi

if [ ! -f "${OUTPUT}" ]; then
  PRODUCED=$(find . -maxdepth 1 -name "*.AppImage" | head -1)
  if [ -z "${PRODUCED}" ]; then
    echo "::error::AppImage build failed — no .AppImage produced"
    exit 1
  fi
  mv "${PRODUCED}" "${OUTPUT}"
fi
chmod +x "${OUTPUT}"

SIZE_KB=$(du -k "${OUTPUT}" | cut -f1)
if [ "${SIZE_KB}" -lt 51200 ]; then
  echo "::error::AppImage is only ${SIZE_KB}KB — Qt libs likely not bundled"
  exit 1
fi

mkdir -p build
mv "${OUTPUT}" "build/${OUTPUT}"
echo "AppImage: build/${OUTPUT} ($(du -sh "build/${OUTPUT}" | cut -f1))"

if command -v zsyncmake >/dev/null 2>&1; then
  zsyncmake "build/${OUTPUT}" -o "build/${OUTPUT}.zsync" || true
fi

# ── .deb (before AppImage smoke — packaging must not depend on teardown) ───
DEB_NAME="fincept-terminal_${VERSION}_amd64"
DEB_ROOT="build/${DEB_NAME}"

mkdir -p "${DEB_ROOT}/DEBIAN" "${DEB_ROOT}/usr"
cmake --install build --prefix "${DEB_ROOT}/usr"

cat > "${DEB_ROOT}/DEBIAN/control" <<EOF
Package: fincept-terminal
Version: ${VERSION}
Section: finance
Priority: optional
Architecture: amd64
Depends: libgl1, libxcb-cursor0, libxkbcommon0, libfontconfig1, libfreetype6, libdbus-1-3
Maintainer: Soldier0x0 <https://github.com/Soldier0x0>
Homepage: https://github.com/${REPO_SLUG}
Description: Fincept Terminal (local-only fork)
 Native C++20 financial terminal — local mode, Ollama default, no Fincept cloud login.
EOF

dpkg-deb --build --root-owner-group "${DEB_ROOT}"
mv "build/${DEB_NAME}.deb" "build/FinceptTerminal-${VERSION}-linux-x64.deb"
echo "Built: FinceptTerminal-${VERSION}-linux-x64.deb ($(du -sh "build/FinceptTerminal-${VERSION}-linux-x64.deb" | cut -f1))"

# ── Smoke-test AppImage (after .deb is staged) ───────────────────────────────
APPIMAGE="build/${OUTPUT}"
chmod +x "${APPIMAGE}"
export QT_QPA_PLATFORM=offscreen
export QTWEBENGINE_DISABLE_SANDBOX=1
bash "${GITHUB_WORKSPACE}/.github/scripts/ci_app_checks.sh" smoke "${APPIMAGE}" ci-smoke 600

if [ -n "${GITHUB_ENV:-}" ]; then
  echo "VERSION=${VERSION}" >> "${GITHUB_ENV}"
fi
