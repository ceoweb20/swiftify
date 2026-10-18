# Swiftify

a spotify player for desktop, built with Qt Quick on top of a Rust core that
wraps [librespot](https://github.com/librespot-org/librespot). it follows
Material 3 Expressive (heavy inspo from end4) and has an immersive player,
synced lyrics, visualizers, an equalizer, and local files with local playlists
where u can edit the tracks metadata.
![Alt text](/assets/home.png)
this apps purpose is to replace spotifys heavy webview client, but its also mostly eyecandy lol
it runs significantly lighter than the spotify client.

**EVEN WITH THE EYECANDY TURNED ON....** it is lighter than spotify + spotifys renderers both on cpu AND ram!!!!!

id recommend turning the visualizers and background off if the app runs in the background tho lol

***anyways runs on linux (made for wayland and hyprland) macos, and windows.***

## previews
first image shows equalizer, tempo and pitch settings. second picture shows how the immersive player and the visualizer/lyrics look
<table border="0">
  <tr>
    <td><img src="/assets/eq.png" alt="equalizer and tempo" width="400"/></td>
    <td><img src="/assets/immersive.png" alt="immersive player with visualizers" width="400"/></td>
  </tr>
</table>

> ### Note
> i will probably not be maintaining it, if you are interested in taking over this project you can send me an email

- [Before you start](#before-you-start)
- [Building from source](#building-from-source)
  - [Build requirements](#build-requirements)
  - [macOS](#macos)
  - [Linux](#linux)
  - [Windows](#windows)
- [Command line](#command-line)

## Before you start

- **spotify premium.** playback goes through librespot, and that only works with premium.
- **a spotify client id.** your library, playlists and search come from the spotify
  web api through your own developer app. the first launch walks u through it:
  make an app in the [spotify developer dashboard](https://developer.spotify.com/dashboard),
  add `http://127.0.0.1:8898/login` as a redirect uri, and paste the apps client id
  into swiftify. you dont need the client secret.

## Building from source

### Personalized Spotify playlists

**Made for You / Для тебя** in the sidebar opens all personalized playlists
returned for the signed-in playback account. Home also shows Spotify's own
playlist shelves and the Made For You page (Daily Mixes, Blends, On Repeat,
Discover Weekly, etc., when Spotify provides them). Shelves scroll horizontally;
the full grid is not limited to the home page's eight-card previews.

Saved playlists are also read from the session rootlist, including playlists
inside folders, and merged by ID with the existing Web API library. Personalized
recommendations remain separate from saved playlists and cannot be edited unless
the account actually owns or collaborates on a saved copy. They can be pinned in
Swiftify without following or modifying them on Spotify.

This uses the existing playback login: no cookies, new scopes, or developer app
are needed. A refresh button reloads collections; failures preserve existing
collections and do not block playback or the ordinary library. The playback and
library logins must belong to the same account. Section names follow the system language.

The discovery API is **internal and unsupported by Spotify**, so availability
and response formats can change. Query definitions are read only from Spotify's
public web-player assets, cached in memory for 24 hours, and never executed. No
external hash registry is used, and tokens remain in the Rust core.

**Jam is a live collaborative session, not a playlist. Spotify AI DJ also is not
a normal playlist and has no documented public API for its generated commentary
and program control. Neither native Jam sessions nor AI DJ are implemented here.**
Use the official Spotify client for those features. A playlist named DJ is not a
replacement for Spotify AI DJ.

### Build requirements

- Qt 6.9 or newer: Qt Base, Qt Declarative and Qt Shader Tools
- QtKeychain and QCoro 0.10 or newer, both built for Qt 6
- CMake 3.24 or newer, Ninja, and a C++20 compiler
- Rust (stable, from [rustup](https://rustup.rs))

the first cmake run needs internet: it pulls [Corrosion](https://github.com/corrosion-rs/corrosion)
(which builds the rust core as part of the cmake build) and two fonts, Material Symbols
Rounded and Google Sans Flex, each checked against a pinned hash.

the builds below skip the tests with `-DBUILD_TESTING=OFF`. for a manual CMake
build, drop that and run `ctest --test-dir build` if u want them.

### macOS

u need [homebrew](https://brew.sh) and apple's command line tools (`xcode-select --install`, or full xcode). then:

```sh
brew install qtbase qtdeclarative qtshadertools qtkeychain qcoro6 cmake ninja
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y
source "$HOME/.cargo/env"

git clone https://github.com/shredos67/swiftify.git
cd swiftify
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build
ditto build/swiftify.app /Applications/Swiftify.app
```

it's signed ad hoc, so no developer account needed.
with full xcode installed the icon gets compiled with `actool` and picks up macos 26's glass,
dark and tinted looks; with just the command line tools it's built from the PNGs.

### Linux

on arch either build a proper package, which installs to `/usr`:

```sh
git clone https://github.com/shredos67/swiftify.git
cd swiftify/packaging/arch
makepkg -si
```
or use cmake:

```sh
# replace with your package manager if not on arch
sudo pacman -S --needed git qt6-base qt6-declarative qt6-shadertools qt6-wayland qtkeychain-qt6 qcoro libpulse alsa-lib openssl cmake ninja rust pkgconf

git clone https://github.com/shredos67/swiftify.git
cd swiftify
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build build
cmake --install build
```

on debian/ubuntu (or derivatives with Qt 6.8), use the build script that handles
compatibility patches automatically:

```sh
sudo apt-get install git qt6-base-dev qt6-declarative-dev qt6-shader-baker \
    qt6-shadertools-dev qt6-wayland qtkeychain-qt6-dev qcoro-qt6-dev \
    libpulse-dev libasound2-dev libssl-dev cmake ninja-build pkg-config
# install rust from https://rustup.rs if you dont have it

git clone https://github.com/shredos67/swiftify.git
cd swiftify
./tools/build-debian.sh
```

or with `--install-deps` to install the dependencies automatically:

```sh
git clone https://github.com/shredos67/swiftify.git
cd swiftify
./tools/build-debian.sh --install-deps
```

anywhere else, install the same reqs (Qt 6.9+ with D-Bus, OpenGL and Wayland, QtKeychain
and QCoro for Qt 6, libpulse, plus the ALSA, OpenSSL and pkg-config dev packages), then
run the cmake commands above. if you have Qt 6.8, use `tools/build-debian.sh` which
applies the necessary compatibility patches.

`cmake --install` drops `swiftify` in `~/.local/bin` (make sure that's on your `PATH`)
and the desktop entry, icons and fonts in `~/.local/share`. 

your login gets saved in the secret service through QtKeychain, so u need something
providing one: gnome-keyring, kwallet or keepassxc.

on linux it also hooks into MPRIS (media keys, `playerctl`, shell media widgets),
logind (pauses before the machine sleeps), pipewire/pulseaudio device changes, and
the tray. `-DSWIFTIFY_DESKTOP_INTEGRATION=OFF` turns those off.

### Windows

**because most Windows users cant even use their own OS, i made a script that handles building so it's as simple as possible :)**

first install Qt 6.9+ with *Qt Shader Tools*, CMake and Ninja from the Qt online installer,
Rust from [rustup](https://rustup.rs), and Git. choose either Qt for *MSVC 2022 64-bit*
with Visual Studio 2022 17.1+ (or Build Tools) and its *Desktop development with C++*
workload, or Qt for *MinGW 64-bit* with its matching MinGW toolchain.

then run this in Windows PowerShell:

```powershell
git clone https://github.com/shredos67/swiftify.git
Set-Location swiftify
.\tools\build-windows.ps1
```

for a different Qt kit, toolchain location or install directory, pass overrides:

```powershell
.\tools\build-windows.ps1 -QtPath 'C:\Qt\6.11.2\msvc2022_64'
.\tools\build-windows.ps1 -QtPath 'C:\Qt\6.9.3\mingw_64' -MingwPath 'C:\Qt\Tools\mingw1310_64\bin' -InstallPrefix "$env:LOCALAPPDATA\Programs\Swiftify"
```

adjust the example paths to your installed versions, MinGW must match your Qt
kit, not just the example folder name. 

quit a running installed copy through its tray menu before updating it.

if PowerShell blocks the script, review it first, then run
`powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\build-windows.ps1`.
this applies only to that process, not your machine's execution policy.

start the installed app with:

```powershell
& "$env:LOCALAPPDATA\Programs\Swiftify\bin\swiftify.exe"
```

font downloads are hash-checked on every configure; on Windows, a failed CMake
download is retried with `curl.exe` using the system certificate store.

## Command line

only one swiftify runs at a time. launching it again just passes the command to the one
that's already running, so these work great as keybinds:

| flag | |
|---|---|
| `--immersive` | open the immersive player |
| `--play-pause` | play or pause |
| `--next` | next song |
| `--previous` | previous song |

on hyprland for example:

```ini
bind = SUPER SHIFT, M, exec, swiftify --immersive
bind = , XF86AudioPlay, exec, swiftify --play-pause
```
