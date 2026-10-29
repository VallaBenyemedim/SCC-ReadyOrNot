@echo off
setlocal
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
pushd "%~dp0.."
if not exist x64\Tests mkdir x64\Tests
cl /nologo /std:c++20 /EHsc /W4 /utf-8 /Fo:x64\Tests\esp_presentation_test.obj /Fe:x64\Tests\esp_presentation_test.exe test\esp_presentation_test.cpp
if errorlevel 1 exit /b 1
x64\Tests\esp_presentation_test.exe
set taskTestResult=%errorlevel%
popd
exit /b %taskTestResult%
