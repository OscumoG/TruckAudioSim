#Requires -Version 5.1
# TruckAudioSim ETS2 Live (v0.20) - sin driver, sin cambios automaticos de dispositivos.
# Este script solo captura la mezcla WASAPI de una salida de reproduccion diferente
# a la salida fisica; no inyecta codigo en ETS2.
[CmdletBinding()]
param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$engine = Join-Path $root 'bin-x64\tas_spatial.exe'
$probe  = Join-Path $root 'bin-x64\tas_endpoint_probe.exe'
$profileFiel = Join-Path $root 'profiles\ETS2_Estereo_Fiel.json'
$profileCabina = Join-Path $root 'profiles\Scania_S_7_1_Fidelidad.json'

function Fail([string]$reason) {
    Write-Host "`nERROR: $reason" -ForegroundColor Red
    exit 1
}
function Choose-Device([object[]]$devices, [string]$prompt, [object]$suggestion) {
    $defaultText = ''
    if ($null -ne $suggestion) { $defaultText = " [Intro = $($suggestion.Index)]" }
    while ($true) {
        $answer = Read-Host "$prompt$defaultText"
        if ([string]::IsNullOrWhiteSpace($answer) -and $null -ne $suggestion) {
            return $suggestion
        }
        $n = 0
        if ([int]::TryParse($answer, [ref]$n)) {
            $matched = @($devices | Where-Object { $_.Index -eq $n })
            if ($matched.Count -eq 1) { return $matched[0] }
        }
        Write-Host 'Numero de dispositivo no valido. Intenta otra vez.' -ForegroundColor Yellow
    }
}

Write-Host '======================================================' -ForegroundColor Cyan
Write-Host '  TruckAudioSim v0.20 - ETS2 LIVE - Windows 11 x64' -ForegroundColor Cyan
Write-Host '======================================================' -ForegroundColor Cyan
Write-Host 'Beta: captura WASAPI de ETS2 -> objetos espaciales -> auriculares.'
Write-Host 'No instala controladores ni modifica la salida predeterminada de Windows.'
Write-Host 'IMPORTANTE: tu Steam Streaming Speakers ofrece 2.0 EXACT, no 7.1 real.'
Write-Host ''

foreach ($path in @($engine, $probe, $profileFiel, $profileCabina)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { Fail "Falta archivo: $path. Extrae todo el ZIP." }
}

Write-Host 'Dispositivos detectados:' -ForegroundColor White
$raw = @(& $engine --list-devices 2>&1)
if ($LASTEXITCODE -ne 0) { Fail "La enumeracion de dispositivos fallo: $($raw -join ' ')" }
$devices = @()
foreach ($lineObject in $raw) {
    $line = [string]$lineObject
    if ($line -match '^\s*\[(\d+)\]\s+(.+?)\s*$') {
        $devices += [pscustomobject]@{ Index = [int]$Matches[1]; Name = $Matches[2]; Id = '' }
    } elseif ($line -match '^\s*ID:\s*(\S+)\s*$') {
        if ($devices.Count -gt 0) { $devices[$devices.Count - 1].Id = $Matches[1] }
    }
}
if ($devices.Count -lt 2) { Fail "No se encontraron dos dispositivos de reproduccion: $($raw -join ' ')" }
foreach ($d in $devices) {
    Write-Host ("  [{0}] {1}" -f $d.Index, $d.Name)
}
Write-Host ''
$defaultSource = @($devices | Where-Object { $_.Name -match 'Steam Streaming Speakers' } | Select-Object -First 1)
$defaultOutput = @($devices | Where-Object { $_.Name -match 'Realtek High Definition Audio' -and $_.Name -notmatch 'Digital' } | Select-Object -First 1)
$source = Choose-Device $devices 'Dispositivo de ENTRADA (salida virtual donde enviaras ETS2)' $(if ($defaultSource.Count) { $defaultSource[0] } else { $null })
$output = Choose-Device $devices 'Dispositivo de SALIDA (auriculares fisicos con Dolby Atmos)' $(if ($defaultOutput.Count) { $defaultOutput[0] } else { $null })
if ($source.Index -eq $output.Index -or $source.Id -eq $output.Id) { Fail 'Entrada y salida deben ser distintas para evitar realimentacion.' }
if ([string]::IsNullOrWhiteSpace($source.Id) -or [string]::IsNullOrWhiteSpace($output.Id)) { Fail 'No se pudo obtener el identificador estable de ambos dispositivos.' }
Write-Host ''
Write-Host "Origen:  [$($source.Index)] $($source.Name)" -ForegroundColor Cyan
Write-Host "Destino: [$($output.Index)] $($output.Name)" -ForegroundColor Cyan

