$ErrorActionPreference = 'Stop'
$source = Join-Path $PSScriptRoot 'Kontrol-ID-Provisioner.cs'
$manifest = Join-Path $PSScriptRoot 'app.manifest'
$output = Join-Path $PSScriptRoot '..\..\usr\local\www\kontrol-id\Kontrol-ID-Provisioner.exe'
$compiler = Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
if (-not (Test-Path -LiteralPath $compiler)) {
    $compiler = Join-Path $env:WINDIR 'Microsoft.NET\Framework\v4.0.30319\csc.exe'
}
if (-not (Test-Path -LiteralPath $compiler)) {
    throw 'The .NET Framework C# compiler was not found.'
}
& $compiler /nologo /target:winexe /optimize+ /platform:anycpu /win32manifest:$manifest `
    /reference:System.dll /reference:System.Core.dll /reference:System.Drawing.dll `
    /reference:System.Windows.Forms.dll /reference:System.Web.Extensions.dll `
    /out:$output $source
if ($LASTEXITCODE -ne 0) {
    throw "C# compilation failed with exit code $LASTEXITCODE."
}
Write-Host "Built $output"
