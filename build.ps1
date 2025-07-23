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

# === Step 4: Deploy Artifacts ===
Write-Host ">> Deploying build output..."

# Deploy RED4ext client plugin DLL
$pluginOut = "$clientBuildDir\src\Release\Cyberverse.Red4Ext.dll"
$pluginDest = "C:\Program Files (x86)\Steam\steamapps\common\Cyberpunk 2077\red4ext\plugins\Cyberverse"
if (-Not (Test-Path $pluginDest)) {
    New-Item -ItemType Directory -Path $pluginDest | Out-Null
}
if (Test-Path $pluginOut) {
    Copy-Item $pluginOut -Destination $pluginDest -Force
} else {
    Write-Warning "⚠ Plugin DLL not found: $pluginOut"
}

# Deploy redscript files
$redscriptSrc = "$repoRoot\client\RedscriptModule\src\*.reds"
$redscriptDest = "C:\Program Files (x86)\Steam\steamapps\common\Cyberpunk 2077\r6\scripts"
Copy-Item $redscriptSrc -Destination $redscriptDest -Force

# Deploy managed server output
$managedOut = "$repoRoot\server\Managed\bin\$dotnetConfig\$dotnetFramework\*"
$managedDest = "$repoRoot\server\Managed\out"
if (-Not (Test-Path $managedDest)) {
    New-Item -ItemType Directory -Path $managedDest | Out-Null
}
try {
    Copy-Item $managedOut -Destination $managedDest -Recurse -Force
} catch {
    Write-Warning "⚠ Could not copy managed server output. Is the server running?"
}

# Deploy native server DLL to out
$nativeDll = "$nativeBuildDir\src\Release\Cyberverse.Server.Native.dll"
if (Test-Path $nativeDll) {
    Copy-Item $nativeDll -Destination $managedDest -Force
} else {
    Write-Warning "⚠ Native server DLL not found: $nativeDll"
}

Write-Host "✅ Build and deployment complete."
Write-Host "Run the server via:"
Write-Host "`tcd server/Managed/out"
Write-Host "`t./Cyberverse.Server.exe"
