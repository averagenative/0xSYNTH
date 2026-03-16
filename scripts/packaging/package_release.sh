#!/bin/bash
#
# package_release.sh — Build and package 0xSYNTH for release.
#
# Usage:
#   ./scripts/packaging/package_release.sh [version]
#
# Builds:
#   - Linux:   tar.gz (standalone + plugins + presets)
#   - Linux:   AppImage (standalone only)
#   - Windows: zip (standalone + plugins + presets)
#   - Windows: NSIS installer (.exe)
#
# Outputs to release/ directory.
#

set -e

VERSION="${1:-1.0.0}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$(dirname "$SCRIPT_DIR")")"

cd "$PROJECT_DIR"

echo "=== 0xSYNTH Release Packaging v${VERSION} ==="
echo ""

# ── Clean ──
rm -rf release
mkdir -p release

# ══════════════════════════════════════════════════════════════════════
# LINUX BUILD
# ══════════════════════════════════════════════════════════════════════
echo "--- Building Linux (native) ---"
cmake -B build -DCMAKE_BUILD_TYPE=Release -DENABLE_MP3=ON 2>&1 | tail -3
cmake --build build -j$(nproc) 2>&1 | tail -5

# Run tests
echo "--- Running tests ---"
cd build && ctest --output-on-failure 2>&1 | tail -3 && cd "$PROJECT_DIR"

# ── Linux tar.gz ──
echo ""
echo "--- Packaging Linux tar.gz ---"
LINUX_DIR="release/0xSYNTH-${VERSION}-linux-x64"
mkdir -p "${LINUX_DIR}/presets"

cp build/0xsynth              "${LINUX_DIR}/" 2>/dev/null || echo "  (no standalone binary)"
cp build/0xSYNTH.clap         "${LINUX_DIR}/" 2>/dev/null || echo "  (no CLAP)"
cp build/0xSYNTH.vst3         "${LINUX_DIR}/" 2>/dev/null || echo "  (no VST3)"
cp -r presets/factory          "${LINUX_DIR}/presets/"
cp README.md LICENSE FEATURES.md "${LINUX_DIR}/"

cat > "${LINUX_DIR}/INSTALL.txt" << 'EOF'
0xSYNTH — Linux Installation
==============================

Standalone:
  chmod +x 0xsynth
  ./0xsynth

Plugins:
  CLAP: cp 0xSYNTH.clap ~/.clap/
  VST3: mkdir -p ~/.vst3 && cp 0xSYNTH.vst3 ~/.vst3/

Presets should be alongside the binary in presets/factory/
or in ~/.local/share/0xSYNTH/presets/
EOF

cd release
tar czf "0xSYNTH-${VERSION}-linux-x64.tar.gz" "0xSYNTH-${VERSION}-linux-x64"
cd "$PROJECT_DIR"
echo "  -> release/0xSYNTH-${VERSION}-linux-x64.tar.gz"

# ── Linux AppImage ──
echo ""
echo "--- Packaging Linux AppImage ---"
APPDIR="release/0xSYNTH.AppDir"
mkdir -p "${APPDIR}/usr/bin"
mkdir -p "${APPDIR}/usr/bin/presets"
mkdir -p "${APPDIR}/usr/share/applications"
mkdir -p "${APPDIR}/usr/share/icons/hicolor/256x256/apps"

cp build/0xsynth "${APPDIR}/usr/bin/" 2>/dev/null || true
cp -r presets/factory "${APPDIR}/usr/bin/presets/"

# Icon
cp assets/icon_256.png "${APPDIR}/0xsynth.png"
cp assets/icon_256.png "${APPDIR}/usr/share/icons/hicolor/256x256/apps/0xsynth.png"
ln -sf 0xsynth.png "${APPDIR}/.DirIcon"

# Desktop entry
cat > "${APPDIR}/0xsynth.desktop" << EOF
[Desktop Entry]
Name=0xSYNTH
Exec=0xsynth
Icon=0xsynth
Type=Application
Categories=AudioVideo;Audio;
Comment=Multi-Engine Synthesizer
EOF
cp "${APPDIR}/0xsynth.desktop" "${APPDIR}/usr/share/applications/"

