# Builds the shareable SM2 VR package, dist\SM2-VR-<PORT-VERSION>-win64.zip (Update 22: SM2-VR-Update-22-win64.zip),
# from a built checkout (tools\build.ps1 -Observer). It holds the game modules, the Python tools a VR session
# imports (worked out from the code by tools\package_manifest.py, so a new import cannot be forgotten), an
# embeddable Python 3.12 of its own (hash-pinned), the starter, an empty Music folder, PORT-VERSION, SHARING.md
# as README.txt and the notices. Friends unzip it anywhere they can write (spaces in the path are fine) and
# double-click "Launch Spidy VR.cmd": no Python to install, and nothing outside the folder is used for Python.
# Update 24: unzip-and-play. Every module has Microsoft's C++ runtime built in (/MT), so there is no Visual C++
# Redistributable to install; "Launch Spidy VR.cmd" is the only starter in the folder's top (Spidy Launcher.exe
# is in tools\); and the self-test checks all of that on the staged folder.
# Never packed: build intermediates, .git, reports\, Music\ songs, live.txt or next-launch.txt (personal settings;
# the defaults live in the code), sources.
param(
    [switch]$KeepFolder,   # keep the staged folder (build\package\...) to look through
    [string]$Name          # the zip's and its folder's name; default SM2-VR-<PORT-VERSION>-win64 (release.yml passes its own)
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$bin = Join-Path $root 'build\windows-ninja'
$versionFile = Join-Path $root 'PORT-VERSION'
if (-not (Test-Path -LiteralPath $versionFile)) { throw 'PORT-VERSION is missing.' }
$portVersion = (Get-Content -LiteralPath $versionFile -TotalCount 1).Trim()
if (-not $Name) { $Name = 'SM2-VR-' + ($portVersion -replace '[^A-Za-z0-9.]+', '-').Trim('-') }
$Name = $Name -replace '-win64$', ''

# Python 3.12.10 is the newest 3.12 with Windows binaries; its hash matches python.org's release.
$pythonVersion = '3.12.10'
$pythonHash = '4ACBED6DD1C744B0376E3B1CF57CE906F9DC9E95E68824584C8099A63025A3C3'
$cache = Join-Path $root 'build\package-cache'
$pythonZip = Join-Path $cache "python-$pythonVersion-embed-amd64.zip"
if (-not (Test-Path -LiteralPath $pythonZip)) {
    New-Item -ItemType Directory -Force $cache | Out-Null
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    Invoke-WebRequest -UseBasicParsing "https://www.python.org/ftp/python/$pythonVersion/python-$pythonVersion-embed-amd64.zip" -OutFile $pythonZip
}
if ((Get-FileHash -LiteralPath $pythonZip -Algorithm SHA256).Hash -ne $pythonHash) {
    throw "The Python download does not match the pinned hash. Delete $pythonZip and try again."
}

# Staged under build\ (never dist\, where people unzip packages), in a folder whose path has spaces, so the
# self-test below also proves the package runs from such a path.
$stageParent = Join-Path $root 'build\package\space test'
$stage = Join-Path $stageParent $Name
if (Test-Path -LiteralPath $stageParent) { Remove-Item -LiteralPath $stageParent -Recurse -Force }
foreach ($folder in @('build\windows-ninja', 'tools', 'python', 'docs\licenses', 'Music')) {
    New-Item -ItemType Directory -Force (Join-Path $stage $folder) | Out-Null
}

# The embeddable Python first: it works out what the package needs.
Expand-Archive -LiteralPath $pythonZip -DestinationPath (Join-Path $stage 'python')
# The embeddable Python ignores the script's folder and PYTHONPATH; the tools import each other.
Add-Content -LiteralPath (Join-Path $stage 'python\python312._pth') -Value '..\tools' -Encoding ascii
$python = Join-Path $stage 'python\python.exe'
# Update 26: the package's Python is exactly the pinned one (the starter uses it and nothing else).
$stagedVersion = "$(& $python -c "import sys; print('%d.%d.%d' % sys.version_info[:3])")".Trim()
if ($LASTEXITCODE -or $stagedVersion -ne $pythonVersion) { throw "The packaged Python is $stagedVersion, not $pythonVersion." }
$manifestTool = Join-Path $root 'tools\package_manifest.py'
$listing = & $python -B -X utf8 $manifestTool list --tools (Join-Path $root 'tools')
if ($LASTEXITCODE) { throw "tools\package_manifest.py list failed: $listing" }
$manifest = ($listing -join "`n") | ConvertFrom-Json
if (@($manifest.missing_tools).Count) { throw "A tool the session imports is missing: $($manifest.missing_tools -join ', ')." }

# The modules: every one the tools and launch-game-vr.ps1 name (all built by build.ps1 -Observer), and the
# haptics test. bHaptics' own library ships as it comes (a bHaptics Redistributable).
$modules = @($manifest.binaries) + @('spidy_haptics_test.exe') | Sort-Object -Unique
$missing = @($modules | Where-Object { -not (Test-Path -LiteralPath (Join-Path $bin $_)) })
if ($missing) {
    throw ("Missing in build\windows-ninja: $($missing -join ', '). Build first: " +
           'powershell -ExecutionPolicy Bypass -File .\tools\build.ps1 -Observer')
}
foreach ($module in $modules) { Copy-Item -LiteralPath (Join-Path $bin $module) -Destination (Join-Path $stage 'build\windows-ninja') }
# Spidy Launcher.exe: optional (the starter does not need it), in tools\ so "Launch Spidy VR.cmd" is the one file
# to start at the folder's top. It finds the package from there (it looks up for tools\run_game_vr.py).
$launcher = Join-Path $bin 'spidy_launcher.exe'
if (Test-Path -LiteralPath $launcher) { Copy-Item -LiteralPath $launcher -Destination (Join-Path $stage 'tools\Spidy Launcher.exe') }
else { Write-Warning 'build\windows-ninja\spidy_launcher.exe is missing: the package has no Spidy Launcher.exe (the starter still works).' }

foreach ($tool in @($manifest.tools)) { Copy-Item -LiteralPath (Join-Path $root "tools\$tool.py") -Destination (Join-Path $stage 'tools') }
foreach ($script in @('launch-game-vr.ps1', 'package_manifest.py')) {
    Copy-Item -LiteralPath (Join-Path $root "tools\$script") -Destination (Join-Path $stage 'tools')
}
# The starter, with Windows line endings whatever this checkout has (cmd.exe misreads some others). Update 24:
# only this one (the checkout's other .cmd files are for development).
foreach ($starter in @('Launch Spidy VR.cmd')) {
    $text = [IO.File]::ReadAllText((Join-Path $root $starter)) -replace "`r?`n", "`r`n"
    [IO.File]::WriteAllText((Join-Path $stage $starter), $text, (New-Object Text.ASCIIEncoding))
}
Copy-Item -LiteralPath $versionFile -Destination $stage
Copy-Item -LiteralPath (Join-Path $root 'SHARING.md') -Destination (Join-Path $stage 'README.txt')
# Zip tools skip empty folders, and a note says what the folder is for (only songs are played from it).
Set-Content -LiteralPath (Join-Path $stage 'Music\Put your songs here.txt') -Encoding ascii -Value @(
    'Put your songs here (mp3, wav, flac, m4a, aac or wma), then pick them on the MUSIC page of the hologram.'
    'Turn Music down to 0 in the game''s Audio settings so it does not play over your songs.')
Copy-Item -LiteralPath (Join-Path $root 'LICENSE') -Destination (Join-Path $stage 'LICENSE.txt')
Copy-Item -LiteralPath (Join-Path $root 'THIRD_PARTY_NOTICES.md') -Destination $stage
Copy-Item -LiteralPath (Join-Path $root 'CREDITS.md') -Destination $stage  # Version 0.0.6: who made it, and SpidyVR's author
Copy-Item -Path (Join-Path $root 'docs\licenses\*') -Destination (Join-Path $stage 'docs\licenses')
$notices = @{
    'third_party\imgui\LICENSE.txt' = 'DearImGui-LICENSE.txt'
    # The bHaptics SDK agreement says its notices travel with the library.
    'third_party\tact-cpp2\sdk\THIRD_PARTY_NOTICES.txt' = 'bHaptics-THIRD_PARTY_NOTICES.txt'
    'third_party\tact-cpp2\LICENSE' = 'bHaptics-LICENSE.txt'
}
foreach ($from in $notices.Keys) {
    $source = Join-Path $root $from
    if (-not (Test-Path -LiteralPath $source)) { throw "$from is missing. Run tools\bootstrap.ps1 -Observer first." }
    Copy-Item -LiteralPath $source -Destination (Join-Path $stage "docs\licenses\$($notices[$from])")
}

# Self-test, with the packaged Python on the packaged tools only, from a path with spaces: every tool a session
# imports is there, every other import resolves in the embeddable Python, the entry scripts import, and
# run_game_vr.py starts (--help: it touches nothing).
$check = & $python -B -X utf8 (Join-Path $stage 'tools\package_manifest.py') check --tools (Join-Path $stage 'tools')
$check | ForEach-Object { Write-Host "  self-test: $_" }
if ($LASTEXITCODE -or @($check)[-1] -ne 'ok') { throw 'The package self-test failed (the lines above say what is missing).' }
$help = & $python -B -X utf8 (Join-Path $stage 'tools\run_game_vr.py') --help
if ($LASTEXITCODE -or -not ("$help" -match '--overlay-webs')) { throw 'The packaged Python cannot start tools\run_game_vr.py.' }
# Update 24, unzip-and-play:
# no module needs Microsoft's Visual C++ Redistributable (all /MT; bHaptics' library uses the game's own),
$runtime = & $python -B -X utf8 (Join-Path $stage 'tools\player_check.py') runtime (Join-Path $stage 'build\windows-ninja')
$runtime | ForEach-Object { Write-Host "  self-test: $_" }
if ($LASTEXITCODE -or @($runtime)[-1] -ne 'ok') {
    throw 'A module still needs the Visual C++ Redistributable (above). Delete build and build again: powershell -ExecutionPolicy Bypass -File .\tools\build.ps1 -Observer'
}
# the staged folder passes the folder checks a player gets (writable; Program Files, OneDrive, long path warned),
$folder = & $python -B -X utf8 (Join-Path $stage 'tools\player_check.py') folder $stage
$folder | ForEach-Object { Write-Host "  self-test: $_" }
if ($LASTEXITCODE -or @($folder)[-1] -ne 'ok') { throw 'The staged package folder fails the folder checks (above).' }
# "Launch Spidy VR.cmd" is the only thing to start at the top, the README says so first,
$starters = @(Get-ChildItem -LiteralPath $stage -File | Where-Object { $_.Extension -in @('.cmd', '.bat', '.exe', '.ps1', '.lnk') })
if ($starters.Count -ne 1 -or $starters[0].Name -ne 'Launch Spidy VR.cmd') {
    throw "The package's top must hold only Launch Spidy VR.cmd to start; it has: $($starters.Name -join ', ')."
}
$readme = (Get-Content -LiteralPath (Join-Path $stage 'README.txt') -TotalCount 12) -join ' '
if ($readme -notmatch [regex]::Escape('Unzip, start your VR app, double-click Launch Spidy VR.cmd')) {
    throw 'README.txt does not start with "Unzip, start your VR app, double-click Launch Spidy VR.cmd".'
}
if ($readme -match 'Visual C\+\+ Redistributable') { throw 'README.txt still asks for the Visual C++ Redistributable.' }
# and the starter's script parses, unblocks the downloaded files and starts with the overlay webs by default.
$scriptPath = Join-Path $stage 'tools\launch-game-vr.ps1'
$tokens = $null; $parseErrors = $null
[void][System.Management.Automation.Language.Parser]::ParseFile($scriptPath, [ref]$tokens, [ref]$parseErrors)
if ($parseErrors) { throw "tools\launch-game-vr.ps1 does not parse: $($parseErrors[0].Message)" }
$script = [IO.File]::ReadAllText($scriptPath)
# Update 26: and it runs only the package's own Python, saying so plainly when that is missing.
foreach ($needed in @('Unblock-File', "`$captureArgs+=@('--overlay-webs')", '[int]$RenderScale=100', '[int]$Supersample=100',
                      "`$Python=`$bundledPython", "This package's own Python is missing")) {
    if (-not $script.Contains($needed)) { throw "tools\launch-game-vr.ps1 lacks: $needed" }
}
# run_game_vr.py makes a reports folder (and Python may make caches) when it starts; none of that ships.
foreach ($made in @('reports', 'tools\__pycache__')) {
    $path = Join-Path $stage $made
    if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path -Recurse -Force }
}
$leftovers = @(Get-ChildItem -LiteralPath $stage -Recurse -Force | Where-Object {
    $_.Name -in @('live.txt', 'next-launch.txt', '.git', 'reports', 'CMakeLists.txt') -or $_.Extension -in @('.obj', '.pdb', '.ilk', '.lib', '.exp', '.pyc') })
if ($leftovers) { throw "The package holds files it must not: $($leftovers.FullName -join ', ')." }

# The zip, with its folder at the top. ZipFile writes forward slashes and keeps empty folders, unlike
# Windows PowerShell's Compress-Archive.
$dist = Join-Path $root 'dist'
New-Item -ItemType Directory -Force $dist | Out-Null
$zip = Join-Path $dist "$Name-win64.zip"
if (Test-Path -LiteralPath $zip) { Remove-Item -LiteralPath $zip -Force }
Add-Type -AssemblyName System.IO.Compression.FileSystem
[IO.Compression.ZipFile]::CreateFromDirectory($stage, $zip, [IO.Compression.CompressionLevel]::Optimal, $true)
if (-not $KeepFolder) { Remove-Item -LiteralPath $stageParent -Recurse -Force }
$size = (Get-Item -LiteralPath $zip).Length / 1MB
$hash = (Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash
Write-Output ("SM2 VR {0}: {1} ({2:N1} MB)" -f $portVersion, $zip, $size)
Write-Output "SHA-256: $hash"
