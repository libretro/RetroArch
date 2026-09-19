param(
    [Parameter(Mandatory)][string]$Executable,
    [Parameter(Mandatory)][string]$SourceDirectory,
    [Parameter(Mandatory)][string]$AssetsDirectory,
    [Parameter(Mandatory)][string]$InfoDirectory,
    [Parameter(Mandatory)][string]$OutputDirectory,
    [Parameter(Mandatory)][ValidatePattern('^[0-9a-f]{40}$')][string]$SourceRevision,
    [Parameter(Mandatory)][ValidatePattern('^[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+$')][string]$Repository,
    [Parameter(Mandatory)][ValidatePattern('^[0-9a-f]{40}$')][string]$AssetsRevision,
    [Parameter(Mandatory)][ValidatePattern('^[0-9a-f]{40}$')][string]$InfoRevision
)
$ErrorActionPreference = 'Stop'
$exe = (Resolve-Path -LiteralPath $Executable).Path
$source = (Resolve-Path -LiteralPath $SourceDirectory).Path
$assets = (Resolve-Path -LiteralPath $AssetsDirectory).Path
$info = (Resolve-Path -LiteralPath $InfoDirectory).Path

$reader = [IO.BinaryReader]::new([IO.File]::OpenRead($exe))
try {
    if ($reader.ReadUInt16() -ne 0x5a4d) { throw 'The executable has no DOS header.' }
    $reader.BaseStream.Position = 0x3c
    $offset = $reader.ReadUInt32()
    $reader.BaseStream.Position = $offset
    if ($reader.ReadUInt32() -ne 0x4550 -or $reader.ReadUInt16() -ne 0xaa64) {
        throw 'The executable must be a native ARM64 PE image.'
    }
} finally { $reader.Dispose() }

