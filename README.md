# Swiftify

a spotify player for desktop, built with Qt Quick on top of a Rust core that
wraps [librespot](https://github.com/librespot-org/librespot). it follows
Material 3 Expressive (heavy inspo from end4) and has an immersive player,
synced lyrics, visualizers and an equalizer.
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

## download

you can download swiftify for your operating system at [https://shredos.dev](https://shredos.dev/#download)

<br/>

## Contents

- [Before you start](#before-you-start)
- [Prebuilt builds](#prebuilt-builds)
- [Building from source](#building-from-source)
  - [Build requirements](#build-requirements)
  - [macOS](#macos)
  - [Linux](#linux)
  - [Windows](#windows)
- [Stats](#stats)
- [Command line](#command-line)

## Before you start

- **spotify premium.** playback goes through librespot, and that only works with premium.
- **a spotify client id.** your library, playlists and search come from the spotify
  web api through your own developer app. the first launch walks u through it:
  make an app in the [spotify developer dashboard](https://developer.spotify.com/dashboard),
  add `http://127.0.0.1:8898/login` as a redirect uri, and paste the apps client id
  into swiftify. you dont need the client secret.

## Tutorials

### spotify playback (librespot with soloist fallback)

spotify refuses librespot the keys to decrypt songs on newer accounts (librespot's
logs say `error audio key 0 1`). swiftify tries librespot first, and switches to
[spotify soloist](https://developer.spotify.com/documentation/soloist), spotify's
own connect client, only when Spotify refuses the account's audio key. Network
errors, unavailable tracks and cancelled requests do not switch players.
Once switched, Soloist handles subsequent tracks until logout or restart.
If it is not configured, playback stops with instructions to install Soloist
and enter your own API key in Settings → Account → Spotify Soloist.

soloist runs on linux (x86_64, aarch64, armv7), apple silicon macs and 64-bit
windows. there is no intel mac build. its binaries may not be redistributed, so
swiftify does not ship one: it downloads the build for your platform from
spotify when you ask it to.

1. open settings → spotify soloist and press "install soloist". it lands in
   swiftify's data directory; a soloist you installed yourself also works
   (`~/.local/bin/soloist`, `/opt/homebrew/bin/soloist`, on your `PATH`, or
   beside `swiftify.exe` on windows)
2. make a soloist api key in the [developer dashboard](https://developer.spotify.com/dashboard/soloist)
   and paste it in the same settings page. every user needs their own key;
   it is kept in `~/.config/soloist/api-key` (or `%LOCALAPPDATA%\soloist\api-key`
   on windows, or `SOLOIST_API_KEY`)
3. press play in swiftify. it starts soloist itself, as a connect device called "Swiftify".
   the first time, pick "Swiftify" once in the devices list of the spotify desktop app on
   the same computer (or the phone app), so soloist is signed in

for a shell, or a build agent, `tools/install-soloist.sh` (linux and macos) and
`tools/install-soloist.ps1` (windows) install the right build for the machine.

on linux with pipewire, swiftify makes a sink called "Swiftify (Soloist)" for soloist to play into,
and plays that on through its own output, so the equalizer and the visualizers still work.
under pulseaudio alone, or on macos and windows, soloist plays straight to the speakers.
`SWIFTIFY_SOLOIST_ROUTE=off` does that on purpose. tempo and pitch do not apply to
Soloist, and crossfade waits for each song to end. soloist builds expire after 90 days;
swiftify replaces its own download before that, and again if one ever expires.

if spotify on your phone doesnt list "Swiftify" for step 3, a firewall (ufw) is probably
dropping the pairing connection; pair from the spotify desktop app on the same computer.

### Discord RPC

Enable under Settings → Account → Discord Rich Presence (disabled by default) to broadcast 
your current track, artist, public album cover, and playback status to the Discord desktop app.
This uses a standard listening activity named Swiftify; pauses freeze progress and display Paused. 
Because this uses custom RPC rather than Spotify's official account integration, Listen Along is not supported.
Ensure Discord’s own activity privacy settings allow game/activity status.
To use Discord RPC you need to create an application on [Discord's Developer Portal](https://discord.com/developers/applications)
and copy it's ID into the settings. Swiftify connects over local IPC (Unix sockets or Windows named pipes) with zero external
SDKs and automatic reconnection.
It checks standard paths for native, Flatpak, Snap, Canary, PTB, and third-party clients like Vesktop.
For more verbose logging run swiftify with `QT_LOGGING_RULES="swiftify.discord.debug=true"`

## Prebuilt builds

The release workflow produces Windows/Linux x86_64 and macOS Apple Silicon/Intel
packages. A pushed version tag publishes them under
[releases](https://github.com/ceoweb20/swiftify/releases).

- **windows.** unzip it anywhere and run `bin\swiftify.exe`.
- **linux.** unpack it anywhere and run `bin/swiftify`. it brings its own Qt, but needs
  the system audio libraries described below.
- **macOS.** choose your architecture, unzip and move `Swiftify.app` to a writable
  application directory. Ad-hoc builds are not notarized; macOS may require approval.

Keep the extracted portable tree together. In-app installation is only available
in updater-enabled builds, in a user-writable installation; system installations
should use their package manager.

## Building from source

### Language and Discord activity

In **Settings → Appearance → Language**, choose **System**, **English**, or
**Русский**. The choice is saved and takes effect immediately, without restarting.
Track metadata, playlist names, search queries and custom preset names are not translated.

**Settings → Account → Discord Rich Presence** optionally shares the current track,
artist, public Spotify cover and progress with the running Discord **desktop** app.
It is off by default. Activity requests use the listening type and the name
**Spotify**. A pause keeps the track visible with **Paused / На паузе**, without
running timestamps; resuming restores progress. Disabling RPC clears the activity.
This is Rich Presence, not Spotify's official linked-account activity; it does not
enable Listen Along. Some Discord versions may prefer the application's registered
name over the requested activity name; in that case, rename your application in the
Developer Portal. Discord's own
activity privacy settings must also allow displaying activity.

This build has no shared Discord application registered for Swiftify yet. Create
an application named Swiftify at <https://discord.com/developers/applications> and
paste its numeric **Application ID** into the field, then press Enter or leave the
field. This is a public identifier, **not** a bot token, client secret, or Spotify
Client ID. Applications such as Sonora ship their developer's shared Application ID,
which is why their users don't need this step; do not reuse another app's ID.

RPC uses local IPC and automatically reconnects when Discord starts again. There
is no additional SDK or dependency. Local file paths and private cover URLs are
never included. The RPC protocol has automated tests against a fake local Discord
server; testing the real Discord status requires a valid application ID and Discord.

### Language and playback quality

English and Russian are available in Settings → Appearance → Language, including
menus, dialogs, playback errors and Soloist setup. Dates, weekdays and numbers
follow the selected language. Track names, artist names and user playlists are
not translated.

Librespot no longer advertises a second Connect device; only Soloist does so when
it is needed for fallback playback. The legacy device-name setting was removed.

The Lossless section appears only while the Soloist backend is selected. The
library account's subscription is read from Spotify's profile API. Premium is a
requirement, not proof that Lossless is available for the paired Soloist account
or its region. Configure the Swiftify device's quality in Spotify itself: the
current Soloist WebSocket API cannot set or verify delivered stream quality.

Translations use Qt `qsTr` / `qsTranslate` and an embedded UTF-8 catalog at
`translations/ru.json`; no additional Linguist tools are required. Russian plural
entries contain three forms (one, few, many).

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
library logins must belong to the same account. Section names follow the selected
interface language (refresh if changed during an in-flight load).

The discovery API is **internal and unsupported by Spotify**, so availability
and response formats can change. Query definitions are read only from Spotify's
public web-player assets, cached in memory for 24 hours, and never executed. No
external hash registry is used, and tokens remain in the Rust core.

**Jam is a live collaborative session, not a playlist. Spotify AI DJ also is not
a normal playlist and has no documented public API for its generated commentary
and program control. Neither native Jam sessions nor AI DJ are implemented here.**
Use the official Spotify client for those features. A playlist named DJ is not a
replacement for Spotify AI DJ.

### Continue after restarting

Swiftify remembers the current track, its position, queue, shuffle order, and
repeat mode. After restarting, the track is shown **paused**; only pressing Play
loads it into the audio core and resumes from the saved position. Startup never
starts music automatically. A restored position can also be changed before Play.

State is kept locally alongside preferences, checkpointed during playback and
flushed on orderly quit. Logging out clears it. Missing local files and unavailable
Spotify tracks are not silently replaced with another song; Play reports the usual
loading error. Invalid snapshots are ignored. Extremely large queues keep up to
2,000 entries around the current occurrence and up to 100 history entries.

### Full-screen visualizers

**Settings → Visualizer → Full-screen player visualizer** controls the immersive
bars/wave, artwork ring and the live spectrum behind the equalizer independently
of the player-bar and track-list meters. The same switch is available at the top
of the full-screen Sound panel. The global visualizer switch still takes priority.
Decorative progress-bar shapes and the live background have their own settings.

### Saved logins without Keychain

From 0.17.0, Spotify logins are stored in an **encrypted file**, not Keychain or
another OS credential backend. Create a separate vault password (at least 12
characters) and sign in to Spotify once again. Old Keychain entries are left
untouched; the app never attempts to read them. Each launch asks for the vault
password once; reopening the window and refreshing tokens do not ask again.

The key stays only in memory. There is no plaintext fallback or forgotten-password
recovery. OTA updates preserve the encrypted file. See [credential vault details,
migration, limitations and recovery](docs/credential-vault.md).

### Portable builds and updates

GitHub Actions builds macOS (Apple Silicon and Intel), Windows x64 and Linux x64
portable packages. When all four builds pass on `main` and `CMakeLists.txt` names a
version that has no release yet, that release is published with the packages and
signed update manifests. Pushing a `v<version>` tag does the same. The portable packages look for new releases once a day and install a verified one
by themselves, restarting once nothing is playing (**Settings → About → Updates**;
both switches can be turned off). System/package-manager installations are not
overwritten.

Releases are signed with this repository's own Ed25519 key, and the macOS packages with
a certificate of its own (not notarized, so a browser download needs right-click → Open
once). See [release/update setup, trust model and limitations](docs/updates.md).

### Saving the current song and Windows widgets

The heart beside the current track in the bottom player and full-screen player
uses the same Spotify saved-song state as track menus. It stays synchronized
across both players, and ignores duplicate clicks while Spotify is updating the library. Saving still requires a connected Spotify
library with the existing saved-song permissions.

Windows builds with a sufficiently recent SMTC SDK also publish track duration
and position through the media-session timeline, not just taskbar progress.
Widgets using the Windows media-session API can read these values; other/custom
widget integrations may need their own support. Runtime verification on Windows
is still required.

### Build requirements

- Qt 6.10 or newer: Qt Base, Qt Declarative, Qt Shader Tools and Qt WebSockets
- QCoro 0.10 or newer, built for Qt 6
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
brew install qtbase qtdeclarative qtshadertools qtwebsockets qcoro6 cmake ninja
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y
source "$HOME/.cargo/env"

git clone https://github.com/ceoweb20/swiftify.git
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
git clone https://github.com/ceoweb20/swiftify.git
cd swiftify/packaging/arch
makepkg -si
```
or use cmake:

```sh
# replace with your package manager if not on arch
sudo pacman -S --needed git qt6-base qt6-declarative qt6-shadertools qt6-wayland qt6-websockets qcoro libpulse alsa-lib openssl cmake ninja rust pkgconf

git clone https://github.com/ceoweb20/swiftify.git
cd swiftify
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build build
cmake --install build
```

on debian/ubuntu (or derivatives with Qt 6.8 or 6.9), use the build script that handles
compatibility patches automatically:

```sh
sudo apt-get install git qt6-base-dev qt6-declarative-dev qt6-shader-baker \
    qt6-shadertools-dev qt6-wayland qt6-websockets-dev qcoro-qt6-dev \
    libpulse-dev libasound2-dev libssl-dev cmake ninja-build pkg-config
# install rust from https://rustup.rs if you dont have it

git clone https://github.com/ceoweb20/swiftify.git
cd swiftify
./tools/build-debian.sh
```

or with `--install-deps` to install the dependencies automatically:

```sh
git clone https://github.com/ceoweb20/swiftify.git
cd swiftify
./tools/build-debian.sh --install-deps
```

anywhere else, install the same reqs (Qt 6.10+ with D-Bus, OpenGL and Wayland,
QCoro for Qt 6, libpulse, plus the ALSA, OpenSSL and pkg-config dev packages), then
run the cmake commands above. if you have Qt 6.8 or 6.9, use `tools/build-debian.sh` which
applies the necessary compatibility patches.

`cmake --install` drops `swiftify` in `~/.local/bin` (make sure that's on your `PATH`)
and the desktop entry, icons and fonts in `~/.local/share`. 

Logins use the password-protected vault on Linux too; no secret-service daemon is needed.

on linux it also hooks into MPRIS (media keys, `playerctl`, shell media widgets),
logind (pauses before the machine sleeps), pipewire/pulseaudio device changes, and
the tray. `-DSWIFTIFY_DESKTOP_INTEGRATION=OFF` turns those off.

### Windows

**because most Windows users cant even use their own OS, i made a script that handles building so it's as simple as possible :)**

first install Qt 6.10+ with *Qt Shader Tools* and *Qt WebSockets*, CMake and Ninja from the Qt online installer,
Rust from [rustup](https://rustup.rs), and Git. choose either Qt for *MSVC 2022 64-bit*
with Visual Studio 2022 17.1+ (or Build Tools) and its *Desktop development with C++*
workload, or Qt for *MinGW 64-bit* with its matching MinGW toolchain.

then run this in Windows PowerShell:

```powershell
git clone https://github.com/ceoweb20/swiftify.git
Set-Location swiftify
.\tools\build-windows.ps1
```

for a different Qt kit, toolchain location or install directory, pass overrides:

```powershell
.\tools\build-windows.ps1 -QtPath 'C:\Qt\6.11.2\msvc2022_64'
.\tools\build-windows.ps1 -QtPath 'C:\Qt\6.10.3\mingw_64' -MingwPath 'C:\Qt\Tools\mingw1310_64\bin' -InstallPrefix "$env:LOCALAPPDATA\Programs\Swiftify"
```

adjust the example paths to your installed versions, MinGW must match your Qt
kit, not just the example folder name. 

**the toolchain decides one feature:** the Windows media controls (the media overlay,
lock screen and media keys showing what's playing) need `SystemMediaTransportControls`
in the compiler's Windows headers.

| toolchain | media controls |
|---|---|
| MSVC 2022 | yes |
| Qt's MinGW 13.1 (`mingw1310_64`, mingw-w64 runtime v11) | no, everything else works |
| a newer mingw-w64 (tested: GCC 16.1, runtime v14) | yes |

when they're left out, cmake warns `building without the Windows media controls`.
for every feature, build with MSVC. a newer MinGW works too, but it isn't the one
your Qt kit was built with, so the script warns about the mismatch.

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


## Stats

the Stats page (in the sidebar, or `swiftify --stats`) shows what you've been listening to,
like the [listening-stats](https://github.com/HeadshotInteractive/listening-stats) spicetify app:
listening time, plays, top songs, artists, albums and genres, when in the day and the week you
listen, a calendar of the last year, and your recent plays. pick a period (today, 4 weeks,
6 months, all time) and rank by plays or by time.

it reads from either:

- **stats.fm**, by username. your profile's stats have to be public; nothing else is needed.
- **this computer**: every song you listen to for 30 seconds or more (or half of a shorter one)
  gets kept in `listening-history.jsonl` in swiftify's data folder, and never leaves it. genres
  come from spotify through your library login. you can export the history as csv, or turn
  tracking off, under Settings → Listening Stats.

## Command line

only one swiftify runs at a time. launching it again just passes the command to the one
that's already running, so these work great as keybinds:

| flag | |
|---|---|
| `--immersive` | open the immersive player |
| `--play-pause` | play or pause |
| `--next` | next song |
| `--previous` | previous song |
| `--stats` | open the stats page |

on hyprland for example:

```ini
bind = SUPER SHIFT, M, exec, swiftify --immersive
bind = , XF86AudioPlay, exec, swiftify --play-pause
```
