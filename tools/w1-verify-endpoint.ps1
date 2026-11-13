# SPDX-License-Identifier: MIT
# Passive user-mode validation: never installs a driver, changes the default
# endpoint, modifies spatial sound or changes Windows security settings.
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][ValidateSet(2,6,8)][int]$Channels,
    [string]$EndpointId,
    [switch]$RequireClean,
    [string]$VerifierPath
)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if (-not $VerifierPath) {
    $VerifierPath = Join-Path $root 'build-win\Release\tas_endpoint_verify.exe'
}
if (-not (Test-Path -LiteralPath $VerifierPath -PathType Leaf)) {
    throw "Compila primero la versión actual desde el código fuente: cmake -S . -B build-win -A x64; cmake --build build-win --config Release. No se usa el ejecutable antiguo de bin-x64 porque no admite --expect-channels."
}
$verifier = (Resolve-Path -LiteralPath $VerifierPath).Path
$logDir = Join-Path $root 'build-win\validation'
New-Item -ItemType Directory -Path $logDir -Force | Out-Null
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$log = Join-Path $logDir "w1-$($Channels)ch-$stamp.log"
if (-not $EndpointId) {
    Write-Host "Salidas de reproducción disponibles:" -ForegroundColor Cyan
    & $verifier --list-devices
    if ($LASTEXITCODE -ne 0) { throw 'No se pudieron enumerar las salidas de audio.' }
    $EndpointId = Read-Host 'ID completo del dispositivo TruckAudioSim Virtual Audio en la VM'
}
if ([string]::IsNullOrWhiteSpace($EndpointId)) { throw 'Debes indicar un ID de dispositivo.' }
Write-Host "Antes de la prueba verifica manualmente que Windows tenga $Channels canales a 48 kHz en esa salida." -ForegroundColor Yellow
$argsList = @('--output-id', $EndpointId, '--expect-channels', [string]$Channels)
if ($RequireClean) { $argsList += '--require-clean' }
$hash = (Get-FileHash -LiteralPath $verifier -Algorithm SHA256).Hash
$header = @(
  "TruckAudioSim W1 verifier (no driver install)",
  "LocalTime: $(Get-Date -Format o)",
  "Binary: $verifier", "SHA256: $hash", "RequestedChannels: $Channels",
  "EndpointId: $EndpointId", "StrictDrops: $([bool]$RequireClean)",
  "Arguments: $($argsList -join ' ')"
)
$header | Set-Content -LiteralPath $log -Encoding UTF8
& $verifier @argsList 2>&1 | Tee-Object -FilePath $log -Append
$exit = $LASTEXITCODE
"ExitCode: $exit" | Add-Content -LiteralPath $log -Encoding UTF8
Write-Host "Registro guardado: $log"
if ($exit -ne 0) { throw "Prueba W1 fallida (código $exit). Consulta $log" }
Write-Host "PASS del verificador WASAPI para $Channels canales. Esto NO confirma firma o instalación del driver ni escucha física." -ForegroundColor Green
