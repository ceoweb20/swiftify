#Requires -Version 5.1
# installs spotify soloist for 64-bit windows, for development and ci. the app
# installs it by itself from settings -> spotify soloist. spotify forbids
# redistributing soloist, so it comes from spotify's own cdn.
param(
    [string]$Destination = "$env:LOCALAPPDATA\Programs\soloist"
)

$ErrorActionPreference = 'Stop'

if ($env:PROCESSOR_ARCHITECTURE -ne 'AMD64') {
    throw "Spotify Soloist for Windows supports x86_64 only, this is $env:PROCESSOR_ARCHITECTURE."
}

$url = 'https://soloist-builds.spotifycdn.com/soloist_release_windows_x86_64.zip'
$work = Join-Path ([IO.Path]::GetTempPath()) ([IO.Path]::GetRandomFileName())
New-Item -ItemType Directory -Path $work | Out-Null

try {
    $zip = Join-Path $work 'soloist.zip'
    Write-Host "Downloading Soloist for windows/x86_64"
    Invoke-WebRequest -Uri $url -OutFile $zip
    Expand-Archive -Path $zip -DestinationPath $work -Force

    $exe = Get-ChildItem -Path $work -Filter 'soloist.exe' -Recurse | Select-Object -First 1
    if (-not $exe) {
        throw 'The archive did not contain soloist.exe.'
    }
    New-Item -ItemType Directory -Force -Path $Destination | Out-Null
    $target = Join-Path $Destination 'soloist.exe'
    Copy-Item $exe.FullName $target -Force
    & $target --version

    Write-Host "Installed: $target"
    Write-Host 'API key: %LOCALAPPDATA%\soloist\api-key, or SOLOIST_API_KEY, or settings in Swiftify.'
}
finally {
    Remove-Item -Recurse -Force $work -ErrorAction SilentlyContinue
}
