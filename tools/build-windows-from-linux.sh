#!/usr/bin/env bash
# cross-builds a self-contained windows x86_64 folder and zip of swiftify from
# linux, downloading the matching windows mingw qt kit from download.qt.io on
# the first run so nothing has to be installed or built on windows
# this is mainly intended for testing on windows without needing to deal with this qt account crap
#
#
# usage
#   ./tools/build-windows-from-linux.sh
#   ./tools/build-windows-from-linux.sh --install-deps
#   ./tools/build-windows-from-linux.sh --qt /path/to/Qt/6.12.0/mingw_64 --out dist
#
# needs mingw-w64-gcc, host qt6 (qt6-base, qt6-declarative, qt6-shadertools,
# qt6-websockets), cmake, ninja, git, curl, python3, bsdtar, zip and rust

set -euo pipefail

QT=""
HOST_QT="/usr"
OUT="dist"
JOBS="$(nproc 2>/dev/null || echo 4)"
INSTALL_DEPS=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --qt) QT="$2"; shift 2 ;;
        --host-qt) HOST_QT="$2"; shift 2 ;;
        --out) OUT="$2"; shift 2 ;;
        --jobs) JOBS="$2"; shift 2 ;;
        --install-deps) INSTALL_DEPS=true; shift ;;
        -h|--help)
            sed -n '2,12p' "$0" | sed 's/^# \{0,1\}//'
            exit 0
            ;;
        *) echo "unknown option $1" >&2; exit 1 ;;
    esac
done

fail() { echo "error: $1" >&2; exit 1; }

if $INSTALL_DEPS; then
    if command -v pacman >/dev/null 2>&1; then
        sudo pacman -S --needed mingw-w64-gcc qt6-base qt6-declarative qt6-shadertools qt6-websockets \
            cmake ninja git curl libarchive
    elif command -v apt-get >/dev/null 2>&1; then
        sudo apt-get update
        sudo apt-get install -y g++-mingw-w64-x86-64-posix qt6-base-dev qt6-base-dev-tools \
            qt6-declarative-dev qt6-declarative-dev-tools qt6-shader-baker qt6-shadertools-dev \
            qt6-websockets-dev cmake ninja-build git curl libarchive-tools
    else
        fail "--install-deps supports pacman and apt-get; install mingw-w64-gcc and host qt6 manually"
    fi
fi

for tool in x86_64-w64-mingw32-g++ x86_64-w64-mingw32-gcc x86_64-w64-mingw32-windres x86_64-w64-mingw32-objdump \
    cmake ninja git curl bsdtar; do
    command -v "$tool" >/dev/null 2>&1 || fail "$tool is missing (run with --install-deps or install mingw-w64-gcc)"
done

SOURCE="$(cd "$(dirname "$0")/.." && pwd)"
VERSION="$(sed -n '/^project(Swiftify/,/VERSION/s/^ *VERSION \([0-9.]*\).*/\1/p' "$SOURCE/CMakeLists.txt")"
[[ -n "$VERSION" ]] || fail "could not read the version from CMakeLists.txt"
NAME="swiftify-$VERSION-windows-x86_64"
mkdir -p "$OUT"
OUT="$(cd "$OUT" && pwd)"
WORK="$SOURCE/build-windows-cross"
DEPS="$WORK/deps"
STAGE="$OUT/$NAME"
mkdir -p "$WORK/sources"

# host qt version, which the windows qt kit must match for moc/qmlcachegen/qsb
HOST_QT_VERSION="$(pkg-config --modversion Qt6Core 2>/dev/null || true)"
if [[ -z "$HOST_QT_VERSION" && -x "$HOST_QT/bin/qmake6" ]]; then
    HOST_QT_VERSION="$("$HOST_QT/bin/qmake6" -query QT_VERSION)"
fi
[[ -n "$HOST_QT_VERSION" ]] || fail "could not detect host Qt6 version; install qt6-base"

# rust target x86_64-pc-windows-gnu: use system rustup if present, or a local
# rustup inside build-windows-cross when the system uses a plain distro rust package
RUST_TARGET="x86_64-pc-windows-gnu"
RUST_CMAKE_ARGS=("-DRust_CARGO_TARGET=$RUST_TARGET")
has_rust_target() {
    local rustc_bin="$1" sysroot
    sysroot="$("$rustc_bin" --print target-libdir --target "$RUST_TARGET" 2>/dev/null || true)"
    [[ -n "$sysroot" && -d "$sysroot" ]] && compgen -G "$sysroot/libstd*" >/dev/null
}
if ! has_rust_target rustc; then
    if command -v rustup >/dev/null 2>&1; then
        rustup target add "$RUST_TARGET"
    else
        export RUSTUP_HOME="$WORK/rustup"
        export CARGO_HOME="$WORK/cargo"
        export PATH="$CARGO_HOME/bin:$PATH"
        if [[ ! -x "$CARGO_HOME/bin/rustup" ]]; then
            echo "installing a local rust toolchain with $RUST_TARGET in $WORK/rustup..."
            curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y \
                --no-modify-path --profile minimal --default-toolchain stable --target "$RUST_TARGET"
        else
            "$CARGO_HOME/bin/rustup" target add "$RUST_TARGET"
        fi
        RUST_CMAKE_ARGS+=(
            "-DRust_COMPILER=$CARGO_HOME/bin/rustc"
            "-DRust_CARGO=$CARGO_HOME/bin/cargo"
        )
    fi
