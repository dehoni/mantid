@echo off
::
:: Sets up the environment configured for Mantid by CMake and
:: starts the appropriate version of Visual Studio

:: Assume the buildenv.bat script exists and is in the same directory
call %~dp0buildenv.bat
set VCVARS=@MSVC_VAR_LOCATION@
:: Start command line
%COMSPEC% /k ""%VCVARS%\vcvarsall.bat"" amd64
