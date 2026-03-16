# 0xSYNTH macOS Release Instructions

Run these on a Mac with Homebrew, SDL2, and CMake installed.

## Prerequisites

```bash
brew install sdl2 cmake
```

## App Icon

The macOS `.icns` icon is pre-built at `assets/0xsynth.icns` and checked into the repo.

To regenerate it from the source PNGs:

```bash
mkdir -p assets/0xsynth.iconset
sips -z 16 16   assets/icon_16.png  --out assets/0xsynth.iconset/icon_16x16.png
sips -z 32 32   assets/icon_32.png  --out assets/0xsynth.iconset/icon_16x16@2x.png
sips -z 32 32   assets/icon_32.png  --out assets/0xsynth.iconset/icon_32x32.png
sips -z 64 64   assets/icon_64.png  --out assets/0xsynth.iconset/icon_32x32@2x.png
sips -z 128 128 assets/icon_128.png --out assets/0xsynth.iconset/icon_128x128.png
sips -z 256 256 assets/icon_256.png --out assets/0xsynth.iconset/icon_128x128@2x.png
sips -z 256 256 assets/icon_256.png --out assets/0xsynth.iconset/icon_256x256.png
sips -z 256 256 assets/icon_256.png --out assets/0xsynth.iconset/icon_256x256@2x.png
sips -z 256 256 assets/icon_256.png --out assets/0xsynth.iconset/icon_512x512.png
sips -z 256 256 assets/icon_256.png --out assets/0xsynth.iconset/icon_512x512@2x.png
iconutil -c icns assets/0xsynth.iconset -o assets/0xsynth.icns
rm -rf assets/0xsynth.iconset
```

## Build

```bash
cd ~/projects/0xSYNTH
cmake -B build -DCMAKE_BUILD_TYPE=Release -DENABLE_MP3=ON
cmake --build build -j$(sysctl -n hw.ncpu)
```

## Package 1: Standalone .app + DMG

### Create .app bundle

```bash
VERSION="1.0.0"
APP_DIR="release/0xSYNTH.app"

mkdir -p "${APP_DIR}/Contents/MacOS"
mkdir -p "${APP_DIR}/Contents/Resources"

# Binary
cp build/0xsynth "${APP_DIR}/Contents/MacOS/0xSYNTH"

# Presets
cp -r presets/factory "${APP_DIR}/Contents/Resources/"

# Icon
cp assets/0xsynth.icns "${APP_DIR}/Contents/Resources/0xsynth.icns"

# Info.plist
cat > "${APP_DIR}/Contents/Info.plist" << PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleExecutable</key>
    <string>0xSYNTH</string>
    <key>CFBundleIdentifier</key>
    <string>com.dcmichael.0xsynth</string>
    <key>CFBundleName</key>
    <string>0xSYNTH</string>
    <key>CFBundleIconFile</key>
    <string>0xsynth</string>
    <key>CFBundleVersion</key>
    <string>${VERSION}</string>
    <key>CFBundleShortVersionString</key>
    <string>${VERSION}</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>NSHighResolutionCapable</key>
    <true/>
    <key>LSMinimumSystemVersion</key>
    <string>11.0</string>
</dict>
</plist>
PLIST
```

### Create DMG

```bash
DMG_STAGE="release/dmg_stage"
mkdir -p "${DMG_STAGE}"
cp -r "${APP_DIR}" "${DMG_STAGE}/"
ln -s /Applications "${DMG_STAGE}/Applications"
cp README.md LICENSE "${DMG_STAGE}/"

hdiutil create -volname "0xSYNTH v${VERSION}" \
    -srcfolder "${DMG_STAGE}" \
    -ov -format UDZO \
    "release/0xSYNTH-${VERSION}-macos-x64.dmg"

rm -rf "${DMG_STAGE}"
```

## Package 2: Zip with standalone + plugins

### Prepare plugins

```bash
PLUGIN_DIR="release/Plugins"
mkdir -p "${PLUGIN_DIR}"

# CLAP (single file)
cp build/0xSYNTH.clap "${PLUGIN_DIR}/"

# VST3 (macOS bundle structure)
VST3_BUNDLE="${PLUGIN_DIR}/0xSYNTH.vst3"
mkdir -p "${VST3_BUNDLE}/Contents/MacOS"
cp build/0xSYNTH.vst3 "${VST3_BUNDLE}/Contents/MacOS/0xSYNTH"

cat > "${VST3_BUNDLE}/Contents/Info.plist" << VST3PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleExecutable</key>
    <string>0xSYNTH</string>
    <key>CFBundleIdentifier</key>
    <string>com.dcmichael.0xsynth.vst3</string>
    <key>CFBundleName</key>
    <string>0xSYNTH</string>
    <key>CFBundleVersion</key>
    <string>${VERSION}</string>
    <key>CFBundlePackageType</key>
    <string>BNDL</string>
</dict>
</plist>
VST3PLIST
```

### Create zip

```bash
cd release
zip -r "0xSYNTH-${VERSION}-macos-x64.zip" \
    0xSYNTH.app Plugins/ -q
cd ..
```

### Gatekeeper instructions (include in zip as INSTALL.txt)

```
0xSYNTH — macOS Installation
==============================

STANDALONE:
  Drag 0xSYNTH.app to /Applications.

PLUGINS:
  CLAP: cp Plugins/0xSYNTH.clap ~/Library/Audio/Plug-Ins/CLAP/
  VST3: cp -r Plugins/0xSYNTH.vst3 ~/Library/Audio/Plug-Ins/VST3/

GATEKEEPER (unsigned app):
  If macOS blocks the app with "cannot be opened because the developer
  cannot be verified", run in Terminal:

  xattr -cr /Applications/0xSYNTH.app
  xattr -cr ~/Library/Audio/Plug-Ins/CLAP/0xSYNTH.clap
  xattr -cr ~/Library/Audio/Plug-Ins/VST3/0xSYNTH.vst3

  Or: System Settings > Privacy & Security > "Allow Anyway"

PRESETS:
  Factory presets are bundled inside the .app.
  User presets save to ~/Library/Application Support/0xSYNTH/presets/
```

## Expected outputs

```
release/0xSYNTH-1.0.0-macos-x64.dmg     # DMG with app + Applications symlink
release/0xSYNTH-1.0.0-macos-x64.zip     # Zip with app + plugins
```
