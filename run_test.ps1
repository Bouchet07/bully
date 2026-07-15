param(
    [string]$PatchName = "patch",
    [int]$Rounds = 50,
    [int]$Concurrency = 4
)

# Paths
$WorkspaceDir = "C:\Users\diego\Desktop\Programming\c++\bully"
$EngineDir = "$WorkspaceDir\engines"
$ResultDir = "$WorkspaceDir\results"
$BaseExe = "$EngineDir\bully-base.exe"
$PatchExe = "$WorkspaceDir\build\bully-native.exe"
$SavedPatchExe = "$EngineDir\bully-$PatchName.exe"
$BookFile = "$WorkspaceDir\books\noob_3moves.epd"
$CuteChess = "C:\Program Files (x86)\Cute Chess\cutechess-cli.exe"
$Ordo = "C:\Users\diego\Desktop\Programming\c++\ordo-1.2.6-win\ordo-win64.exe"

# Create directories if they don't exist
if (!(Test-Path $EngineDir)) {
    New-Item -ItemType Directory -Path $EngineDir | Out-Null
}
if (!(Test-Path $ResultDir)) {
    New-Item -ItemType Directory -Path $ResultDir | Out-Null
}

# Ensure compiled executable exists
if (!(Test-Path $PatchExe)) {
    Write-Error "Compiled executable not found at $PatchExe. Please run 'ninja -C build' first."
    exit 1
}

# Copy current compiled native executable to patch location
Copy-Item -Path $PatchExe -Destination $SavedPatchExe -Force
Write-Host "Saved patch executable to $SavedPatchExe"

# Ensure base executable exists. If not, copy current native as base.
if (!(Test-Path $BaseExe)) {
    Copy-Item -Path $PatchExe -Destination $BaseExe -Force
    Write-Host "Base executable not found. Saved current build as base: $BaseExe"
}

# Output file paths
$PgnFile = "$ResultDir\match_$PatchName.pgn"
$RatingFile = "$ResultDir\ratings_$PatchName.txt"

Write-Host "Starting match Base vs $PatchName ($Rounds rounds, $($Rounds*2) games)..."
& $CuteChess -engine cmd=$BaseExe name=Base -engine cmd=$SavedPatchExe name=$PatchName -each proto=uci tc=1+0.05 option.Hash=16 -openings file=$BookFile format=epd -concurrency $Concurrency -rounds $Rounds -repeat -recover -pgnout $PgnFile

# Run Ordo
Write-Host "Calculating ratings..."
& $Ordo -p $PgnFile -o $RatingFile -A Base

Write-Host "`n=== MATCH RESULTS ($PatchName) ==="
Get-Content $RatingFile
Write-Host "================================"
