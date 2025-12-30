# Build and deploy ShowMe VST3 plugin
param(
    [switch]$Deploy  # Add -Deploy to copy to FL Studio's VST3 folder
)

$ErrorActionPreference = "Stop"
$ProjectRoot = $PSScriptRoot

# Find MSBuild
$MSBuild = "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe"
if (-not (Test-Path $MSBuild)) {
    # Try common alternatives
    $alternatives = @(
        "C:\Program Files\Microsoft Visual Studio\2022\*\MSBuild\Current\Bin\amd64\MSBuild.exe",
        "C:\Program Files (x86)\Microsoft Visual Studio\2022\*\MSBuild\Current\Bin\amd64\MSBuild.exe"
    )
    foreach ($alt in $alternatives) {
        $found = Get-ChildItem $alt -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($found) { $MSBuild = $found.FullName; break }
    }
}

if (-not (Test-Path $MSBuild)) {
    Write-Error "MSBuild not found. Install Visual Studio Build Tools."
    exit 1
}

Write-Host "Building..." -ForegroundColor Cyan
& $MSBuild "$ProjectRoot\Builds\VisualStudio2022\ShowMe.sln" -p:Configuration=Release -p:Platform=x64 -v:minimal

if ($LASTEXITCODE -ne 0) {
    Write-Error "Build failed"
    exit 1
}

$VST3Path = "$ProjectRoot\Builds\VisualStudio2022\x64\Release\VST3\ShowMe.vst3"
Write-Host "Build successful: $VST3Path" -ForegroundColor Green

if ($Deploy) {
    $Destination = "C:\Program Files\Common Files\VST3\ShowMe.vst3"
    Write-Host "Deploying to $Destination..." -ForegroundColor Cyan

    # Delete old and copy new (need admin for Program Files)
    $deleteCmd = "if exist `"$Destination`" rmdir /s /q `"$Destination`""
    $copyCmd = "xcopy /E /I /Y `"$VST3Path`" `"$Destination\`""
    Start-Process cmd -ArgumentList "/c $deleteCmd && $copyCmd" -Verb RunAs -Wait

    Write-Host "Deployed! Restart FL Studio and rescan plugins." -ForegroundColor Green
}
