<# Builds the x64 static Boost libraries used by hMailServer. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory=$false)][ValidatePattern('^1\.\d+\.\d+$')][string]$Version='1.92.0',
    [Parameter(Mandatory=$false)][string]$Toolset='msvc-14.2',
    [Parameter(Mandatory=$false)][int]$Jobs=[int]$env:NUMBER_OF_PROCESSORS
)

$ErrorActionPreference='Stop'
$PSNativeCommandUseErrorActionPreference=$false
. (Join-Path $PSScriptRoot 'build-common.ps1')

$log=Join-Path $PSScriptRoot 'build-boost.log'
Start-BuildLog -LogPath $log -Title "Boost $Version build log"
$libs=Get-LibrariesPath
$underscored=$Version -replace '\.','_'
$src=Join-Path $libs "boost_$underscored"
if ($Jobs -lt 1) { $Jobs=4 }

$range = switch -Regex ($Toolset) {
    '^msvc-14\.2$' { '[16.0,17.0)'; break }
    '^msvc-14\.3$' { '[17.0,18.0)'; break }
    default { $null }
}
$vcvars=Resolve-VcVars64 -VersionRange $range
Get-SourceArchive -Url "https://archives.boost.io/release/$Version/source/boost_$underscored.tar.gz" -SrcDir $src -LibsPath $libs
Import-VsEnvironment -VcVars64 $vcvars
Remove-Item Env:\NoDefaultCurrentDirectoryInExePath -ErrorAction SilentlyContinue

Push-Location $src
try {
    Invoke-BuildStep 'Bootstrapping Boost b2' { cmd /c 'bootstrap.bat' }
    if ($LASTEXITCODE -ne 0) { throw "Boost bootstrap failed: $LASTEXITCODE" }

    Invoke-BuildStep 'Building Boost x64 static libraries' {
        .\b2 debug release threading=multi link=static `
            --with-thread --with-filesystem --with-regex --with-chrono --with-system --with-atomic `
            --toolset=$Toolset address-model=64 stage --build-dir=out64 -j $Jobs
    }
    if ($LASTEXITCODE -ne 0) { throw "Boost build failed: $LASTEXITCODE" }
}
finally { Pop-Location }

$stage=Join-Path $src 'stage\lib'
if (!(Test-Path (Join-Path $src 'boost'))) { throw 'Boost headers were not produced.' }
if (!(Test-Path $stage)) { throw 'Boost stage\lib was not produced.' }
foreach($lib in @('thread','filesystem','regex','chrono','atomic')) {
    if (!(Get-ChildItem $stage -Filter "*boost_$lib-*.lib" -ErrorAction SilentlyContinue)) {
        throw "No staged library was found for boost_$lib"
    }
}
Write-Log "Boost $Version built successfully."
