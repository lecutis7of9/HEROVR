param(
    [ValidateRange(0,25)][int]$Seconds=0,
    [ValidateRange(0,8192)][int]$Size=0,
    [ValidateRange(50,200)][int]$RenderScale=100,
    [ValidateRange(50,100)][int]$Upscale=100,
    [ValidatePattern('^(game|[+-]?\d(\.\d{1,2})?)$')][string]$MipBias='0.5',
    [ValidateRange(0,16)][int]$Anisotropy=16,
    [ValidateSet('off','fxaa')][string]$EyeAA='fxaa',
    [ValidateRange(100,200)][int]$Supersample=100,
    [ValidateRange(0,240)][int]$MonitorRate=30,
    [ValidateRange(50,400)][int]$BuildingReach=150,
    [ValidateRange(1,65)][float]$SwingSpeed=32,
    [ValidateRange(40,300)][int]$Weight=80,
    [ValidateRange(90,480)][int]$FlipSpeed=180,
    [ValidateRange(0,90)][int]$SnapTurn=30,
    [ValidateRange(0,360)][int]$SmoothTurn=0,
    [ValidateRange(0,100)][int]$Haptics=100,
    [ValidateSet('Small','Medium','Large')][string]$ScreenSize='Medium',
    [ValidateSet('Off','Small','Medium','Large')][string]$Hud='Medium',
    [ValidateSet('Off','Low','Medium','High')][string]$ScreenDepth='Medium',
    [ValidateRange(0,2500)][int]$EyeHeight=0,
    [ValidateRange(0,1200)][int]$ArmLength=0,
    [string]$Python,
    [switch]$CaptureImages,
    [switch]$OverlayWebs,  # Update 24: the default; kept so older command lines still work
    [switch]$GameWebs,     # Update 24: the game's own web lines instead of Spidy's overlay strands
    [switch]$NoWebGrab,
    [switch]$NoAirWebs,
    [switch]$NoBody,
    [switch]$NoPunch,
    [switch]$NoWebShooter,
    [switch]$NoAimMarkers,
    [switch]$NoPromptHelp,
    [switch]$NoCalibrationPrompt,
    [switch]$Flips,
    [switch]$TriggerWebs,
    [switch]$ArcadeWebs,
    [switch]$NoStandOnWalls,
    [switch]$NoWallRun,
    [switch]$NoEyeOcclusion,
    [switch]$NoEyeAntialiasing,
    [switch]$GameMonitorView,
    [switch]$OwnShadows,
    [switch]$EyeLensDistances,
    [switch]$NoEyeReflections,
    [switch]$StockMonitorView,
    [switch]$FullDesktopView,
    [switch]$SmallerDesktopView,  # Update 33: a 1280 x 720 window (next launch), not 1920 x 1080
    [switch]$EngineTaa,  # Update 34: the eyes get the game's TAA targets, for the wrist's ENGINE TAA
    [switch]$KeepGameSettings,
    [switch]$AttachOnly,
    [switch]$CloseGame,  # 0.0.5 hotfix 3: the launcher asked; a running Spider-Man 2 is closed first, then VR starts
    [string]$XrRuntime
)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
# Update 31 (since 30.1): which update is installed, first and in green (a report from old files is told apart at once).
$versionFile=Join-Path $projectRoot 'PORT-VERSION'
$portVersion=if(Test-Path -LiteralPath $versionFile) { (Get-Content -LiteralPath $versionFile -TotalCount 1).Trim() } else { 'before Update 8' }
$publicFile=Join-Path $projectRoot 'VERSION'
$publicVersion=if(Test-Path -LiteralPath $publicFile) { (Get-Content -LiteralPath $publicFile -TotalCount 1).Trim() } else { 'unknown' }
Write-Host "SM2 VR Version $publicVersion (build $portVersion) - Made by Agent Mulder" -ForegroundColor Green
# Update 24: files from a downloaded zip carry Windows' "came from the internet" mark (Zone.Identifier), which
# can make Windows ask about or refuse them. Spidy's own folder is unmarked first (never anything outside it).
try {
    # The package's own files: the folder itself, tools, python and the modules (not reports or a build's
    # thousands of intermediates).
    $places=@($projectRoot) + @('tools','python','build\windows-ninja' | ForEach-Object { Join-Path $projectRoot $_ } |
        Where-Object { Test-Path -LiteralPath $_ })
    $marked=@(Get-ChildItem -LiteralPath $places -File -Force -ErrorAction SilentlyContinue | Where-Object {
        Get-Item -LiteralPath $_.FullName -Stream Zone.Identifier -ErrorAction SilentlyContinue })
    if($marked) {
        $marked | Unblock-File -ErrorAction SilentlyContinue
        Write-Host "Unblocked $($marked.Count) files Windows had marked as downloaded from the internet."
    }
} catch {
    Write-Warning "Could not unblock the downloaded files ($($_.Exception.Message)). If Windows refuses one, right-click the zip, Properties, tick Unblock, and extract it again."
}
# Update 22: the Python that runs the session. The shareable package uses only its own python\python.exe
# (Update 26, below); a development checkout tries the installed ones in turn and keeps the first that really runs and is 3.11 or
# newer. A broken one (a leftover shim pointing at a removed C:\Python310, say) is skipped, never started for VR.
function Test-SpidyPython([string]$candidate) {
    if(-not $candidate -or -not (Test-Path -LiteralPath $candidate -PathType Leaf)) { return $null }
    try {
        $output=& $candidate -c 'import sys; print(sys.version_info[:2])' 2>$null
        if($LASTEXITCODE -ne 0) { return $null }
        if("$output" -match '^\s*\((\d+),\s*(\d+)\)\s*$') {
            $version=[version]"$($Matches[1]).$($Matches[2])"
            if($version -ge [version]'3.11') { return $version }
        }
    } catch { }
    return $null
}
$pythonHelp=('No working Python 3.11 or newer was found. Either use the shareable SM2 VR package (it has its own ' +
             'Python in its python folder), or install Python 3.12 from https://www.python.org/downloads/windows/ ' +
             "and tick 'Add python.exe to PATH' during setup, then open a new PowerShell window and run this again.")
