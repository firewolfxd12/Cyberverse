# ================================================
# Full Build Script for Cyberverse (Windows + PowerShell)
# Save this file as build.ps1 and run it from the project root
# ================================================

# === CONFIG ===
$repoRoot = "$PSScriptRoot"
$vsBuildConfig = "Release"
$dotnetConfig = "Release"
$dotnetFramework = "net8.0"

# === Step 1: Build Native Server (C++) ===
Write-Host ">> Building native server..."
$nativeBuildDir = "$repoRoot\server\Native\build"
if (-Not (Test-Path $nativeBuildDir)) {
    New-Item -ItemType Directory -Path $nativeBuildDir | Out-Null
}
Push-Location $nativeBuildDir
cmake ..
Pop-Location

# Build native C++ project using MSBuild
$nativeSln = "$nativeBuildDir\Cyberverse.Server.Native.sln"
Write-Host ">> Compiling C++ solution..."
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" $nativeSln /p:Configuration=$vsBuildConfig

# === Step 2: Build Managed Server (C#/.NET) ===
Write-Host ">> Building managed C# server..."
Push-Location "$repoRoot\server\Managed"
dotnet restore
dotnet build -c $dotnetConfig
Pop-Location

# === Step 3: Build Client Plugin (RED4ext) ===
Write-Host ">> Building RED4ext client plugin..."
$clientRed4extDir = "$repoRoot\client\red4ext"
$clientBuildDir = "$clientRed4extDir\build"
if (-Not (Test-Path $clientBuildDir)) {
    New-Item -ItemType Directory -Path $clientBuildDir | Out-Null
}
Push-Location $clientBuildDir
cmake ..
Pop-Location

$clientSln = "$clientBuildDir\Cyberverse.RED4ext.sln"
Write-Host ">> Compiling RED4ext client plugin..."
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" $clientSln /p:Configuration=$vsBuildConfig