# AppRun
cat > "${APPDIR}/AppRun" << 'EOF'
#!/bin/bash
SELF="$(readlink -f "$0")"
HERE="${SELF%/*}"
export LD_LIBRARY_PATH="${HERE}/usr/lib:${LD_LIBRARY_PATH}"
exec "${HERE}/usr/bin/0xsynth" "$@"
EOF
chmod +x "${APPDIR}/AppRun"

# Build AppImage if tool available
if command -v appimagetool &>/dev/null; then
    ARCH=x86_64 appimagetool "${APPDIR}" "release/0xSYNTH-${VERSION}-x86_64.AppImage" 2>/dev/null
    echo "  -> release/0xSYNTH-${VERSION}-x86_64.AppImage"
else
    echo "  appimagetool not found — AppDir created at ${APPDIR}"
    echo "  Install: wget https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage"
fi

# ══════════════════════════════════════════════════════════════════════
# WINDOWS BUILD (cross-compile)
# ══════════════════════════════════════════════════════════════════════
echo ""
echo "--- Building Windows (MinGW cross-compile) ---"
cmake -B build-win64 -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-gui.cmake -DENABLE_MP3=ON 2>&1 | tail -3
cmake --build build-win64 --target oxs_standalone oxs_clap oxs_vst3 -j$(nproc) 2>&1 | tail -5

# ── Windows zip ──
echo ""
echo "--- Packaging Windows zip ---"
WIN_DIR="release/0xSYNTH-${VERSION}-windows-x64"
mkdir -p "${WIN_DIR}/presets"

cp build-win64/0xsynth.exe     "${WIN_DIR}/"
cp build-win64/0xSYNTH.clap   "${WIN_DIR}/"
cp build-win64/0xSYNTH.vst3   "${WIN_DIR}/"
cp -r presets/factory           "${WIN_DIR}/presets/"
cp README.md LICENSE FEATURES.md "${WIN_DIR}/"

cat > "${WIN_DIR}/INSTALL.txt" << 'EOF'
0xSYNTH — Windows Installation
================================

Standalone:
  Double-click 0xsynth.exe to run.
  Presets are in the presets/factory/ folder.

Plugins (manual install):
  CLAP: Copy 0xSYNTH.clap to C:\Program Files\Common Files\CLAP\
  VST3: Copy 0xSYNTH.vst3 to C:\Program Files\Common Files\VST3\0xSYNTH.vst3\Contents\x86_64-win\

Or use the installer (0xSYNTH-setup.exe) which handles everything automatically.

Recordings save to a "recordings" folder next to 0xsynth.exe.
EOF

cd release
zip -r "0xSYNTH-${VERSION}-windows-x64.zip" "0xSYNTH-${VERSION}-windows-x64" -q
cd "$PROJECT_DIR"
echo "  -> release/0xSYNTH-${VERSION}-windows-x64.zip"

# ── Windows NSIS installer ──
echo ""
echo "--- Building Windows installer (NSIS) ---"
if command -v makensis &>/dev/null; then
    # Update version in .nsi
    sed -i "s/!define VER \".*\"/!define VER \"${VERSION}\"/" scripts/packaging/0xsynth_installer.nsi
    sed -i "s/!define VERFULL \".*\"/!define VERFULL \"${VERSION}.0\"/" scripts/packaging/0xsynth_installer.nsi

    makensis scripts/packaging/0xsynth_installer.nsi 2>&1 | tail -5
    echo "  -> release/0xSYNTH-${VERSION}-windows-x64-setup.exe"
else
    echo "  makensis not found — skipping installer"
fi

# ══════════════════════════════════════════════════════════════════════
# SUMMARY
# ══════════════════════════════════════════════════════════════════════
echo ""
echo "=== Release artifacts ==="
ls -lh release/*.tar.gz release/*.zip release/*.exe release/*.AppImage 2>/dev/null
echo ""
echo "To create a GitHub release:"
echo "  git tag v${VERSION}"
echo "  git push origin v${VERSION}"
echo "  gh release create v${VERSION} release/0xSYNTH-${VERSION}-* --title 'v${VERSION}' --generate-notes"
