param(
    [Parameter(Mandatory = $true)]
    [string]$ExtensionRoot,

    [string[]]$Abis = @("arm64-v8a", "armeabi-v7a", "x86_64"),

    [string]$BuildType = "Release"
)

$ErrorActionPreference = "Stop"

function Get-LatestChildDir([string]$Path) {
    if (!(Test-Path $Path)) {
        throw "Path not found: $Path"
    }
    $child = Get-ChildItem -Path $Path -Directory | Sort-Object Name -Descending | Select-Object -First 1
    if ($null -eq $child) {
        throw "No child directories found under: $Path"
    }
    return $child.FullName
}

function Resolve-AndroidSdkRoot {
    if ($env:ANDROID_SDK_ROOT) { return $env:ANDROID_SDK_ROOT }
    if ($env:ANDROID_HOME) { return $env:ANDROID_HOME }
    $defaultSdk = Join-Path $env:LOCALAPPDATA "Android\Sdk"
    if (Test-Path $defaultSdk) { return $defaultSdk }
    throw "Android SDK not found. Set ANDROID_SDK_ROOT or ANDROID_HOME."
}

$ExtensionRoot = (Resolve-Path $ExtensionRoot).Path
$ManifestPath = Join-Path $ExtensionRoot "manifest.json"
$NativeDir = Join-Path $ExtensionRoot "native"

if (!(Test-Path $ManifestPath)) {
    throw "manifest.json not found in $ExtensionRoot"
}
if (!(Test-Path (Join-Path $NativeDir "CMakeLists.txt"))) {
    throw "native/CMakeLists.txt not found in $ExtensionRoot"
}

$Manifest = Get-Content -Raw $ManifestPath | ConvertFrom-Json
$LibraryName = $Manifest.native_runtime.library_name
if ([string]::IsNullOrWhiteSpace($LibraryName)) {
    throw "manifest.json is missing native_runtime.library_name"
}

$SdkRoot = Resolve-AndroidSdkRoot
$NdkRoot = Get-LatestChildDir (Join-Path $SdkRoot "ndk")
$CmakeRoot = Get-LatestChildDir (Join-Path $SdkRoot "cmake")
$CmakeExe = Join-Path $CmakeRoot "bin\cmake.exe"
$NinjaExe = Join-Path $CmakeRoot "bin\ninja.exe"
$Toolchain = Join-Path $NdkRoot "build\cmake\android.toolchain.cmake"
$ReadElfExe = Join-Path $NdkRoot "toolchains\llvm\prebuilt\windows-x86_64\bin\llvm-readelf.exe"

if (!(Test-Path $CmakeExe)) { throw "cmake.exe not found: $CmakeExe" }
if (!(Test-Path $NinjaExe)) { throw "ninja.exe not found: $NinjaExe" }
if (!(Test-Path $Toolchain)) { throw "Android toolchain file not found: $Toolchain" }
if (!(Test-Path $ReadElfExe)) { throw "llvm-readelf.exe not found: $ReadElfExe" }

$Deps = @()
if ($Manifest.native_runtime -and $Manifest.native_runtime.dependencies) {
    $Deps = @($Manifest.native_runtime.dependencies)
}
$NeedsSharedStl = $Deps -contains "libc++_shared.so"
$AndroidStl = if ($NeedsSharedStl) { "c++_shared" } else { "c++_static" }

$BuildRoot = Join-Path $ExtensionRoot "build"
$AdapterRoot = Join-Path $ExtensionRoot "runtime_bundle\adapters"
$JniLibRoot = Join-Path $ExtensionRoot "runtime_bundle\jniLibs"
New-Item -ItemType Directory -Force $BuildRoot, $AdapterRoot | Out-Null
if ($NeedsSharedStl) {
    New-Item -ItemType Directory -Force $JniLibRoot | Out-Null
}

$AbiLibDirMap = @{
    "arm64-v8a" = "aarch64-linux-android"
    "x86_64" = "x86_64-linux-android"
    "armeabi-v7a" = "arm-linux-androideabi"
    "x86" = "i686-linux-android"
}

foreach ($Abi in $Abis) {
    $AbiBuildDir = Join-Path $BuildRoot $Abi
    $AbiOutDir = Join-Path $AdapterRoot $Abi
    New-Item -ItemType Directory -Force $AbiBuildDir, $AbiOutDir | Out-Null
    if ($NeedsSharedStl) {
        $AbiJniOutDir = Join-Path $JniLibRoot $Abi
        New-Item -ItemType Directory -Force $AbiJniOutDir | Out-Null
    }

    $ConfigureArgs = @(
        "-S", $NativeDir,
        "-B", $AbiBuildDir,
        "-DCMAKE_TOOLCHAIN_FILE=$Toolchain",
        "-DANDROID_ABI=$Abi",
        "-DANDROID_PLATFORM=android-24",
        "-DANDROID_STL=$AndroidStl",
        "-DCMAKE_SHARED_LINKER_FLAGS=-Wl,-z,max-page-size=16384",
        "-DCMAKE_BUILD_TYPE=$BuildType",
        "-G", "Ninja",
        "-DCMAKE_MAKE_PROGRAM=$NinjaExe"
    )
    & $CmakeExe @ConfigureArgs
    if ($LASTEXITCODE -ne 0) {
        throw "CMake configure failed for $Abi"
    }

    & $CmakeExe --build $AbiBuildDir --parallel
    if ($LASTEXITCODE -ne 0) {
        throw "CMake build failed for $Abi"
    }

    $BuiltLibrary = Get-ChildItem -Path $AbiBuildDir -Filter "*.so" -Recurse |
        Where-Object { $_.Name -eq $LibraryName } |
        Select-Object -First 1

    if ($null -eq $BuiltLibrary) {
        throw "Could not find $LibraryName in $AbiBuildDir"
    }

    $DynamicSymbols = (& $ReadElfExe --dyn-syms $BuiltLibrary.FullName | Out-String)
    $RequiredExports = @(
        "instalink_extension_describe",
        "instalink_extension_invoke",
        "instalink_extension_free_string"
    )
    $MissingExports = $RequiredExports | Where-Object { $DynamicSymbols -notmatch [regex]::Escape($_) }
    if ($MissingExports.Count -gt 0) {
        throw "Native adapter is missing required exported ABI symbols for ${Abi}: $($MissingExports -join ', '). Ensure the adapter uses INSTALINK_EXTENSION_EXPORT or visibility(default) on InstaLink ABI functions."
    }

    Copy-Item -Force $BuiltLibrary.FullName (Join-Path $AbiOutDir $LibraryName)

    if ($NeedsSharedStl) {
        $AbiLibDirName = $AbiLibDirMap[$Abi]
        if (![string]::IsNullOrWhiteSpace($AbiLibDirName)) {
            $SharedStlPath = Join-Path $NdkRoot "toolchains\llvm\prebuilt\windows-x86_64\sysroot\usr\lib\$AbiLibDirName\libc++_shared.so"
            if (Test-Path $SharedStlPath) {
                Copy-Item -Force $SharedStlPath (Join-Path $AbiJniOutDir "libc++_shared.so")
                Write-Host "Copied libc++_shared.so for $Abi"
            }
        }
    }

    Write-Host "Built $LibraryName for $Abi ($AndroidStl)"
}
