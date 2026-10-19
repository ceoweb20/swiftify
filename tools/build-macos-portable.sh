#!/usr/bin/env bash
# Native arm64 or Intel build; dependencies use the same installed Qt kit.
set -euo pipefail
QT="${QT_ROOT_DIR:?Set QT_ROOT_DIR to the macOS Qt kit}"
SOURCE="$(cd "$(dirname "$0")/.." && pwd)"
WORK="${SWIFTIFY_BUILD_ROOT:-$SOURCE/build-macos-portable}"
JOBS="${CARGO_BUILD_JOBS:-4}"
case "$(uname -m)" in
    arm64) TARGET=macos-aarch64 ;;
    x86_64) TARGET=macos-x86_64 ;;
    *) echo 'Unsupported macOS architecture' >&2; exit 1 ;;
esac
mkdir -p "$WORK/sources"
common=(-G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF "-DCMAKE_PREFIX_PATH=$QT;$WORK/deps" -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0)
dependency() {
    local name="$1" url="$2" tag="$3"
    shift 3
    if [[ ! -d "$WORK/sources/$name" ]]; then
        git clone --quiet --depth 1 --branch "$tag" "$url" "$WORK/sources/$name"
    fi
    cmake -S "$WORK/sources/$name" -B "$WORK/$name" "${common[@]}" "-DCMAKE_INSTALL_PREFIX=$WORK/deps" "$@"
    cmake --build "$WORK/$name" --parallel "$JOBS"
    cmake --install "$WORK/$name"
}
dependency qtkeychain https://github.com/frankosterfeld/qtkeychain.git 0.17.0 \
    -DBUILD_WITH_QT5=OFF -DBUILD_TEST_APPLICATION=OFF -DBUILD_TRANSLATIONS=OFF
dependency qcoro https://github.com/qcoro/qcoro.git v0.13.0 \
    -DUSE_QT_VERSION=6 -DQCORO_WITH_QTDBUS=OFF -DQCORO_WITH_QTWEBSOCKETS=OFF -DQCORO_WITH_QTQUICK=OFF \
    -DQCORO_WITH_QML=OFF -DQCORO_WITH_QTTEST=OFF -DQCORO_BUILD_EXAMPLES=OFF
# a signing identity that stays the same from build to build (a certificate of
# the project's own is enough) gives the app the same designated requirement each
# time, which is what the keychain checks before it asks again. an ad hoc
# signature is the build's own hash, so every build and every update is a stranger.
IDENTITY="${SWIFTIFY_CODESIGN_IDENTITY:--}"
cmake -S "$SOURCE" -B "$WORK/app" "${common[@]}" \
    ${SWIFTIFY_UPDATE_PUBLIC_KEY:+"-DSWIFTIFY_UPDATE_PUBLIC_KEY=$SWIFTIFY_UPDATE_PUBLIC_KEY"} -DSWIFTIFY_COMPILE_APP_ICON=OFF \
    "-DSWIFTIFY_CODESIGN_IDENTITY=$IDENTITY" \
    ${SWIFTIFY_UPDATE_REPOSITORY:+"-DSWIFTIFY_UPDATE_REPOSITORY=$SWIFTIFY_UPDATE_REPOSITORY"}
cmake --build "$WORK/app" --parallel "$JOBS"
APP="$WORK/app/swiftify.app"
"$QT/bin/macdeployqt" "$APP" -qmldir="$SOURCE/qml" -always-overwrite -appstore-compliant
python3 "$SOURCE/tools/package-portable.py" --root "$APP" --target "$TARGET" --mark-only
# No Developer ID certificate or notarization: Gatekeeper is left as it is, and
# quarantine is not stripped from anything.
codesign --force --deep --sign "$IDENTITY" "$APP"
codesign --verify --deep --strict "$APP"
requirement="$(codesign -d -r- "$APP" 2>&1 | grep designated || true)"
echo "$requirement"
if [[ "$IDENTITY" != "-" && "$requirement" != *"certificate root"* ]]; then
    echo "the app was not signed with the stable identity" >&2
    exit 1
fi
python3 "$SOURCE/tools/package-portable.py" --root "$APP" --target "$TARGET" --out "$SOURCE/dist"
