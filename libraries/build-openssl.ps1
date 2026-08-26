<# Builds OpenSSL 3.5.x into the layout used by hMailServer. #>
[CmdletBinding()]
param([Parameter(Mandatory=$false)][ValidatePattern('^3\.5\.\d+$')][string]$Version='3.5.8')

$ErrorActionPreference='Stop'
$PSNativeCommandUseErrorActionPreference=$false
. (Join-Path $PSScriptRoot 'build-common.ps1')

$log=Join-Path $PSScriptRoot 'build-openssl.log'
Start-BuildLog -LogPath $log -Title "OpenSSL $Version build log"
$libs=Get-LibrariesPath
$src=Join-Path $libs "openssl-$Version"
$out=Join-Path $src 'out64'
$vcvars=Resolve-VcVars64

if ($null -eq (Get-Command perl -ErrorAction SilentlyContinue)) {
    throw 'Perl was not found on PATH. OpenSSL Configure requires Perl.'
}

Get-SourceArchive -Url "https://www.openssl.org/source/openssl-$Version.tar.gz" -SrcDir $src -LibsPath $libs
Import-VsEnvironment -VcVars64 $vcvars
$env:CFLAGS='-DOPENSSL_TLS_SECURITY_LEVEL=0'

Push-Location $src
try {
    Invoke-BuildStep "Configuring OpenSSL $Version" {
        perl Configure no-asm VC-WIN64A "--prefix=$out" "--openssldir=$out" -D_WIN32_WINNT=0x600 --api=1.1.1 no-deprecated
    }
    if ($LASTEXITCODE -ne 0) { throw "OpenSSL Configure failed: $LASTEXITCODE" }
    Invoke-BuildStep 'Cleaning OpenSSL build' { nmake clean }
    if ($LASTEXITCODE -ne 0) { throw "OpenSSL clean failed: $LASTEXITCODE" }
    Invoke-BuildStep 'Building and installing OpenSSL' { nmake install_sw }
    if ($LASTEXITCODE -ne 0) { throw "OpenSSL build failed: $LASTEXITCODE" }
}
finally { Pop-Location }

foreach($item in @('bin\libcrypto-3-x64.dll','bin\libssl-3-x64.dll','include','lib')) {
    $path=Join-Path $out $item
    if (!(Test-Path $path)) { throw "Expected OpenSSL output is missing: $path" }
}
Write-Log "OpenSSL $Version built successfully into $out"