foreach ($path in @("$source/COPYING", "$assets/COPYING", "$assets/ozone/regular.ttf", "$assets/ozone/bold.ttf", "$assets/pkg/fallback-font.ttf", "$assets/pkg/chinese-fallback-font.ttf", "$info/COPYING", "$info/dist/info/fbneo_libretro.info")) {
    if (!(Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing packaging input: $path" }
}
$null = New-Item -ItemType Directory -Path $OutputDirectory -Force
$out = (Resolve-Path -LiteralPath $OutputDirectory).Path
$archive = Join-Path $out 'RetroArch-WinARM64.zip'
if (Test-Path -LiteralPath $archive) { throw "Output already exists: $archive" }
$stageRoot = Join-Path $out ([guid]::NewGuid().ToString('N'))
$stage = Join-Path $stageRoot 'RetroArch-WinARM64'
$null = New-Item -ItemType Directory -Path $stage -Force
foreach ($dir in @('assets/pkg', 'assets/fonts', 'info', 'cores', 'system', 'saves', 'states', 'screenshots', 'config', 'playlists', 'cache', 'downloads', 'licenses')) {
    $null = New-Item -ItemType Directory -Path (Join-Path $stage $dir) -Force
}
Copy-Item -LiteralPath $exe -Destination "$stage/retroarch.exe"
Copy-Item -LiteralPath "$source/COPYING" -Destination "$stage/COPYING"
Copy-Item -LiteralPath "$assets/ozone" -Destination "$stage/assets/ozone" -Recurse
Copy-Item -LiteralPath "$assets/COPYING" -Destination "$stage/licenses/retroarch-assets.txt"
foreach ($name in @('fallback-font.ttf', 'fallback-font.txt', 'chinese-fallback-font.ttf', 'chinese-fallback-font.txt')) {
    Copy-Item -LiteralPath "$assets/pkg/$name" -Destination "$stage/assets/pkg/$name"
}
foreach ($name in @('DejaVuSans.LICENSE.txt', 'mplus-1pLICENSE_E.txt', 'mplus-1p_LICENSE_J.txt')) {
    Copy-Item -LiteralPath "$assets/fonts/$name" -Destination "$stage/assets/fonts/$name"
}
Copy-Item -Path "$info/dist/info/*.info" -Destination "$stage/info"
Copy-Item -LiteralPath "$info/COPYING" -Destination "$stage/licenses/core-info.txt"
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'FONT-LICENSES.txt') -Destination "$stage/licenses/FONT-LICENSES.txt"

@'
assets_directory = ":/assets"
libretro_directory = ":/cores"
libretro_info_path = ":/info"
system_directory = ":/system"
savefile_directory = ":/saves"
savestate_directory = ":/states"
screenshot_directory = ":/screenshots"
playlist_directory = ":/playlists"
rgui_config_directory = ":/config"
cache_directory = ":/cache"
core_assets_directory = ":/downloads"
menu_driver = "ozone"
video_driver = "d3d11"
audio_driver = "wasapi"
'@ | Set-Content -LiteralPath "$stage/retroarch.cfg" -Encoding utf8

@"
RetroArch for Windows ARM64

Extract this directory to a writable location and run retroarch.exe.
This portable package includes the Ozone menu assets and core metadata.
Other menu themes, shaders, databases and controller profiles can be installed
separately. No cores, game ROMs or BIOS files are included.

Only native Windows ARM64 cores can be loaded by this executable. x64 and x86
core DLLs are incompatible. Core metadata does not imply that an ARM64 core is
available from the online updater; install a compatible core manually when needed.

RetroArch is distributed under GPL-3.0-or-later. See COPYING. Bundled assets and
fonts retain their own licenses and attribution in licenses/ and assets/fonts/.
Ozone assets are copied without modification from libretro/retroarch-assets.
Core metadata is copied from libretro/libretro-super (MIT).

Corresponding complete source, including the MSVC projects:
https://github.com/$Repository/archive/$SourceRevision.zip
Build Release|ARM64 using pkg/msvc/RetroArch-msvc2022.sln with Visual Studio C++
ARM64 tools and a Windows SDK. Packaging is implemented by
pkg/msvc/packaging/package-arm64.ps1 and .github/workflows/Windows-ARM64.yml.
Exact resource revisions are recorded in build-info.json. Font notices in this
package correspond to the pinned asset revision used by that workflow.
"@ | Set-Content -LiteralPath "$stage/README.txt" -Encoding utf8

@{
    architecture = 'ARM64'
    source_repository = $Repository
    source_revision = $SourceRevision
    assets_repository = 'libretro/retroarch-assets'
    assets_revision = $AssetsRevision
    core_info_repository = 'libretro/libretro-super'
    core_info_revision = $InfoRevision
    executable_sha256 = (Get-FileHash -LiteralPath "$stage/retroarch.exe" -Algorithm SHA256).Hash.ToLowerInvariant()
} | ConvertTo-Json | Set-Content -LiteralPath "$stage/build-info.json" -Encoding utf8

# Keep smoke-test state outside the distributable directory.
$smokeConfig = Join-Path $stageRoot 'smoke.cfg'
@'
config_save_on_exit = "false"
video_driver = "null"
audio_enable = "false"
menu_driver = "rgui"
pause_nonactive = "false"
history_list_enable = "false"
'@ | Set-Content -LiteralPath $smokeConfig -Encoding utf8
$log = Join-Path $stageRoot 'smoke.log'
$launchArgs = @('--config', "`"$stage/retroarch.cfg`"", '--appendconfig', "`"$smokeConfig`"", '--menu', '--verbose', '--max-frames', '180', '--log-file', "`"$log`"")
$process = Start-Process -FilePath "$stage/retroarch.exe" -WorkingDirectory $stageRoot -ArgumentList $launchArgs -WindowStyle Hidden -PassThru
if (!$process.WaitForExit(45000)) {
    Stop-Process -Id $process.Id
    throw 'The ARM64 menu smoke test timed out.'
}
if ($process.ExitCode -ne 0) { throw "The ARM64 menu smoke test failed: $($process.ExitCode). See $log" }
$logText = Get-Content -LiteralPath $log -Raw
if ($logText -notmatch '\[Video\] Found display server: "null"') { throw 'The menu smoke test did not initialize the null display server.' }

Compress-Archive -LiteralPath $stage -DestinationPath $archive -CompressionLevel Optimal
$digest = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant()
"$digest  RetroArch-WinARM64.zip" | Set-Content -LiteralPath "$archive.sha256" -Encoding ascii
Write-Output "Packaged native ARM64 frontend: $archive"