fi

# download the matching windows mingw_64 qt kit from download.qt.io if --qt was not given
if [[ -z "$QT" ]]; then
    QT_NODOT="${HOST_QT_VERSION//./}"
    QT="$WORK/qt"
    if [[ ! -f "$QT/lib/cmake/Qt6Core/Qt6CoreConfig.cmake" ]]; then
        echo "downloading Windows Qt $HOST_QT_VERSION (mingw_64) into $QT..."
        mkdir -p "$WORK/qt/archives" "$QT"
        BASE_URL=""
        UPDATES_XML=""
        for candidate in \
            "https://download.qt.io/online/qtsdkrepository/windows_x86/desktop/qt6_${QT_NODOT}/qt6_${QT_NODOT}_mingw" \
            "https://download.qt.io/online/qtsdkrepository/windows_x86/desktop/qt6_${QT_NODOT}"; do
            if UPDATES_XML="$(curl -fsSL "$candidate/Updates.xml" 2>/dev/null)" && [[ -n "$UPDATES_XML" ]]; then
                BASE_URL="$candidate"
                break
            fi
        done
        [[ -n "$BASE_URL" ]] || fail "could not find Qt $HOST_QT_VERSION Windows MinGW repository on download.qt.io"

        while read -r pkg ver archive; do
            case "$pkg:$archive" in
                "qt.qt6.${QT_NODOT}.win64_mingw":qtbase-* | \
                "qt.qt6.${QT_NODOT}.win64_mingw":qtsvg-* | \
                "qt.qt6.${QT_NODOT}.win64_mingw":qtdeclarative-* | \
                "qt.qt6.${QT_NODOT}.win64_mingw":d3dcompiler_47-* | \
                "qt.qt6.${QT_NODOT}.win64_mingw":MinGW-w64-* | \
                "qt.qt6.${QT_NODOT}.addons.qtshadertools.win64_mingw":qtshadertools-* | \
                "qt.qt6.${QT_NODOT}.addons.qtwebsockets.win64_mingw":qtwebsockets-* | \
                "qt.qt6.${QT_NODOT}.addons.qtimageformats.win64_mingw":qtimageformats-*)
                    dest="$WORK/qt/archives/${ver}${archive}"
                    if [[ ! -f "$dest" ]]; then
                        echo "  fetching $archive..."
                        curl -fL --retry 3 -o "$dest.part" "$BASE_URL/$pkg/${ver}${archive}"
                        mv "$dest.part" "$dest"
                    fi
                    bsdtar -xf "$dest" -C "$QT"
                    ;;
            esac
        done < <(awk -F'[<>]' '
            /<Name>/ { name = $3 }
            /<Version>/ { ver = $3 }
            /<DownloadableArchives>/ {
                n = split($3, arr, ",")
                for (i = 1; i <= n; i++) {
                    gsub(/^[ \t]+|[ \t]+$/, "", arr[i])
                    if (arr[i] != "") print name, ver, arr[i]
                }
            }
        ' <<< "$UPDATES_XML")
    fi
fi
QT="$(cd "$QT" && pwd)"
[[ -f "$QT/lib/cmake/Qt6Core/Qt6CoreConfig.cmake" ]] || fail "no Qt6CoreConfig.cmake in $QT"

# 1. build the native host shape_atlas tool first
SHAPE_ATLAS="$WORK/host-tools/shape_atlas"
if [[ ! -x "$SHAPE_ATLAS" ]]; then
    echo "building host shape_atlas tool..."
    mkdir -p "$WORK/host-tools"
    g++ -std=c++20 -O2 -fPIC "$SOURCE/tools/shape_atlas/main.cpp" \
        -I"$SOURCE/tools/shape_atlas" \
        $(pkg-config --cflags --libs Qt6Core Qt6Gui Qt6Qml) \
        -o "$SHAPE_ATLAS"
fi

# 2. write a cmake toolchain file for x86_64-w64-mingw32
TOOLCHAIN="$WORK/mingw-toolchain.cmake"
cat > "$TOOLCHAIN" <<EOF
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)
set(QT_HOST_PATH "$HOST_QT")
set(CMAKE_FIND_ROOT_PATH "$QT" "$DEPS" "/usr/x86_64-w64-mingw32")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
EOF

