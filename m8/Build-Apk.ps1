param([string]$SdkRoot = "$env:LOCALAPPDATA/Android/Sdk",
      [string]$JavaHome = 'C:/Users/enaci/.jdks/jbr-21.0.11',
      [string]$GradleHome = 'C:/Users/enaci/.gradle/wrapper/dists/gradle-8.5-bin/5t9huq95ubn472n8rpzujfbqh/gradle-8.5')
$ErrorActionPreference = 'Stop'
$m8Root = $PSScriptRoot.Replace('\','/')
$env:JAVA_HOME = $JavaHome
$env:GRADLE_USER_HOME = "$m8Root/gradle-home"
$env:ANDROID_USER_HOME = "$m8Root/tool-home/.android"
$env:TEMP = "$m8Root/tmp"; $env:TMP = $env:TEMP
$env:JAVA_TOOL_OPTIONS = "-Duser.home=$m8Root/tool-home -Djava.io.tmpdir=$m8Root/tmp"
New-Item -ItemType Directory $env:TEMP,$env:ANDROID_USER_HOME,"$env:GRADLE_USER_HOME/caches" -Force | Out-Null
if (!(Test-Path "$m8Root/app/build/generated/engineJni/arm64-v8a/libOpenXWAM8.so")) { throw 'Build M8 native first' }
# Seed a PRIVATE dependency cache; never let Gradle update the shared baseline cache.
if (!(Test-Path "$env:GRADLE_USER_HOME/caches/modules-2")) {
    & robocopy "$env:USERPROFILE/.gradle/caches/modules-2" "$env:GRADLE_USER_HOME/caches/modules-2" /E /XF '*.lock' /NFL /NDL /NJH /NJS
    if ($LASTEXITCODE -ge 8) { throw 'Dependency cache copy failed' }
}
# Local copy of the existing debug key. No new shared signing state.
if (!(Test-Path "$env:ANDROID_USER_HOME/debug.keystore")) {
    Copy-Item -LiteralPath "$env:USERPROFILE/.android/debug.keystore" -Destination "$env:ANDROID_USER_HOME/debug.keystore"
}
& "$GradleHome/bin/gradle.bat" -p $m8Root --offline --no-daemon --console=plain assembleDebug 2>&1 | Tee-Object "$m8Root/evidence/apk-build.log"
if ($LASTEXITCODE) { throw 'M8 APK build failed' }
Get-FileHash -Algorithm SHA256 "$m8Root/app/build/outputs/apk/debug/app-debug.apk" | Format-List
