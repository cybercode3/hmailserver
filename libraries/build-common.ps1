<#
.SYNOPSIS
    Shared helpers for the hMailServer third-party library build scripts.
.DESCRIPTION
    Provides UTF-8 build logging, Visual Studio 2019 x64 environment discovery,
    hMailServerLibs validation and clean source archive download/extraction.
#>

$script:BuildLogEncoding = 'UTF8'
$script:BuildLogPath = $null

function Start-BuildLog {
    param([Parameter(Mandatory=$true)][string]$LogPath,
          [Parameter(Mandatory=$true)][string]$Title)
    $script:BuildLogPath = $LogPath
    Set-Content -Path $LogPath -Value "$Title - started $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')" -Encoding $script:BuildLogEncoding
}

function Write-Log {
    param([string]$Message)
    Write-Host $Message
    Add-Content -Path $script:BuildLogPath -Value $Message -Encoding $script:BuildLogEncoding
}

function Invoke-BuildStep {
    param([string]$Description,[scriptblock]$Command)
    Write-Log $Description
    Add-Content -Path $script:BuildLogPath -Value "----- $Description -----" -Encoding $script:BuildLogEncoding

    $previous = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    $writer = New-Object System.IO.StreamWriter($script:BuildLogPath, $true, (New-Object System.Text.UTF8Encoding($false)))
    $writer.AutoFlush = $true
    try {
        & $Command 2>&1 | ForEach-Object {
            if ($_ -is [System.Management.Automation.ErrorRecord]) {
                $line = $_.Exception.Message
            } else {
                $line = [string]$_
            }
            Write-Host $line
            $writer.WriteLine($line)
        }
    }
    finally {
        $writer.Close()
        $ErrorActionPreference = $previous
    }
}

function Get-LibrariesPath {
    $path = $env:hMailServerLibs
    if ([string]::IsNullOrWhiteSpace($path)) {
        throw 'The environment variable hMailServerLibs was not found. Please create it.'
    }
    if (!(Test-Path $path)) {
        throw "The hMailServerLibs folder does not exist: $path"
    }
    return $path
}

function Resolve-VcVars64 {
    param([string]$VersionRange='[16.0,17.0)')

    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (!(Test-Path $vswhere)) {
        throw "vswhere.exe was not found at $vswhere. Install Visual Studio 2019 with the x64 C++ tools."
    }

    $args = @('-products','*','-requires','Microsoft.VisualStudio.Component.VC.Tools.x86.x64','-property','installationPath')
    if ($VersionRange) { $args += @('-version',$VersionRange) } else { $args = @('-latest') + $args }

    $install = & $vswhere @args | Select-Object -First 1
    if ([string]::IsNullOrWhiteSpace($install)) {
        throw 'No compatible Visual Studio C++ x64 toolchain was found.'
    }

    $vcvars = Join-Path $install 'VC\Auxiliary\Build\vcvars64.bat'
    if (!(Test-Path $vcvars)) { throw "vcvars64.bat was not found at $vcvars" }
    return $vcvars
}

function Import-VsEnvironment {
    param([Parameter(Mandatory=$true)][string]$VcVars64)

    Write-Log 'Importing the Visual Studio x64 build environment'
    $output = cmd /c "call `"$VcVars64`" >nul 2>&1 && set"
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to initialize the Visual Studio x64 environment through $VcVars64"
    }

    foreach ($line in $output) {
        if ($line -match '^([^=]+)=(.*)$') {
            Set-Item -Path "Env:\$($matches[1])" -Value $matches[2]
        }
    }
}

function Get-SourceArchive {
    param([Parameter(Mandatory=$true)][string]$Url,
          [Parameter(Mandatory=$true)][string]$SrcDir,
          [Parameter(Mandatory=$true)][string]$LibsPath)

    if (Test-Path $SrcDir) {
        Write-Log "Removing existing source folder $SrcDir for a clean build"
        Remove-Item -LiteralPath $SrcDir -Recurse -Force
    }

    $tarPath = Join-Path $LibsPath (Split-Path -Leaf $Url)
    Write-Log "Downloading $Url"
    Invoke-WebRequest -Uri $Url -OutFile $tarPath

    $tarExe = Join-Path $env:SystemRoot 'System32\tar.exe'
    if (!(Test-Path $tarExe)) { throw "Windows tar.exe was not found at $tarExe" }

    Write-Log "Extracting $tarPath"
    & $tarExe -xzf $tarPath -C $LibsPath
    if ($LASTEXITCODE -ne 0) { throw "Extraction failed with exit code $LASTEXITCODE" }
    Remove-Item $tarPath -Force

    if (!(Test-Path $SrcDir)) { throw "Expected source folder was not created: $SrcDir" }
}
