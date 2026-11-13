# SPDX-License-Identifier: MIT
[CmdletBinding(DefaultParameterSetName = 'Build')]
param(
    [Parameter(Mandatory, ParameterSetName = 'EndpointId')]
    [ValidateNotNullOrEmpty()]
    [string]$OutputId,
    [Parameter(Mandatory, ParameterSetName = 'EndpointIndex')]
    [ValidateRange(0, 2147483647)]
    [int]$OutputIndex,
    [Parameter(ParameterSetName = 'EndpointId')]
    [Parameter(ParameterSetName = 'EndpointIndex')]
    [switch]$RequireSpatial,
    [Parameter(ParameterSetName = 'EndpointId')]
    [Parameter(ParameterSetName = 'EndpointIndex')]
    [switch]$AudioOnly,
    [Parameter(Mandatory, ParameterSetName = 'ListDevices')]
    [switch]$ListAudioDevices,
    [Parameter(Mandatory, ParameterSetName = 'GuidedSonic')]
    [switch]$GuidedSonic
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$build = Join-Path $root 'build-win'
$logDirectory = Join-Path $build 'validation'
New-Item -ItemType Directory -Path $logDirectory -Force | Out-Null
$runName = [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmssfff')
$log = Join-Path $logDirectory "w0-$runName.log"
$junit = Join-Path $logDirectory "w0-$runName.xml"

function Write-ValidationLine([string]$Line) {
    $Line | Tee-Object -FilePath $log -Append | Write-Host
}

function Invoke-LoggedNative {
    param([string]$Program, [string[]]$CommandArgs, [switch]$Utf8, [switch]$CaptureOutput)
    $ErrorActionPreference = 'Continue'
    $PSNativeCommandUseErrorActionPreference = $false
    Write-ValidationLine "> $Program $($CommandArgs -join ' ')"
    $encoding = [Console]::OutputEncoding
    try {
        if ($Utf8) { [Console]::OutputEncoding = [Text.Encoding]::UTF8 }
        $lines = @(
            & $Program @CommandArgs 2>&1 | ForEach-Object { "$_" } |
                Tee-Object -FilePath $log -Append | ForEach-Object {
                    Write-Host $_
                    if ($CaptureOutput) { $_ }
                }
        )
        $code = $LASTEXITCODE
    } finally {
        if ($Utf8) { [Console]::OutputEncoding = $encoding }
    }
    Write-ValidationLine "Exit code = $code"
    if ($code -ne 0) {
        throw "$Program exited with code $code. See $log"
    }
    if ($CaptureOutput) { $lines }
}

function Invoke-GuidedSonic {
    param([string]$Program, [string]$Wav)

    $lines = @(Invoke-LoggedNative $Program @('--list-devices') -Utf8 -CaptureOutput)
    $devices = @{}
    $pattern = '(?m)^[ \t]*\[(?<index>[0-9]+)\][ \t]+(?<name>[^\r\n]+)\r?\n[ \t]+ID:[ \t]+(?<id>[^\r\n]+)'
    foreach ($entry in [regex]::Matches(($lines -join "`n"), $pattern)) {
        $devices[$entry.Groups['index'].Value] = $entry
    }
    if ($devices.Count -eq 0) {
        throw 'Windows no muestra salidas de audio activas.'
    }

    do {
        $index = (Read-Host 'Escribe el numero de tus auriculares o Q para salir').Trim()
        if ($index -eq 'Q') {
            Write-ValidationLine 'Prueba cancelada antes de reproducir audio.'
            return
        }
    } until ($devices.ContainsKey($index))
    $device = $devices[$index]
    $name = $device.Groups['name'].Value.Trim()
    $id = $device.Groups['id'].Value.Trim()
    Write-ValidationLine "Salida elegida: $name"
    Write-ValidationLine "ID de salida: $id"

    Write-Host 'En Configuracion > Sistema > Sonido, abre esa salida y elige Windows Sonic para auriculares.'
    do {
        $ready = (Read-Host 'Presiona Intro cuando este activo o escribe Q para salir').Trim()
    } until ($ready -eq '' -or $ready -eq 'Q')
    if ($ready -eq 'Q') {
        Write-ValidationLine 'Prueba cancelada antes de reproducir audio.'
        return
    }
    Write-ValidationLine 'Proveedor indicado por el usuario: Windows Sonic para auriculares'
    Write-ValidationLine "WAV SHA256: $((Get-FileHash -LiteralPath $Wav).Hash)"
    Write-Host 'Escucha si la fuente se desplaza de izquierda a derecha y vuelve a la izquierda.'
    $audio = @(Invoke-LoggedNative $Program @('--smoke-test', '--wav', $Wav,
        '--output-id', $id, '--require-spatial') -CaptureOutput)
    if (!($audio -match '^Smoke-test mode: SPATIAL_DYNAMIC\b') -or
        !($audio -match '^Smoke-test stream completed: 192000 input frames\b')) {
        throw 'El registro no confirma la ruta espacial y el WAV completo.'
    }

    do {
        $heard = (Read-Host 'Oiste el recorrido? Escribe S (si), N (no) o D (dudoso)').Trim().ToUpperInvariant()
    } until ($heard -in @('S', 'N', 'D'))
    switch ($heard) {
        'S' { Write-ValidationLine 'Confirmacion audible: recorrido izquierda-derecha-izquierda percibido.' }
        'N' { Write-ValidationLine 'Confirmacion audible: recorrido no percibido.' }
        'D' { Write-ValidationLine 'Confirmacion audible: resultado dudoso.' }
    }
}

Push-Location $root
try {
    Write-ValidationLine ("UTC " + [DateTime]::UtcNow.ToString('o'))
    $os = Get-CimInstance Win32_OperatingSystem
    Write-ValidationLine "$($os.Caption) $($os.Version) build $($os.BuildNumber) $($os.OSArchitecture)"
    $exe = Join-Path $build 'Release\tas_spatial.exe'
    $wav = 'demo_stereo_48k.wav'
    if ($AudioOnly -or $ListAudioDevices -or $GuidedSonic) {
        $exe = Join-Path $root 'validacion-x64\tas_spatial.exe'
        $wav = 'validacion-x64\demo_stereo_48k.wav'
        if (!(Test-Path -LiteralPath $exe -PathType Leaf)) {
            throw "Missing bundled diagnostic: $exe"
        }
        Write-ValidationLine "Diagnostic SHA256: $((Get-FileHash -LiteralPath $exe).Hash)"
    } else {
        Get-Command cmake.exe, ctest.exe, python.exe -ErrorAction Stop | Out-Null
        Invoke-LoggedNative cmake.exe @('--version')
        Invoke-LoggedNative python.exe @('--version')
        Invoke-LoggedNative cmake.exe @('-S', '.', '-B', $build, '-G', 'Visual Studio 17 2022',
            '-A', 'x64', '-DTAS_REQUIRE_PYTHON_TESTS=ON')
        $compilerInfo = Get-ChildItem -LiteralPath (Join-Path $build 'CMakeFiles') -Recurse -Filter CMakeCXXCompiler.cmake |
            Sort-Object LastWriteTime -Descending | Select-Object -First 1
        if ($compilerInfo) {
            Select-String -LiteralPath $compilerInfo.FullName -Pattern '^set\(CMAKE_CXX_COMPILER_(ID|VERSION) "' |
                ForEach-Object { Write-ValidationLine $_.Line }
        }
        Invoke-LoggedNative cmake.exe @('--build', $build, '--config', 'Release')
        Invoke-LoggedNative ctest.exe @('--test-dir', $build, '-C', 'Release', '--output-on-failure',
            '--no-tests=error', '--output-junit', $junit)
        Write-ValidationLine "CTest XML: $junit"
    }

    if ($GuidedSonic) {
        Invoke-GuidedSonic $exe $wav
    } elseif ($PSCmdlet.ParameterSetName -ne 'Build') {
        Invoke-LoggedNative $exe @('--list-devices') -Utf8
        if ($ListAudioDevices) { return }
        if (!$AudioOnly) { Invoke-LoggedNative python.exe @('tools\make-demo-wav.py') }
        Write-ValidationLine "WAV SHA256: $((Get-FileHash -LiteralPath $wav).Hash)"
        $smokeArgs = @('--smoke-test', '--wav', $wav)
        if ($PSCmdlet.ParameterSetName -eq 'EndpointId') {
            $smokeArgs += @('--output-id', $OutputId)
        } else {
            $smokeArgs += @('--output-index', "$OutputIndex")
        }
        if ($RequireSpatial) { $smokeArgs += '--require-spatial' }
        Invoke-LoggedNative $exe $smokeArgs
        Write-ValidationLine 'Audible output and perceived motion require manual confirmation.'
    }
} finally {
    Pop-Location
    Write-Host "Validation log: $log"
}
