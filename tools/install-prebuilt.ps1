# HERO VR hotfix 2: installs the release's ready-made mod files (build\windows-ninja, python, docs\licenses)
# into this folder, so players never compile. tools\build.ps1 runs this when a launcher runs it.
# Exit 0 = installed, 3 = this release has no ready-made mod files, 1 = failed (plain message on stderr).
# Windows PowerShell 5.1 compatible. Writes only through [Console] (no CLIXML for older launchers).
$ErrorActionPreference = 'Stop'
function Say([string]$t) { [Console]::Out.WriteLine($t) }
function Fail([string]$t) { [Console]::Error.WriteLine($t); exit 1 }
$root = Split-Path -Parent $PSScriptRoot
$tmp = Join-Path $root '.prebuilt-tmp'
try {
    $verFile = Join-Path $root 'VERSION'
    if (-not (Test-Path -LiteralPath $verFile)) { Fail 'This folder has no VERSION file, so the launcher cannot tell which mod files it needs. Unzip the HERO VR zip again into a new folder.' }
    $verLine = (Get-Content -LiteralPath $verFile -TotalCount 1).Trim()
    $m = [regex]::Match($verLine, '(\d+(\.\d+)+)')
    if (-not $m.Success) { Fail "VERSION reads '$verLine', not a version number. Unzip the HERO VR zip again." }
    $tag = 'v' + $m.Groups[1].Value

    # Where version.json is: SM2VR_PREBUILT_MANIFEST, else launcher.ini PrebuiltManifestUrl, else this version's release.
    $url = $env:SM2VR_PREBUILT_MANIFEST
    $repo = 'lecutis7of9/HEROVR'
    $ini = Join-Path $root 'tools\launcher\launcher.ini'
    if (Test-Path -LiteralPath $ini) {
        foreach ($l in Get-Content -LiteralPath $ini) {
            $kv = $l -split '=', 2
            if ($kv.Count -ne 2) { continue }
            $k = $kv[0].Trim(); $v = $kv[1].Trim()
            if ($k -eq 'PrebuiltManifestUrl' -and $v -and -not $url) { $url = $v }
            if ($k -eq 'UpdateRepo' -and $v) { $repo = $v.Trim('/') }
        }
    }
    if (-not $url) { $url = "https://github.com/$repo/releases/download/$tag/version.json" }
    if (-not ($url -like 'https://*' -or $url -like 'http://127.0.0.1*')) { Fail "The mod files address must be https: $url" }

    [Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
    $wc = New-Object Net.WebClient
    $wc.Headers['User-Agent'] = 'HERO-VR-install-prebuilt'
    Say "Installing the ready-made mod files for $verLine (no build needed)."
    Say "Reading $url"
    try { $json = $wc.DownloadString($url) }
    catch {
        $resp = $_.Exception.InnerException.Response
        if (-not $resp) { $resp = $_.Exception.Response }
        if ($resp -and [int]$resp.StatusCode -eq 404) { Say 'This release has no ready-made mod files.'; exit 3 }
        Fail "Could not download the mod files list. Check the internet connection and press Repair in the launcher. ($($_.Exception.Message))"
    }
    $man = $json | ConvertFrom-Json
    $file = @($man.files | Where-Object { $_.kind -eq 'prebuilt' }) | Select-Object -First 1
    if (-not $file) { Say 'This release has no ready-made mod files.'; exit 3 }
    if (-not ($file.url -like 'https://*' -or $file.url -like 'http://127.0.0.1*')) { Fail "The mod files address must be https: $($file.url)" }

    if (Test-Path -LiteralPath $tmp) { Remove-Item -LiteralPath $tmp -Recurse -Force }
    New-Item -ItemType Directory -Path $tmp | Out-Null
    $zip = Join-Path $tmp $file.name
    Say "Downloading $($file.name) ($([math]::Round($file.size / 1MB, 1)) MB)..."
    try { $wc.DownloadFile($file.url, $zip) }
    catch { Fail "Could not download the mod files. Check the internet connection and press Repair in the launcher. ($($_.Exception.Message))" }
    $sha = (Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($file.sha256 -and $sha -ne ([string]$file.sha256).ToLowerInvariant()) { Fail "The downloaded mod files are damaged (checksum mismatch). Press Repair in the launcher to download them again. Nothing was changed." }
    Say 'Checksum OK.'

    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $out = Join-Path $tmp 'x'
    [IO.Compression.ZipFile]::ExtractToDirectory($zip, $out)
    $top = $out
    if (-not (Test-Path -LiteralPath (Join-Path $out 'build'))) {
        $dirs = @(Get-ChildItem -LiteralPath $out -Directory)
        if ($dirs.Count -eq 1) { $top = $dirs[0].FullName }
    }
    $dll = Join-Path $top 'build\windows-ninja\spidy_stereo_probe.dll'
    if (-not (Test-Path -LiteralPath $dll)) { Fail 'The downloaded file is not the HERO VR mod files (no build\windows-ninja\spidy_stereo_probe.dll). Nothing was changed.' }
    $pvHave = Join-Path $root 'PORT-VERSION'; $pvGot = Join-Path $top 'PORT-VERSION'
    if ((Test-Path -LiteralPath $pvHave) -and (Test-Path -LiteralPath $pvGot)) {
        $a = (Get-Content -LiteralPath $pvHave -TotalCount 1).Trim(); $b = (Get-Content -LiteralPath $pvGot -TotalCount 1).Trim()
        if ($a -ne $b) { Fail "The downloaded mod files are for $b, but this folder has $a. Nothing was changed." }
    }

    $build = Join-Path $root 'build'
    if (-not (Test-Path -LiteralPath $build)) { New-Item -ItemType Directory -Path $build | Out-Null }
    $dest = Join-Path $build 'windows-ninja'
    if (Test-Path -LiteralPath $dest) { Remove-Item -LiteralPath $dest -Recurse -Force }
    Move-Item -LiteralPath (Join-Path $top 'build\windows-ninja') -Destination $dest
    $py = Join-Path $top 'python'
    if (Test-Path -LiteralPath $py) {
        $pyDest = Join-Path $root 'python'
        if (Test-Path -LiteralPath $pyDest) { Remove-Item -LiteralPath $pyDest -Recurse -Force }
        Move-Item -LiteralPath $py -Destination $pyDest
    }
    $lic = Join-Path $top 'docs\licenses'
    if (Test-Path -LiteralPath $lic) {
        $licDest = Join-Path $root 'docs\licenses'
        if (-not (Test-Path -LiteralPath $licDest)) { New-Item -ItemType Directory -Path $licDest -Force | Out-Null }
        Copy-Item -Path (Join-Path $lic '*') -Destination $licDest -Recurse -Force
    }
    $now = Get-Date
    Get-ChildItem -LiteralPath $dest -File | ForEach-Object { $_.LastWriteTime = $now }
    $port = if (Test-Path -LiteralPath $pvHave) { (Get-Content -LiteralPath $pvHave -TotalCount 1).Trim() } else { $verLine }
    [IO.File]::WriteAllText((Join-Path $dest 'HERO-VR-MOD-FILES.txt'), "Ready-made HERO VR mod files for $port`r`nPackage=$($file.name)`r`nSHA256=$sha`r`nInstalled=$($now.ToString('o'))`r`n")
    Say "Mod files installed: $port."
    exit 0
}
catch { Fail "Installing the mod files failed: $($_.Exception.Message). Press Repair in the launcher to try again." }
finally { if (Test-Path -LiteralPath $tmp) { Remove-Item -LiteralPath $tmp -Recurse -Force -ErrorAction SilentlyContinue } }
