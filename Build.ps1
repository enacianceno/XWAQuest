param([string]$JdkHome = $env:JAVA_HOME)
$ErrorActionPreference = 'Stop'
if (!$JdkHome) { throw 'Set JAVA_HOME to JDK 21 or supply -JdkHome.' }
$java = Join-Path $JdkHome 'bin/java.exe'
$version = (& $java -version 2>&1 | Out-String)
if ($version -notmatch 'version "21\.') { throw "M1 requires JDK 21. Found: $version" }
$env:JAVA_HOME = $JdkHome
Push-Location $PSScriptRoot
try {
    & .\gradlew.bat :app:assembleDebug --console=plain
    if ($LASTEXITCODE -ne 0) { throw "Gradle failed: $LASTEXITCODE" }
    Get-FileHash app/build/outputs/apk/debug/app-debug.apk -Algorithm SHA256
} finally { Pop-Location }