common=(
    -G Ninja
    -DCMAKE_BUILD_TYPE=Release
    -DBUILD_TESTING=OFF
    "-DCMAKE_TOOLCHAIN_FILE=$TOOLCHAIN"
    "-DCMAKE_PREFIX_PATH=$QT;$DEPS"
    "-DQT_HOST_PATH=$HOST_QT"
)

# 3. cross-build qtkeychain and qcoro
build_dependency() {
    local name="$1" url="$2" tag="$3"
    shift 3
    if [[ ! -d "$WORK/sources/$name" ]]; then
        git clone --quiet --depth 1 --branch "$tag" "$url" "$WORK/sources/$name"
    fi
    cmake -S "$WORK/sources/$name" -B "$WORK/$name" "${common[@]}" \
        "-DCMAKE_INSTALL_PREFIX=$DEPS" -DCMAKE_INSTALL_BINDIR=bin -DCMAKE_INSTALL_LIBDIR=lib "$@"
    cmake --build "$WORK/$name" --parallel "$JOBS"
    cmake --install "$WORK/$name"
}
build_dependency qtkeychain https://github.com/frankosterfeld/qtkeychain.git 0.17.0 \
    -DBUILD_WITH_QT5=OFF -DBUILD_WITH_QT6=ON -DBUILD_TEST_APPLICATION=OFF -DBUILD_TRANSLATIONS=OFF
build_dependency qcoro https://github.com/qcoro/qcoro.git v0.13.0 \
    -DUSE_QT_VERSION=6 -DQCORO_WITH_QTDBUS=OFF -DQCORO_WITH_QTWEBSOCKETS=OFF -DQCORO_WITH_QTQUICK=OFF \
    -DQCORO_WITH_QML=OFF -DQCORO_WITH_QTTEST=OFF -DQCORO_BUILD_EXAMPLES=OFF -DBUILD_TESTING=OFF

# 4. cross-build swiftify
cmake -S "$SOURCE" -B "$WORK/swiftify" "${common[@]}" \
    "-DSWIFTIFY_SHAPE_ATLAS_TOOL=$SHAPE_ATLAS" \
    "${RUST_CMAKE_ARGS[@]}" \
    "-DCMAKE_INSTALL_PREFIX=$STAGE"
cmake --build "$WORK/swiftify" --parallel "$JOBS"
rm -rf "${STAGE:?}"
cmake --install "$WORK/swiftify"

# 5. bundle qt plugins, qml modules, fonts and dlls into $STAGE/bin
BIN="$STAGE/bin"
mkdir -p "$BIN"

# copy qt plugins needed at runtime
for plugin_dir in platforms tls imageformats iconengines styles networkinformation generic; do
    if [[ -d "$QT/plugins/$plugin_dir" ]]; then
        mkdir -p "$BIN/$plugin_dir"
        find "$QT/plugins/$plugin_dir" -maxdepth 1 -name '*.dll' -exec cp -f {} "$BIN/$plugin_dir/" \;
    fi
done

# copy qml modules used by the app
for qml_mod in QML QtCore QtQml QtQuick QtNetwork QtWebSockets Qt/labs; do
    if [[ -d "$QT/qml/$qml_mod" ]]; then
        mkdir -p "$BIN/qml/$(dirname "$qml_mod")"
        cp -rf "$QT/qml/$qml_mod" "$BIN/qml/$qml_mod"
        find "$BIN/qml/$qml_mod" -name '*.pdb' -delete
    fi
done

# copy d3dcompiler if present in the qt kit
for d3d in "$QT/bin/d3dcompiler_47.dll" "$QT/d3dcompiler_47.dll"; do
    [[ -f "$d3d" ]] && cp -f "$d3d" "$BIN/" && break
done

# recursively resolve and copy every non-system dll imported by the exe, plugins and qml modules
MINGW_SYSROOT_BIN="/usr/x86_64-w64-mingw32/bin"
MINGW_GCC_LIB="$(dirname "$(x86_64-w64-mingw32-g++ -print-file-name=libstdc++-6.dll)")"
SEARCH_DIRS=("$DEPS/bin" "$QT/bin" "$MINGW_GCC_LIB" "$MINGW_SYSROOT_BIN")

while :; do
    copied=false
    while read -r dll; do
        [[ -f "$BIN/$dll" ]] && continue
        for dir in "${SEARCH_DIRS[@]}"; do
            if [[ -f "$dir/$dll" ]]; then
                cp -f "$dir/$dll" "$BIN/"
                copied=true
                break
            fi
        done
    done < <(find "$BIN" -type f \( -iname '*.exe' -o -iname '*.dll' \) -exec x86_64-w64-mingw32-objdump -p {} + 2>/dev/null \
        | awk '/DLL Name:/ { print $3 }' | sort -u)
    $copied || break
done

cat > "$BIN/qt.conf" <<'EOF'
[Paths]
Plugins = .
QmlImports = qml
EOF

echo "built $STAGE (run $BIN/swiftify.exe)"