# Update 26: the shareable package (a python folder, or no sources) ALWAYS runs its own Python 3.12.10 and never
# looks at the PC's: a friend's older system Python was found and complained about when the bundled one could not
# be used. If the bundled one is missing or does not start, the package says so plainly and stops.
$bundledPython=Join-Path $projectRoot 'python\python.exe'
$packaged=(Test-Path -LiteralPath (Join-Path $projectRoot 'python')) -or
          -not (Test-Path -LiteralPath (Join-Path $projectRoot 'src'))
if($packaged) {
    if($Python -and $Python -ne $bundledPython) {
        Write-Host "This package always uses its own Python (python\python.exe); -Python $Python is ignored."
    }
    $Python=$bundledPython
    # The PC's Python settings must not reach the package's own (the embeddable one ignores them; this is a guard).
    foreach($variable in @('PYTHONHOME','PYTHONPATH','PYTHONSTARTUP','PYTHONUSERBASE')) {
        Remove-Item -LiteralPath "Env:$variable" -ErrorAction SilentlyContinue
    }
    if(-not (Test-Path -LiteralPath $bundledPython -PathType Leaf)) {
        throw ("This package's own Python is missing ($bundledPython). Spidy never uses the Python installed on " +
               'your PC, so nothing needs installing: right-click the zip, choose Extract All..., extract the whole ' +
               'zip again and keep every file. If it goes missing again, your antivirus removed it: allow the folder.')
    }
    $bundledOutput=''
    try { $bundledOutput="$(& $bundledPython -c 'import sys; print(sys.version_info[:2])' 2>&1)" } catch { $bundledOutput="$_" }
    if(-not (Test-SpidyPython $bundledPython)) {
        throw ("This package's own Python ($bundledPython) did not start: $bundledOutput. Spidy never uses the Python " +
               'installed on your PC, so installing or updating Python will not help. Extract the whole zip again ' +
               '(right-click it, Extract All...) to a folder such as Documents, and allow that folder in your antivirus.')
    }
    $bundledVersion="$(& $bundledPython -c "import sys; print('%d.%d.%d' % sys.version_info[:3])" 2>$null)".Trim()
    Write-Host "Using the package's own Python $bundledVersion (python\python.exe)."
} elseif($Python) {
    if(-not (Test-SpidyPython $Python)) {
        Write-Host "-Python $Python does not run, or is older than 3.11."
        throw $pythonHelp
    }
} else {
    $candidates=[System.Collections.Generic.List[string]]::new()
    if($env:USERPROFILE) {
        $candidates.Add((Join-Path $env:USERPROFILE '.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'))
    }
    foreach($command in @(Get-Command python.exe,python3.exe -CommandType Application -All -ErrorAction SilentlyContinue)) {
        $candidates.Add($command.Source)
    }
    # The py launcher knows installs that are not on PATH.
    $py=Get-Command py.exe -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
    if($py) {
        try {
            $found=& $py.Source -3 -c 'import sys; print(sys.executable)' 2>$null
            if($LASTEXITCODE -eq 0 -and $found) { $candidates.Add("$found".Trim()) }
        } catch { }
    }
    if($env:LOCALAPPDATA) {
        $installs=Join-Path $env:LOCALAPPDATA 'Programs\Python'
        if(Test-Path -LiteralPath $installs) {
            foreach($folder in (Get-ChildItem -LiteralPath $installs -Directory | Sort-Object Name -Descending)) {
                $candidates.Add((Join-Path $folder.FullName 'python.exe'))
            }
        }
    }
    foreach($candidate in $candidates) {
        if(Test-SpidyPython $candidate) { $Python=$candidate; break }
        if(Test-Path -LiteralPath $candidate -PathType Leaf) { Write-Host "Skipping $candidate (it does not run, or is older than 3.11)." }
    }
    if(-not $Python) { throw $pythonHelp }
}
# Update 24: Spidy's modules have Microsoft's C++ runtime built in (/MT), so no Visual C++ Redistributable is
# needed. tools\player_check.py (run by run_game_vr.py before anything is written) still asks for it, plainly,
# when an older build's modules import it, and checks the folder, the game and its version.
foreach($name in @('spidy_headset_probe.exe','spidy_bridge.dll','spidy_render_probe.dll',
                   'spidy_ray_bridge.dll','spidy_movement_bridge.dll','spidy_stereo_probe.dll',
                   'spidy_render_memory.dll')) {
    $requiredFile=Join-Path $projectRoot "build\windows-ninja\$name"
    if(-not(Test-Path -LiteralPath $requiredFile)) {
        # The shareable package (no src\) is never built: a missing module means a partial unzip.
        if(Test-Path -LiteralPath (Join-Path $projectRoot 'src')) { throw 'Build first with tools\build.ps1 -Observer.' }
        throw "build\windows-ninja\$name is missing. Unzip the whole SM2 VR package again, into a new folder."
    }
}
# An update zip extracted into a folder of its own rather than over the mod ($portVersion is printed at the start).
$misplaced=Get-ChildItem -LiteralPath $projectRoot -Directory | Where-Object {
    $_.Name -notin @('tools','src','include','tests','apps','docs','build','reports','python','test logs') -and
    (Test-Path -LiteralPath (Join-Path $_.FullName 'tools\launch-game-vr.ps1')) }
