# Publishes a mod update that CoopLauncher.exe picks up on the players' PCs.
#
#   .\scripts\publish_update.ps1 -Tag v0.2.1 -Notes "Time of day sync"
#
# Makes dist\cdmp-update.zip (cdmp.asi + winmm.dll; never cdmp.ini or the
# launcher) and a GitHub release in $Repo with it. The release notes get
# "game_pe=<timestamp>" from offsets.h: the launcher switches the mod off on
# any other game build. Also refreshes dist\friend-first-install.zip.

param(
    [Parameter(Mandatory)] [string]$Tag,
    [string]$Notes = '',
    [string]$Repo = 'macauleywilliams96-dotcom/CrimsonDesertCoop',
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo')]
    [string]$Config = 'RelWithDebInfo'
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$bin = Join-Path $root "build\bin\$Config"
$dist = Join-Path $root 'dist'
foreach ($f in 'cdmp.asi', 'CoopLauncher.exe') {
    if (-not (Test-Path (Join-Path $bin $f))) { throw "Build first: $f not found in $bin" }
}
$offsets = Get-Content (Join-Path $root 'src\mod\game\offsets.h') -Raw
if ($offsets -notmatch 'kPeTimestamp = 0x([0-9A-Fa-f]{8})') { throw 'kPeTimestamp not found in offsets.h' }
$gamePe = $Matches[1].ToUpper()
$loader = Join-Path $root 'third_party\ultimate-asi-loader\v9.7.4\dinput8.dll'

# The update (what the launcher extracts into bin64).
$stage = Join-Path $dist 'update'
if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
New-Item -ItemType Directory -Force $stage | Out-Null
Copy-Item (Join-Path $bin 'cdmp.asi') $stage
Copy-Item $loader (Join-Path $stage 'winmm.dll')
$zip = Join-Path $dist 'cdmp-update.zip'
if (Test-Path $zip) { Remove-Item $zip }
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip

# First install for a new player: launcher + mod + guest config.
$first = Join-Path $dist 'first-install'
if (Test-Path $first) { Remove-Item -Recurse -Force $first }
New-Item -ItemType Directory -Force $first | Out-Null
Copy-Item (Join-Path $stage '*') $first
Copy-Item (Join-Path $bin 'CoopLauncher.exe') $first
# dist\friend\cdmp.ini is the guest config with the SteamIDs filled in, if made.
$guestIni = Join-Path $dist 'friend\cdmp.ini'
if (-not (Test-Path $guestIni)) { $guestIni = Join-Path $root 'kit\coop\cdmp.ini.guest' }
Copy-Item $guestIni (Join-Path $first 'cdmp.ini')
Copy-Item (Join-Path $root 'kit\coop\README.txt') $first
Set-Content -Encoding ascii (Join-Path $first 'cdmp_version.txt') $Tag
$firstZip = Join-Path $dist 'friend-first-install.zip'
if (Test-Path $firstZip) { Remove-Item $firstZip }
Compress-Archive -Path (Join-Path $first '*') -DestinationPath $firstZip

$body = "$Notes`n`ngame_pe=$gamePe"
gh release create $Tag $zip --repo $Repo --title $Tag --notes $body
if ($LASTEXITCODE -ne 0) { throw 'gh release create failed' }
Write-Host "Published $Tag to $Repo (game_pe=$gamePe). First install: $firstZip"
