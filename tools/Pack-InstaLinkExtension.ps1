param(
    [Parameter(Mandatory = $true)]
    [string]$ExtensionRoot,

    [string]$OutputZip = ""
)

$ErrorActionPreference = "Stop"

$ExtensionRoot = (Resolve-Path $ExtensionRoot).Path
$ManifestPath = Join-Path $ExtensionRoot "manifest.json"
if (!(Test-Path $ManifestPath)) {
    throw "manifest.json not found in $ExtensionRoot"
}

$Manifest = Get-Content -Raw $ManifestPath | ConvertFrom-Json

if ($Manifest.runtime_mode -eq "native" -or $Manifest.runtime_mode -eq "hybrid") {
    $RuntimeBundle = Join-Path $ExtensionRoot "runtime_bundle"
    if (!(Test-Path $RuntimeBundle)) {
        throw "Error: 'runtime_bundle' folder not found. For native extensions, you must compile the C++ code successfully before packing. The extension would be empty otherwise."
    }
}
if ([string]::IsNullOrWhiteSpace($OutputZip)) {
    $DistDir = Join-Path $ExtensionRoot "dist"
    New-Item -ItemType Directory -Force $DistDir | Out-Null
    $OutputZip = Join-Path $DistDir ("{0}-{1}.ins" -f $Manifest.slug, $Manifest.version)
}

$StageDir = Join-Path $env:TEMP ("instalink_ext_" + [Guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Force $StageDir | Out-Null

$ExcludedNames = @("native", "build", "dist", ".git", ".gradle", ".idea")

try {
    Get-ChildItem -Force $ExtensionRoot | Where-Object {
        $ExcludedNames -notcontains $_.Name
    } | ForEach-Object {
        Copy-Item -Force -Recurse $_.FullName (Join-Path $StageDir $_.Name)
    }

    if (Test-Path $OutputZip) {
        Remove-Item -Force $OutputZip
    }

    Compress-Archive -Path (Join-Path $StageDir "*") -DestinationPath $OutputZip -Force
    Write-Host "Packed extension to $OutputZip"
}
finally {
    if (Test-Path $StageDir) {
        Remove-Item -Force -Recurse $StageDir
    }
}
