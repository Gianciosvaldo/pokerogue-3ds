# Fetches the portable Python the builder runs on (once), checked by SHA-256. Called by BUILD-SD-CARD.bat.
# Needs Windows 10 (version 1803 or later) or Windows 11 for curl.exe and tar.exe.
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$pin = (Get-Content (Join-Path $here 'pins.json') -Raw | ConvertFrom-Json).python
$runtime = Join-Path $here 'runtime'
$python = Join-Path $runtime 'python\python.exe'
$marker = Join-Path $runtime 'python.ok'
$log = Join-Path (Split-Path -Parent $here) 'build-log.txt'

function Log($msg) { Add-Content -Path $log -Value ("{0}  setup: {1}" -f (Get-Date -Format s), $msg) }

if ((Test-Path $python) -and (Test-Path $marker) -and ((Get-Content $marker -Raw).Trim() -eq $pin.sha256)) { exit 0 }

try {
  New-Item -ItemType Directory -Force -Path $runtime | Out-Null
  $downloads = Join-Path $here 'downloads'
  New-Item -ItemType Directory -Force -Path $downloads | Out-Null
  $file = Join-Path $downloads ([IO.Path]::GetFileName(([Uri]$pin.url).AbsolutePath))
  Write-Host ''
  Write-Host ' Getting ready: downloading the builder''s own copy of Python (45 MB, only the first time)...'
  Log "download $($pin.url)"

  $ok = $false
  if ((Test-Path $file) -and ((Get-FileHash $file -Algorithm SHA256).Hash -eq $pin.sha256)) { $ok = $true }
  for ($try = 1; -not $ok -and $try -le 4; $try++) {
    if (Test-Path $file) { Remove-Item $file -Force }
    $curl = Join-Path $env:SystemRoot 'System32\curl.exe'
    if (Test-Path $curl) {
      & $curl --fail --location --silent --show-error --retry 3 --output $file $pin.url
    } else {
      Invoke-WebRequest -Uri $pin.url -OutFile $file -UseBasicParsing
    }
    if ((Test-Path $file) -and ((Get-FileHash $file -Algorithm SHA256).Hash -eq $pin.sha256)) { $ok = $true }
    elseif ($try -lt 4) { Write-Host " The download didn't come through whole; trying again ($try of 3)..."; Start-Sleep -Seconds (2 * $try) }
  }
  if (-not $ok) { throw "could not download Python from $($pin.url) (check the internet connection)" }

  $tar = Join-Path $env:SystemRoot 'System32\tar.exe'
  if (-not (Test-Path $tar)) { throw 'tar.exe is missing: this needs Windows 10 (version 1803 or later) or Windows 11' }
  if (Test-Path (Join-Path $runtime 'python')) { Remove-Item (Join-Path $runtime 'python') -Recurse -Force }
  & $tar -xzf $file -C $runtime
  if ($LASTEXITCODE -ne 0 -or -not (Test-Path $python)) { throw 'unpacking Python failed' }
  Set-Content -Path $marker -Value $pin.sha256
  Log 'python ready'
  exit 0
} catch {
  Log "FAILED: $_"
  Write-Host ''
  Write-Host " Setup failed: $_" -ForegroundColor Red
  exit 1
}
