@echo off
rem HelixSR GPU Lab: runs tools\gpulab.py with Python 3.10+ (downloads the official embeddable Python if none is found).
rem   gpulab.bat status | on GAME | off GAME | install | uninstall
setlocal
set "PY="
py -3 -c "import sys; sys.exit(0 if sys.version_info >= (3, 10) else 1)" >nul 2>nul && set "PY=py -3"
if not defined PY python -c "import sys; sys.exit(0 if sys.version_info >= (3, 10) else 1)" >nul 2>nul && set "PY=python"
if not defined PY if exist "%LOCALAPPDATA%\HelixSR-GPU-Lab\python\python.exe" set PY="%LOCALAPPDATA%\HelixSR-GPU-Lab\python\python.exe"
if not defined PY powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\get_python.ps1" && set PY="%LOCALAPPDATA%\HelixSR-GPU-Lab\python\python.exe"
if not defined PY echo Could not find or download Python. Install Python 3.10 or newer from python.org and try again.
if not defined PY exit /b 1
%PY% -B "%~dp0tools\gpulab.py" %*
