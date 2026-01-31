$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$solver = Join-Path $root "vs2017\x64\Release\sudoku_ants.exe"
$project = Join-Path $root "vs2017\sudoku_ants.vcxproj"
$vsDevCmd = "D:\Program Files\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat"
$apiDir = Join-Path $root "web\api"

if (Test-Path $project) {
	$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
	$msbuild = $null
	if (Test-Path $vswhere) {
		$msbuild = & $vswhere -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe | Select-Object -First 1
	}
	if (-not $msbuild) {
		$msbuildCmd = Get-Command msbuild -ErrorAction SilentlyContinue
		if ($msbuildCmd) {
			$msbuild = $msbuildCmd.Source
		}
	}
	if (-not $msbuild -and (Test-Path $vsDevCmd)) {
		Write-Host "Building solver (Release|x64) via VsDevCmd..."
		$quotedProject = '"' + $project + '"'
		$cmd = "`"$vsDevCmd`" -arch=x64 && msbuild $quotedProject /p:Configuration=Release /p:Platform=x64 /m"
		& cmd.exe /c $cmd
	} else {
		if (-not $msbuild) {
			$msbuild = "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe"
		}
		if (-not (Test-Path $msbuild)) {
			Write-Host "MSBuild not found. Install Visual Studio Build Tools."
			exit 1
		}
		Write-Host "Building solver (Release|x64)..."
		& $msbuild $project /p:Configuration=Release /p:Platform=x64 /m
	}
} else {
	Write-Host "Project not found: $project"
}

if (-not (Test-Path $solver)) {
	Write-Host "Solver not found after build: $solver"
	exit 1
}

$env:SOLVER_PATH = $solver
Write-Host "SOLVER_PATH = $env:SOLVER_PATH"

Set-Location $apiDir
python -m uvicorn main:app --host 0.0.0.0 --port 8000
