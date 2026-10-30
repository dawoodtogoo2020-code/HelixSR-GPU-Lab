# SPDX-License-Identifier: MIT
# Used by gpulab.bat only when no Python 3.10+ is installed: downloads the official embeddable Python from python.org
# (the same pinned build HelixSR 1.8.1's setup uses) into %LOCALAPPDATA%\HelixSR-GPU-Lab\python. No installer, no PATH
# change, no admin rights. Delete that folder to remove it.
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$Url = 'https://www.python.org/ftp/python/3.14.8/python-3.14.8-embed-amd64.zip'
$Sha = 'a93abe456ab01bd96d7a085b3cdb6566b3063f4241360d114142fbdb07f0a310'
$Data = Join-Path $env:LOCALAPPDATA 'HelixSR-GPU-Lab'
$Py = Join-Path $Data 'python\python.exe'
if (Test-Path $Py) { exit 0 }
New-Item -ItemType Directory -Force -Path $Data | Out-Null
$Zip = Join-Path $Data 'python.zip'
Write-Host 'Downloading Python from python.org (one time, about 11 MB)...'
Invoke-WebRequest -Uri $Url -OutFile $Zip -UseBasicParsing
if ((Get-FileHash -Algorithm SHA256 $Zip).Hash.ToLower() -ne $Sha) {
    Remove-Item $Zip -Force
    throw 'The Python download did not match its checksum; nothing was installed.'
}
Expand-Archive -Path $Zip -DestinationPath (Join-Path $Data 'python') -Force
Remove-Item $Zip -Force
