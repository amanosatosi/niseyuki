param(
    [string]$BuildDir = "build",
    [string]$Configuration = "Release",
    [string]$QtBinDir = "",
    [string]$FfmpegDir = "",
    [string]$OutputZip = "niseyuki-windows.zip"
)

$ErrorActionPreference = "Stop"

function Resolve-WindeployQt {
    param([string]$QtBinDir)
    if ($QtBinDir) {
        $candidate = Join-Path $QtBinDir "windeployqt.exe"
        if (Test-Path $candidate) {
            return (Resolve-Path $candidate).Path
        }
    }
    $cmd = Get-Command windeployqt.exe -ErrorAction SilentlyContinue
    if ($cmd) {
        return $cmd.Source
    }
    return $null
}

$buildDirFull = Resolve-Path -Path $BuildDir
$stageRoot = Join-Path $buildDirFull "package"
if (Test-Path $stageRoot) {
    Remove-Item $stageRoot -Recurse -Force
}
New-Item -ItemType Directory -Path $stageRoot | Out-Null

$installDir = Join-Path $stageRoot "install"
& cmake --install $buildDirFull --config $Configuration --prefix $installDir

$installedBin = Join-Path $installDir "bin"
if (-not (Test-Path $installedBin)) {
    throw "Install step did not produce a bin directory."
}

$bundleDir = Join-Path $stageRoot "Niseyuki"
New-Item -ItemType Directory -Path $bundleDir | Out-Null
Copy-Item -Path (Join-Path $installedBin '*') -Destination $bundleDir -Recurse -Force

$exePath = Join-Path $bundleDir "niseyuki.exe"
if (-not (Test-Path $exePath)) {
    throw "Unable to find niseyuki.exe in bundle directory."
}

$windeployqt = Resolve-WindeployQt -QtBinDir $QtBinDir
if ($null -eq $windeployqt) {
    Write-Warning "windeployqt.exe not found. Qt dependencies were not copied."
} else {
    & $windeployqt "--release" "--force" "--dir" $bundleDir $exePath
}

if ($FfmpegDir) {
    $ffmpegSource = Resolve-Path -Path $FfmpegDir
    $ffmpegTarget = Join-Path $bundleDir "ffmpeg\bin"
    if (-not (Test-Path $ffmpegTarget)) {
        New-Item -ItemType Directory -Path $ffmpegTarget | Out-Null
    }
    $executables = @("ffmpeg.exe", "ffprobe.exe", "ffplay.exe")
    foreach ($exe in $executables) {
        $candidate = Join-Path $ffmpegSource $exe
        if (Test-Path $candidate) {
            Copy-Item $candidate -Destination $ffmpegTarget -Force
        }
    }
}

$zipPath = Join-Path (Split-Path $buildDirFull -Parent) $OutputZip
if (Test-Path $zipPath) {
    Remove-Item $zipPath -Force
}
Add-Type -AssemblyName System.IO.Compression.FileSystem
[System.IO.Compression.ZipFile]::CreateFromDirectory($bundleDir, $zipPath)

Write-Host "Created package at $zipPath"
