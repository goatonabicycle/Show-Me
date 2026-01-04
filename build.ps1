# Build and deploy Show Me plugin suite
param(
    [switch]$Deploy,          # Add -Deploy to copy to FL Studio's VST3 folder
    [string]$Plugin = "all"   # "midi", "audio", or "all" (default)
)

$ErrorActionPreference = "Stop"
$ProjectRoot = $PSScriptRoot

# Find MSBuild
$MSBuild = "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe"
if (-not (Test-Path $MSBuild)) {
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

function Build-Plugin {
    param($Name, $SolutionPath, $VST3Name)

    Write-Host "`nBuilding $Name..." -ForegroundColor Cyan
    & $MSBuild $SolutionPath -p:Configuration=Release -p:Platform=x64 -v:minimal

    if ($LASTEXITCODE -ne 0) {
        Write-Error "$Name build failed"
        return $false
    }

    $VST3Path = [System.IO.Path]::GetDirectoryName($SolutionPath) + "\x64\Release\VST3\$VST3Name.vst3"
    Write-Host "$Name build successful: $VST3Path" -ForegroundColor Green

    if ($Deploy) {
        $Destination = "C:\Program Files\Common Files\VST3\$VST3Name.vst3"
        Write-Host "Deploying $Name to $Destination..." -ForegroundColor Cyan

        # Remove old version if exists
        if (Test-Path $Destination) {
            Remove-Item -Recurse -Force $Destination -ErrorAction SilentlyContinue
        }

        # Copy new version
        Copy-Item -Recurse -Force $VST3Path $Destination -ErrorAction Stop

        # Verify it actually worked
        if (Test-Path $Destination) {
            Write-Host "$Name deployed!" -ForegroundColor Green
        } else {
            Write-Error "$Name deploy FAILED - file not found at $Destination"
            Write-Host "Try running PowerShell as Administrator" -ForegroundColor Yellow
            return $false
        }
    }
    return $true
}

$success = $true

# Build Show Me MIDI plugin (fretboard visualizer)
if ($Plugin -eq "all" -or $Plugin -eq "midi") {
    $result = Build-Plugin "Show Me MIDI" "$ProjectRoot\Builds\VisualStudio2022\ShowMeMidi.sln" "ShowMeMidi"
    if (-not $result) { $success = $false }
}

# Build Show Me Audio plugin (pitch detector)
if ($Plugin -eq "all" -or $Plugin -eq "audio") {
    $result = Build-Plugin "Show Me Audio" "$ProjectRoot\Audio\Builds\VisualStudio2022\ShowMeAudio.sln" "ShowMeAudio"
    if (-not $result) { $success = $false }
}

if ($success) {
    Write-Host "`nAll builds successful!" -ForegroundColor Green
    if ($Deploy) {
        Write-Host "Restart FL Studio and rescan plugins." -ForegroundColor Yellow
    }
} else {
    Write-Error "Some builds failed"
    exit 1
}
