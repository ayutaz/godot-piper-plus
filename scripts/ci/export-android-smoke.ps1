param(
    [string]$AddonDir = "addons/piper_plus"
)

$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$ProjectDir = Join-Path $RepoRoot "test\project"
$Godot = if ($env:GODOT) { $env:GODOT } else { Join-Path $env:LOCALAPPDATA "Microsoft\WinGet\Links\godot_console.exe" }
$SettingsDir = Join-Path $env:APPDATA "Godot"
$AndroidSdk = if ($env:ANDROID_SDK_ROOT) { $env:ANDROID_SDK_ROOT } elseif ($env:ANDROID_HOME) { $env:ANDROID_HOME } else { Join-Path $env:LOCALAPPDATA "Android\Sdk" }
$JavaHome = if ($env:JAVA_HOME) { $env:JAVA_HOME } else { "" }
$KeystoreSource = if ($env:GODOT_ANDROID_KEYSTORE_DEBUG_PATH) { $env:GODOT_ANDROID_KEYSTORE_DEBUG_PATH } else { Join-Path $env:USERPROFILE ".android\debug.keystore" }
$KeystoreTarget = Join-Path $ProjectDir "android-debug.keystore"
$ExportPath = Join-Path $ProjectDir "build\android\piper-plus-tests.apk"
$AddonBinDir = Join-Path $RepoRoot $AddonDir "bin"
$ProjectAddonBinDir = Join-Path $ProjectDir "addons\piper_plus\bin"
$GodotVersionLine = & $Godot --version
$GodotMinorVersion = ([regex]::Match($GodotVersionLine, '^\d+\.\d+')).Value
$SettingsPaths = @(
    Join-Path $SettingsDir "editor_settings-4.tres"
)
if ($GodotMinorVersion) {
    $SettingsPaths += Join-Path $SettingsDir ("editor_settings-{0}.tres" -f $GodotMinorVersion)
}

if (-not (Test-Path $Godot)) {
    throw "Godot executable not found: $Godot"
}

if (-not (Test-Path (Join-Path $RepoRoot $AddonDir))) {
    throw "Addon directory not found: $AddonDir"
}

if (-not (Test-Path $AndroidSdk)) {
    throw "Android SDK directory not found: $AndroidSdk"
}

if (-not (Test-Path $KeystoreSource)) {
    throw "Debug keystore not found: $KeystoreSource"
}

New-Item -ItemType Directory -Force -Path (Join-Path $RepoRoot ".ci"), (Split-Path $ExportPath), $ProjectAddonBinDir, $SettingsDir | Out-Null
$SettingsBackups = @{}
foreach ($path in $SettingsPaths | Select-Object -Unique) {
    $backup = Join-Path $RepoRoot (".ci\{0}.backup" -f (Split-Path $path -Leaf))
    if (Test-Path $path) {
        Copy-Item $path $backup -Force
    } else {
        Set-Content -Path $path -Value "" -Encoding utf8
        Copy-Item $path $backup -Force
    }
    $SettingsBackups[$path] = $backup
}

try {
    foreach ($SettingsPath in $SettingsPaths | Select-Object -Unique) {
        $content = Get-Content $SettingsPath -Raw
        $replacements = [ordered]@{
            "export/android/android_sdk_path" = $AndroidSdk
            "export/android/debug_keystore" = $KeystoreTarget
            "export/android/debug_keystore_user" = "androiddebugkey"
            "export/android/debug_keystore_pass" = "android"
        }

        if ($JavaHome) {
            $replacements["export/android/java_sdk_path"] = $JavaHome
        }

        foreach ($key in $replacements.Keys) {
            $value = $replacements[$key].Replace("\", "/")
            $escaped = [regex]::Escape($key)
            if ($content -match "(?m)^$escaped = ") {
                $content = [regex]::Replace($content, "(?m)^$escaped = .*?$", "$key = `"$value`"")
            } else {
                $content += "`n$key = `"$value`""
            }
        }

        Set-Content -Path $SettingsPath -Value $content -Encoding utf8
    }

    Copy-Item $KeystoreSource $KeystoreTarget -Force

    $OrtAlias = Join-Path $AddonBinDir "libonnxruntime.android.arm64.so"
    if (-not (Test-Path $OrtAlias)) {
        $OrtSource = Get-ChildItem -Path (Join-Path $RepoRoot "build-*"), (Join-Path $RepoRoot "build-android-debug"), (Join-Path $RepoRoot "build-android-release") -Recurse -Filter libonnxruntime.so -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($OrtSource) {
            Copy-Item $OrtSource.FullName $OrtAlias -Force
        }
    }

    bash test/prepare-assets.sh

    if (Test-Path $ExportPath) {
        Remove-Item $ExportPath -Force
    }

    & $Godot --headless --path $ProjectDir --export-debug Android $ExportPath

    if (-not (Test-Path $ExportPath)) {
        throw "Android export did not produce $ExportPath"
    }

    $listing = tar -tf $ExportPath
    $listing | Select-String -Pattern "piper_plus|onnxruntime"
}
finally {
    foreach ($SettingsPath in $SettingsBackups.Keys) {
        $backup = $SettingsBackups[$SettingsPath]
        if (Test-Path $backup) {
            Copy-Item $backup $SettingsPath -Force
        }
    }
}
