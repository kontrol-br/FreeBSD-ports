param(
    [string]$CodeSigningThumbprint = $env:KONTROL_CODE_SIGNING_THUMBPRINT,
    [string]$TimestampUrl = 'http://timestamp.digicert.com',
    [switch]$RequireSigned
)

$ErrorActionPreference = 'Stop'
$source = Join-Path $PSScriptRoot 'Kontrol-ID-Provisioner.cs'
$manifest = Join-Path $PSScriptRoot 'app.manifest'
$icon = Join-Path $PSScriptRoot 'Kontrol-ID.ico'
$logo = Join-Path $PSScriptRoot 'Kontrol-Logo.png'
$logoResource = "/resource:$logo,KontrolIdProvisioner.Logo"
$output = Join-Path $PSScriptRoot '..\..\usr\local\www\kontrol-id\Kontrol-ID-Provisioner.exe'
$compiler = Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
if (-not (Test-Path -LiteralPath $compiler)) {
    $compiler = Join-Path $env:WINDIR 'Microsoft.NET\Framework\v4.0.30319\csc.exe'
}
if (-not (Test-Path -LiteralPath $compiler)) {
    throw 'The .NET Framework C# compiler was not found.'
}
& $compiler /nologo /target:winexe /optimize+ /platform:anycpu /win32manifest:$manifest `
    /win32icon:$icon `
    $logoResource `
    /reference:System.dll /reference:System.Core.dll /reference:System.Drawing.dll `
    /reference:System.Windows.Forms.dll /reference:System.Web.Extensions.dll `
    /out:$output $source
if ($LASTEXITCODE -ne 0) {
    throw "C# compilation failed with exit code $LASTEXITCODE."
}
try {
    [Reflection.AssemblyName]::GetAssemblyName($output) | Out-Null
} catch {
    throw "The generated Provisioner is not a valid PE/CLR executable: $($_.Exception.Message)"
}
if (-not [string]::IsNullOrWhiteSpace($CodeSigningThumbprint)) {
    $thumbprint = ($CodeSigningThumbprint -replace '[^0-9A-Fa-f]', '').ToUpperInvariant()
    $certificate = $null
    $storeLocation = ''
    foreach ($location in @('CurrentUser', 'LocalMachine')) {
        $candidate = Get-Item -LiteralPath "Cert:\$location\My\$thumbprint" -ErrorAction SilentlyContinue
        if ($candidate) {
            $certificate = $candidate
            $storeLocation = $location
            break
        }
    }
    if (-not $certificate -or -not $certificate.HasPrivateKey) {
        throw "The code-signing certificate $thumbprint was not found with its private key."
    }
    if (-not ($certificate.EnhancedKeyUsageList.ObjectId.Value -contains '1.3.6.1.5.5.7.3.3')) {
        throw "The selected certificate is not valid for Authenticode code signing."
    }
    $kitsRoot = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\bin'
    $signTool = Get-ChildItem -Path $kitsRoot -Filter 'signtool.exe' -Recurse -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -match '\\x64\\signtool\.exe$' } |
        Sort-Object { [version]$_.VersionInfo.FileVersion } -Descending |
        Select-Object -First 1 -ExpandProperty FullName
    if (-not $signTool) {
        throw 'signtool.exe from the Windows SDK was not found.'
    }
    $storeArguments = @()
    if ($storeLocation -eq 'LocalMachine') { $storeArguments += '/sm' }
    & $signTool sign @storeArguments /sha1 $thumbprint /fd SHA256 /tr $TimestampUrl /td SHA256 $output
    if ($LASTEXITCODE -ne 0) {
        throw "Authenticode signing failed with exit code $LASTEXITCODE."
    }
    & $signTool verify /pa /all $output
    if ($LASTEXITCODE -ne 0) {
        throw "Authenticode verification failed with exit code $LASTEXITCODE."
    }
    Write-Host "Signed $output with $($certificate.Subject)"
} elseif ($RequireSigned) {
    throw 'A trusted Authenticode code-signing certificate is required for this build.'
} else {
    Write-Warning 'Built without Authenticode. Windows SmartScreen will show an unknown publisher until a trusted code-signing certificate is configured.'
}
Write-Host "Built $output"