if ($source.Name -notmatch 'Steam Streaming Speakers|Virtual|CABLE|Voicemeeter|Digital Output') {
    Write-Host 'AVISO: este origen puede tener audio audible por si mismo y producir duplicacion.' -ForegroundColor Yellow
}
Write-Host "`nVerificando formato de la entrada..."
$probeResult = @(& $probe --input-id $source.Id 2>&1)
if ($LASTEXITCODE -ne 0) { Fail "No se pudo consultar el origen: $($probeResult -join ' ')" }
$mixLine = @($probeResult | Where-Object { [string]$_ -match 'Current shared mix:' }) | Select-Object -First 1
if ($null -eq $mixLine) { Fail 'No se detecto el formato compartido del origen.' }
Write-Host $mixLine
if (-not ([string]$mixLine -match 'Current shared mix:\s*(\d+)ch\s+(\d+)Hz\s+(\d+)bit')) {
    Fail 'No se pudo interpretar el formato. Comprueba manualmente con tas_endpoint_probe.exe.'
}
$channelCount = [int]$Matches[1]
$sampleRate = [int]$Matches[2]
$bitDepth = [int]$Matches[3]
if ($sampleRate -ne 48000 -or $channelCount -notin @(2,3,6,8) -or $bitDepth -notin @(16,32)) {
    Fail "Formato de origen no admitido ($channelCount canales, $sampleRate Hz, $bitDepth bits). Selecciona 48 kHz, 16 o 32 bits."
}

Write-Host "`nPerfil para la primera prueba:"
Write-Host ' [1] FIEL: dos objetos (L/R), sin canales traseros inventados (recomendado).'
Write-Host ' [2] CABINA: Scania S, varios objetos derivados del estereo (efecto experimental).'
$mode = Read-Host 'Perfil [Intro = 1]'
$profile = $profileFiel
if ($mode -eq '2') { $profile = $profileCabina }
elseif (-not [string]::IsNullOrWhiteSpace($mode) -and $mode -ne '1') { Fail 'Perfil no valido.' }

Write-Host "`nCONFIGURACION DE WINDOWS" -ForegroundColor Yellow
Write-Host '1. Inicia Euro Truck Simulator 2 desde Steam y deja el juego generando sonido.'
Write-Host '2. Windows -> Configuracion -> Sistema -> Sonido -> Mezclador de volumen.'
Write-Host "3. En 'Aplicaciones', busca Euro Truck Simulator 2 y cambia solo su SALIDA a:"
Write-Host "   $($source.Name)" -ForegroundColor Green
Write-Host "4. Mantén los auriculares Realtek como salida fisica y Atmos activo."
Write-Host '5. No silencies ETS2 en el mezclador, ya que se silenciaria la captura.'
Write-Host '   Si el cambio no se aplica, reinicia ETS2 tras elegir esa salida.'
Write-Host "`nAbre el mezclador de volumen en este momento? (S/n)"
$openMixer = Read-Host 'Respuesta'
if ($openMixer -ne 'n' -and $openMixer -ne 'N') {
    try { Start-Process 'ms-settings:apps-volume' | Out-Null }
    catch { Write-Host 'Abre manualmente Configuracion > Sistema > Sonido > Mezclador de volumen.' -ForegroundColor Yellow }
}
[void](Read-Host "`nPulsa INTRO cuando ETS2 este enviando audio al dispositivo de entrada")
if (-not (Get-Process -Name 'eurotrucks2' -ErrorAction SilentlyContinue)) {
    Write-Host 'Nota: no he detectado eurotrucks2.exe; puedes iniciar el juego despues.' -ForegroundColor Yellow
}
Write-Host "`nIniciando motor espacial:"
Write-Host 'Modo esperado: WASAPI_LOOPBACK_TO_SPATIAL_DYNAMIC' -ForegroundColor Green
Write-Host 'Conduce, gira la camara y compara la posicion de sonidos. CTRL+C para detener.'
Write-Host 'Es normal que la entrada Steam solo aporte DOS canales reales.' -ForegroundColor Yellow
Write-Host ''
& $engine --preset $profile --input-id $source.Id --output-id $output.Id
$exitCode = $LASTEXITCODE
if ($exitCode -ne 0) { Fail "Motor espacial terminado con codigo $exitCode. Revisa el mensaje anterior." }
Write-Host "`nFin de la sesion. Vuelve a poner ETS2 en tu salida habitual cuando termines." -ForegroundColor Cyan
