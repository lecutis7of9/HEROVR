# HEROVR 0.0.78 — reversible LOD comparison (no rebuild needed)
# Run from the HEROVR project folder (where live.txt is located).
# Only changes shared_lod; leaves AA, eye resolution and other settings alone.
param([ValidateSet('baseline','off','higher','restore','status')][string]$Mode='status')
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$live=Join-Path $root 'live.txt'
$backup=Join-Path $root 'live.txt.skyline-backup'
$marker=Join-Path $root 'live.txt.skyline-created'
function Say($s) { Write-Host $s }
if($Mode -eq 'status') {
  Say "Settings: $live"
  if(Test-Path $live) {
    $hits=@(Get-Content $live | Where-Object { $_ -match '^\s*shared_lod\s*=' })
    Say ("shared_lod: " + $(if($hits.Count){$hits[-1]}else{'not set (compiled default: HIGHER / 2)'}))
    $aa=@(Get-Content $live | Where-Object { $_ -match '^\s*vr_aa\s*=' })
    Say ("vr_aa: " + $(if($aa.Count){$aa[-1]}else{'not set (launcher default: ours)'}))
  } else { Say 'No live.txt yet (defaults are used).' }
  Say ("Backup present: " + (Test-Path $backup))
  exit
}
if($Mode -eq 'restore') {
  if(Test-Path $backup) { Copy-Item -LiteralPath $backup -Destination $live -Force; Remove-Item $backup; if(Test-Path $marker){Remove-Item $marker}; Say 'Original live.txt restored.' }
  elseif(Test-Path $marker) { Remove-Item $live -ErrorAction SilentlyContinue; Remove-Item $marker; Say 'Temporary live.txt removed.' }
  else { Say 'No saved original found. Nothing changed.' }
  exit
}
if(!(Test-Path $backup) -and !(Test-Path $marker)) {
  if(Test-Path $live) { Copy-Item -LiteralPath $live -Destination $backup }
  else { New-Item -ItemType File -Path $marker | Out-Null }
}
$value=if($Mode -eq 'off'){'0'}else{'2'}
$lines=if(Test-Path $live){@(Get-Content -LiteralPath $live)}else{@()}
# Remove every active shared_lod assignment, preserve unrelated lines verbatim.
$kept=@($lines | Where-Object { $_ -notmatch '^\s*shared_lod\s*=' })
$kept+=("shared_lod = "+$value)
Set-Content -LiteralPath $live -Value $kept -Encoding UTF8
Say ("Set shared_lod = "+$value+"; other settings retained.")
Say 'Compare the SAME skyline view. This changes LOD, not texture mip streaming or TAA.'
Say 'Use restore to return to your exact original live.txt.'
