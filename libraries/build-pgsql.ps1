<#
.SYNOPSIS
    Builds libpq from a specific PostgreSQL version for hMailServer.

.DESCRIPTION
    Downloads the PostgreSQL source for the requested version into
    %hMailServerLibs%\postgresql-<Version>, generates the src\tools\msvc\config.pl
    that links libpq against a previously built OpenSSL, and builds libpq with the
    VS2019 x64 toolchain (perl build.pl Release libpq). The result is the layout
    hMailServer links against: postgresql-<Version>\Release\libpq (libpq.dll /
    libpq.lib) plus the libpq-fe.h header under src\interfaces\libpq.

    Only PostgreSQL versions that ship the src\tools\msvc\build.pl MSVC build system
    are supported, i.e. 15.x and 16.x. PostgreSQL 17 removed that system in favour of
    Meson and is intentionally out of scope.

.PARAMETER Version
    The PostgreSQL version to build, e.g. 15.19. Must match 15.x or 16.x.

.PARAMETER OpenSSLVersion
    The OpenSSL version to link libpq against. If omitted, the script auto-detects it
    from hMailServer.vcxproj.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^1[56]\.\d+$')]
    [string]$Version,

    [Parameter(Mandatory = $false)]
    [ValidatePattern('^\d+\.\d+\.\d+$')]
    [string]$OpenSSLVersion
)

$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false
. (Join-Path -Path $PSScriptRoot -ChildPath 'build-common.ps1')

$logPath = Join-Path -Path $PSScriptRoot -ChildPath 'build-pgsql.log'
Start-BuildLog -LogPath $logPath -Title "PostgreSQL $Version (libpq) build log"

$libsPath = Get-LibrariesPath
$srcDir = Join-Path -Path $libsPath -ChildPath "postgresql-$Version"
$msvcDir = Join-Path -Path $srcDir -ChildPath 'src\tools\msvc'

if ([string]::IsNullOrEmpty($OpenSSLVersion))
{
    $vcxproj = Join-Path -Path $PSScriptRoot -ChildPath '..\hmailserver\source\Server\hMailServer\hMailServer.vcxproj'
    if (!(Test-Path $vcxproj))
    {
        throw "OpenSSLVersion was not supplied and hMailServer.vcxproj was not found at $vcxproj. Pass -OpenSSLVersion explicitly."
    }

    $match = Select-String -Path $vcxproj -Pattern 'openssl-(\d+\.\d+\.\d+)' | Select-Object -First 1
    if ($null -eq $match)
    {
        throw "Could not auto-detect the OpenSSL version from $vcxproj. Pass -OpenSSLVersion explicitly."
    }

    $OpenSSLVersion = $match.Matches[0].Groups[1].Value
    Write-Log "Auto-detected OpenSSL version $OpenSSLVersion from hMailServer.vcxproj"
}

$openSslOut = Join-Path -Path $libsPath -ChildPath "openssl-$OpenSSLVersion\out64"
if (!(Test-Path $openSslOut))
{
    throw "The OpenSSL build to link libpq against was not found at $openSslOut. Build it first with build-openssl.ps1 -Version $OpenSSLVersion."
}

$vcvars64 = Resolve-VcVars64
if ($null -eq (Get-Command perl -ErrorAction SilentlyContinue))
{
    throw "Perl was not found on PATH. PostgreSQL's build.pl requires Perl (e.g. Strawberry Perl)."
}

$tarUrl = "https://ftp.postgresql.org/pub/source/v$Version/postgresql-$Version.tar.gz"
Get-SourceArchive -Url $tarUrl -SrcDir $srcDir -LibsPath $libsPath

if (!(Test-Path $msvcDir))
{
    throw "The MSVC build folder $msvcDir was not found. PostgreSQL $Version may not ship src\tools\msvc\build.pl. Only 15.x and 16.x are supported."
}

$openSslPerl = $openSslOut -replace '\\', '\\'
$configPl = @"
use strict;
use warnings;

our `$config = {
    # Target Windows Vista so libpq does not statically import
    # GetSystemTimePreciseAsFileTime (unavailable before Windows 8).
    cflags  => '/D_WIN32_WINNT=0x0600',
    # Link libpq against the OpenSSL built by build-openssl.ps1.
    openssl => '$openSslPerl',
};

1;
"@

$configPlPath = Join-Path -Path $msvcDir -ChildPath 'config.pl'
Write-Log "Writing $configPlPath (linking libpq against $openSslOut)"
Set-Content -Path $configPlPath -Value $configPl -Encoding UTF8

Import-VsEnvironment -VcVars64 $vcvars64

Write-Log "Building libpq from PostgreSQL $Version"
Push-Location $msvcDir
try
{
    Invoke-BuildStep 'Compiling libpq (perl build.pl Release libpq)' {
        perl build.pl Release libpq
    }
    if ($LastExitCode -ne 0)
    {
        throw "PostgreSQL 'perl build.pl Release libpq' failed with exit code $LastExitCode. See $logPath for details."
    }
}
finally
{
    Pop-Location
}

$expected = @(
    (Join-Path -Path $srcDir -ChildPath 'Release\libpq\libpq.dll'),
    (Join-Path -Path $srcDir -ChildPath 'Release\libpq\libpq.lib'),
    (Join-Path -Path $srcDir -ChildPath 'src\interfaces\libpq\libpq-fe.h')
)

foreach ($item in $expected)
{
    if (!(Test-Path $item))
    {
        throw "Build completed but expected output was missing: $item"
    }
}

Write-Log "libpq from PostgreSQL $Version built successfully into $(Join-Path -Path $srcDir -ChildPath 'Release\libpq')"
