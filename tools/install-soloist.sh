#!/bin/sh
# installs spotify soloist for this machine, for development and ci. the app
# installs it by itself from settings -> spotify soloist; this script is for
# shells. spotify forbids redistributing soloist, so it comes from spotify's
# own cdn. usage: tools/install-soloist.sh [destination, default ~/.local/bin]
set -eu

os=$(uname -s)
arch=$(uname -m)
case "$os" in
    Linux)
        case "$arch" in
            x86_64) archive=soloist_release_x86_64.tar.gz ;;
            aarch64|arm64) archive=soloist_release_arm64.tar.gz ;;
            armv7l) archive=soloist_release_arm32.tar.gz ;;
            *) echo "no soloist build for linux/$arch" >&2; exit 1 ;;
        esac
        ;;
    Darwin)
        # spotify builds soloist for apple silicon only
        if [ "$arch" != arm64 ]; then
            echo "soloist needs an apple silicon mac, this is $arch" >&2
            exit 1
        fi
        archive=soloist_release_osx_arm64.tar.gz
        ;;
    *)
        echo "no soloist build for $os; on windows use tools/install-soloist.ps1" >&2
        exit 1
        ;;
esac

destination=${1:-"$HOME/.local/bin"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT INT TERM

echo "downloading $archive for $os/$arch"
curl -fL --retry 3 -o "$work/soloist.tar.gz" "https://soloist-builds.spotifycdn.com/$archive"
tar -xzf "$work/soloist.tar.gz" -C "$work"

binary=$(find "$work" -type f -name soloist | head -n 1)
if [ -z "$binary" ]; then
    echo "the archive did not contain a soloist binary" >&2
    exit 1
fi
mkdir -p "$destination"
cp "$binary" "$destination/soloist"
chmod 755 "$destination/soloist"
"$destination/soloist" --version

echo "installed: $destination/soloist"
echo "api key: ~/.config/soloist/api-key, or SOLOIST_API_KEY, or settings in swiftify"