foreach($folder in $misplaced) {
    Write-Warning ("$($folder.FullName) holds an extracted update: its files belong in $projectRoot itself " +
                   '(copy its contents over the mod, replacing files, then build).')
}
# A build older than its sources runs the previous mod (an update extracted but not built).
$builtProbe=(Get-Item -LiteralPath (Join-Path $projectRoot 'build\windows-ninja\spidy_stereo_probe.dll')).LastWriteTimeUtc
# The shareable package has no src\ or include\ (nothing to build there): this check is for a checkout only.
$sourceFolders=@((Join-Path $projectRoot 'src'),(Join-Path $projectRoot 'include')) | Where-Object { Test-Path -LiteralPath $_ }
$newestSource=$null
if($sourceFolders) {
    $newestSource=Get-ChildItem -LiteralPath $sourceFolders -Recurse -File |
        Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
}
if($newestSource -and $newestSource.LastWriteTimeUtc -gt $builtProbe) {
    Write-Warning ("The mod's build is older than its sources ($($newestSource.Name) changed after it): this runs " +
                   'the previous mod. Build first: powershell -ExecutionPolicy Bypass -File .\tools\build.ps1 -Observer')
}
if($Seconds -eq 1 -or ($Size -gt 0 -and $Size -lt 64)) { throw 'Use 0 or 2..25 seconds and 0 or 64..8192 pixels.' }
if($Size -and $RenderScale -ne 100) { throw 'Use -Size or -RenderScale, not both.' }
if($Size -and $Supersample -ne 100) { throw 'Use -Size or -Supersample, not both.' }
if(($EyeHeight -eq 0) -ne ($ArmLength -eq 0) -or ($EyeHeight -and ($EyeHeight -lt 1000 -or $ArmLength -lt 250))) {
    throw 'Give both -EyeHeight (1000..2500 mm) and -ArmLength (250..1200 mm) from a T-pose calibration, or neither.'
}
# The button that shoots and holds webs, and the one that reels them in and shoots web balls.
$web='grip'; $reel='trigger'
if($TriggerWebs) { $web='trigger'; $reel='grip' }
Write-Host 'Spidy VR - connect your headset before starting (Quest 3: Virtual Desktop or Steam Link; SteamVR headsets: start SteamVR).'
if($XrRuntime) { Write-Host "VR runtime: $XrRuntime." }
else { Write-Host 'VR runtime: the one your headset is connected to (add -XrRuntime <manifest> to choose).' }
if($Size) { Write-Host "Eye resolution override: $Size x $Size." }
elseif($RenderScale -gt 100) { Write-Host "Eye resolution: $RenderScale% of the VR runtime's recommendation, per side. Larger eyes cost frame rate: on an RTX 4080 at 90 Hz, 125% gave 33 new pictures a second against 45 at 100% (uneven, felt as judder). For comfort use -RenderScale 100 or less." }
elseif($RenderScale -ne 100) { Write-Host "Eye resolution: $RenderScale% of the VR runtime's recommendation, per side." }
else { Write-Host "Eye resolution: the VR runtime's recommendation (lower -RenderScale is faster but more jagged)." }
# SM2 port: the eyes get no anti-aliasing and no upscaling (SM2VR 0.0.27 stores 0 in SpidyEyeTemporal), so
# -Upscale and -NoEyeAntialiasing change nothing; the game's own Anti aliasing/Upscale settings are for the monitor.
Write-Host "Eyes: rendered at full size without the game's anti-aliasing or upscaling (as SM2VR); -Upscale has no effect in Spider-Man 2."
# Update 9: the mod smooths each eye image itself (FXAA) on its way to the headset; no game addresses involved.
if($EyeAA -eq 'off') { Write-Host "Eye anti-aliasing: off (-EyeAA off); the eyes are shown as the game drew them." } else { Write-Host "Eye anti-aliasing: FXAA on each eye image (-EyeAA off to compare; live.txt 'aa = 0' or 'aa = 1' switches it while playing)." }
# Update 10: -Supersample renders the eyes larger than the headset's images and filters them down (no FXAA then).
if($Supersample -gt 100) { Write-Host "Eye supersampling: the eyes are rendered at $Supersample% of the headset's eye size per side and filtered down to it (Mitchell-Netravali, replaces FXAA); costs frame rate like -RenderScale $Supersample." }
else { Write-Host 'Eye supersampling: off (the default; a supersample chosen on the hologram applies from the next launch).' }
# The game biases its textures sharper for its monitor upscaler/anti-aliasing; without either, the eyes shimmer.
if($MipBias -eq 'game') { Write-Host "Texture mip bias: the game's own (reported, not changed)." }
else { Write-Host "Texture mip bias: $MipBias for the textures the game sharpens for its upscaler (Update 23 default 0.5: calmer, less roof and facade flicker, a little softer; -MipBias 0 is Update 22's sharper look; also changes the monitor; needs the launcher to start the game)." }
if($Anisotropy) { Write-Host "Texture filtering: $($Anisotropy)x anisotropic for those textures (-Anisotropy 0 keeps the game's)." }
else { Write-Host "Texture filtering: the game's own." }
if($Seconds) { Write-Host "Timed test: $Seconds seconds." }
else { Write-Host 'VR stays active until you close the game or press Ctrl+C here.' }
Write-Host 'VR starts with the game: its intro, menus, loading and cutscenes show on a screen in the headset.'
Write-Host 'There the controllers are an Xbox controller: thumbstick moves, A selects, B goes back, grips switch tabs.'
Write-Host 'In VR the menu button pauses and X opens the game menu (map, suits, skills).'
Write-Host "In VR B and Y are the game's own: B dodges the way the left stick points, Y interacts (backpacks,"
Write-Host 'doors, prompts; web strike in a fight). Look at the thing and press Y within about a metre of it;'
Write-Host 'hold Y and you step up to what you look at until the game takes it.'
Write-Host "Where the game shows LB + RB (a door or a grate to pull off), click the right thumbstick."
Write-Host 'Keep the game window in front on the desktop: the game pauses while another window is.'
Write-Host 'Settings > SPIDY VR > VIEW in the pause menu switches between immersive VR and a flat game screen.'
Write-Host "VR settings are in the game's own Settings: pause, Settings, then SPIDY VR (Up from GAME reaches it)."
Write-Host 'There change the web button, aim markers, webs in open air, swing speed, weight, your body calibration,'
Write-Host 'snap turn, smooth turn, vibration, screen size, the HUD, standing on walls, and the experimental flips and'
Write-Host 'flip speed during play.'
Write-Host "Press a $web to shoot that hand's web. Keep it held to swing; release it to let go."
Write-Host "Swing speed cap: $SwingSpeed m/s. Press the $reel while a web is attached to reel in."
if($TriggerWebs) { Write-Host 'Web button: the trigger (the grip reels in and shoots web balls).' }
else { Write-Host 'Web button: the grip (add -TriggerWebs to shoot webs with the trigger and reel with the grip).' }
$stamp=Get-Date -Format 'yyyyMMdd-HHmmss'
$report=Join-Path $projectRoot "reports\game-vr-$stamp.json"
$captureArgs=@()
if($CaptureImages) { $captureArgs=@('--capture-images') }
if($TriggerWebs) { $captureArgs+=@('--trigger-webs') }
if($ArcadeWebs) { $captureArgs+=@('--arcade-webs'); Write-Host 'Web style: arcade (webs go through lamp posts, signs and cars; swings stay above the street).' }
if($NoStandOnWalls) { $captureArgs+=@('--no-stand-on-walls'); Write-Host 'Stand on walls off: on a wall your view stays upright.' }
if($NoWallRun) { $captureArgs+=@('--no-wall-run'); Write-Host "Wall running off: the game's own wall crawl takes you at a wall." }
# Update 24: Spidy's overlay strands by default (as -OverlayWebs before); -GameWebs keeps the game's own.
if($GameWebs) { Write-Host "Webs: the game's own web lines (-GameWebs; without it Spidy's overlay strands)." }
else { $captureArgs+=@('--overlay-webs'); Write-Host "Webs: Spidy overlay strands (add -GameWebs for the game's own web lines)." }
if($NoWebGrab) { $captureArgs+=@('--no-web-grab'); Write-Host 'Web grab off: webs only swing.' }
else {
    Write-Host "A web aimed at a throwable prop or a thug catches it: the $reel reels it in, a sharp pull yanks"
    Write-Host "it to your hand, release the $web to throw (add -NoWebGrab to only swing)."
}
if($NoAirWebs) {
    $captureArgs+=@('--no-air-webs')
    Write-Host 'Webs in open air off: a web that meets nothing within 100 m misses.'
}
else { Write-Host 'A web that meets nothing within 100 m holds in open air there (add -NoAirWebs to make it miss).' }
if($NoBody) {
    $captureArgs+=@('--no-body')
    Write-Host 'Body off: the hero stays hidden in VR and gloves are drawn over the image.'
}
else { Write-Host "Your body: Spider-Man's, following your head and hands (add -NoBody for gloves)." }
if($NoPunch) { $captureArgs+=@('--no-punch'); Write-Host 'Punching off: fists pass through thugs.' }
else { Write-Host 'Punch a thug: a fist that hits him fast enough knocks him back (add -NoPunch to turn it off).' }
if($NoWebShooter) { $captureArgs+=@('--no-web-shooter'); Write-Host "Web shooter off: the $reel of a free hand shoots nothing." }
else {
    Write-Host "Press the $reel of a hand without a web to shoot the game's web balls where it points; a thug"
    Write-Host 'near that line takes it (add -NoWebShooter to turn it off).'
}
if($NoPromptHelp) {
    $captureArgs+=@('--no-prompt-help')
    Write-Host "Prompt help off: Spidy leaves the game's button prompts alone and the interact button is its Y."
}
else {
    Write-Host 'Where the game asks for a button, the interact button (Y) sends whatever it asks for, and the'
    Write-Host 'prompt names your VR control (add -NoPromptHelp to leave the prompts alone). Clicking both'
    Write-Host "thumbsticks latches the whole pad: grips as the game's LB and RB, triggers as its LT and RT."
}
if($NoAimMarkers) {
    $captureArgs+=@('--no-aim-markers')
    Write-Host "Aim markers hidden at the start: AIM MARKERS in Settings > SPIDY VR shows where each hand's web would land."
}
else { Write-Host "Aim markers show where each hand's web would land (add -NoAimMarkers to start hidden)." }
if($Flips) {
    $captureArgs+=@('--flips')
    Write-Host 'Flips (experimental) on: in the air the left stick turns you over; tap A for one flip, hold A to stay turned.'
}
if($EyeHeight) {
    $captureArgs+=@('--eye-height',$EyeHeight,'--arm-length',$ArmLength)
    Write-Host "Your body: eye height $EyeHeight mm, arm $ArmLength mm from a T-pose calibration."
}
elseif($NoCalibrationPrompt) {
    $captureArgs+=@('--no-calibration-prompt')
    Write-Host "Your size comes from the headset's height (Settings > SPIDY VR > CALIBRATE BODY asks for a T-pose)."
}
else {
    Write-Host 'At the first gameplay stand in a T-pose and hold both triggers: Spider-Man takes your height and'
    Write-Host 'arm length (B skips it; Settings > SPIDY VR > CALIBRATE BODY does it again).'
}
if($NoEyeOcclusion) {
    $captureArgs+=@('--no-eye-occlusion')
    Write-Host 'Eye occlusion off: each eye draws everything in view, hidden or not (about half the frame rate).'
}
if($NoEyeAntialiasing) { $captureArgs+=@('--no-eye-antialiasing') }
if($StockMonitorView) {
    $captureArgs+=@('--stock-monitor-view')
    Write-Host 'Monitor: stock game camera. The game culls and shades for it, not your head.'
}
elseif($GameMonitorView) {
    $captureArgs+=@('--game-monitor-view')
    Write-Host "Monitor: the game's own view of your head in immersive VR (a third scene view: about a fifth fewer frames)."
}
elseif($MonitorRate) { Write-Host "Monitor: your left eye in immersive VR, $MonitorRate pictures a second (-MonitorRate 0: every frame, slower in the headset; -StockMonitorView: the stock camera)." }
else { Write-Host 'Monitor: your left eye in immersive VR, every frame (-MonitorRate 30 leaves the headset more).' }
if($NoEyeReflections) { $captureArgs+=@('--no-eye-reflections') }
if($BuildingReach -ne 150) { Write-Host "Buildings: full models out to $BuildingReach% of the game's distance (150 unless set)." }
if($OwnShadows) {
    $captureArgs+=@('--own-shadows')
    Write-Host "Shadows: each eye makes its own sun shadow maps, as before October 11 (more work for the processor)."
}
if($EyeLensDistances) {
    $captureArgs+=@('--eye-lens-distances')
    Write-Host "Distances: the eyes' own wide lenses, as before October 19 (detail and textures load at about half the flat game's distance)."
}
if($KeepGameSettings) {
    $captureArgs+=@('--keep-game-settings')
    Write-Host "Game picture: your own depth of field, sharpness, motion blur, film grain, chromatic aberration and vignette (-KeepGameSettings); VR still turns off frame generation, vertical sync, ray tracing and Reflex."
} else {
    Write-Host 'Game picture: depth of field, motion blur, film grain, chromatic aberration and vignette off and sharpness lowest while VR runs (add -KeepGameSettings to keep yours); restored when the game closes.'
}
if($FullDesktopView) {
    $captureArgs+=@('--full-desktop-view')
    Write-Host "Desktop window: the game's own display settings."
}
elseif(-not $AttachOnly) {
    Write-Host 'Desktop window: small while VR runs, to save GPU time (add -FullDesktopView to keep your settings).'
    if($SmallerDesktopView) {
        $captureArgs+=@('--smaller-desktop-view')
        Write-Host 'Desktop window: 1280 x 720 (-SmallerDesktopView), about 3-5% fewer pixels a frame; the HUD panel is softer. The game reads it at launch.'
    }
}
if($EngineTaa) {
    $captureArgs+=@('--engine-taa')
    Write-Host "ENGINE TAA: prepared (-EngineTaa); set the game's Anti-Aliasing to TAA, then turn ENGINE TAA on from the wrist's GRAPHICS page."
}
if(-not $AttachOnly) { $captureArgs+=@('--auto-launch') }
if($XrRuntime) { $captureArgs+=@('--xr-runtime',$XrRuntime) }
if($CloseGame) { $captureArgs+=@('--close-game') }
if($SnapTurn -ne 30) { Write-Host "Snap turn: $SnapTurn degrees (0: off)." }
if($SmoothTurn -ne 0) { Write-Host "Smooth turn: $SmoothTurn degrees a second (replaces snap turning)." }
if($Weight -ne 80) { Write-Host "Weight: $Weight% of real gravity while webs fly you." }
if($Haptics -ne 100) { Write-Host "Controller vibration: $Haptics%." }
if($Hud -ne 'Medium') { Write-Host "HUD: $Hud." }
if($ScreenDepth -ne 'Medium') { Write-Host "Game screen 3D (cutscenes): $ScreenDepth." }
if($FlipSpeed -ne 180) { Write-Host "Flip speed: $FlipSpeed degrees a second at full tilt of the left stick." }
$screenIndex=@{Small=0;Medium=1;Large=2}[$ScreenSize]
$hudIndex=@{Off=0;Small=1;Medium=2;Large=3}[$Hud]
$depthIndex=@{Off=0;Low=1;Medium=2;High=3}[$ScreenDepth]
$captureArgs+=@('--snap-turn',$SnapTurn,'--smooth-turn',$SmoothTurn,'--haptics',$Haptics,'--screen-size',$screenIndex,
               '--render-scale',$RenderScale,'--upscale',$Upscale,'--mip-bias',$MipBias,'--anisotropy',$Anisotropy,'--eye-aa',$EyeAA.ToLower(),'--supersample',$Supersample,'--weight',$Weight,'--hud',$hudIndex,
               '--flip-speed',$FlipSpeed,'--monitor-rate',$MonitorRate,'--building-reach',$BuildingReach,'--screen-depth',$depthIndex)
& $Python (Join-Path $PSScriptRoot 'run_game_vr.py') --seconds $Seconds --size $Size --swing-speed $SwingSpeed --output $report @captureArgs
exit $LASTEXITCODE
