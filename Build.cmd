@echo off
setlocal EnableExtensions DisableDelayedExpansion
cd /d "%~dp0"

call :find_cmake
if errorlevel 1 (
  echo CMake nao encontrado. Tentando preparar o ambiente...
  call :install_tool CMake Kitware.CMake "https://cmake.org/download/"
  if errorlevel 1 goto failed
)

call :find_git
if errorlevel 1 (
  echo Git nao encontrado. Tentando preparar o ambiente...
  call :install_tool Git Git.Git "https://git-scm.com/download/win"
  if errorlevel 1 goto failed
)

echo CMake e Git encontrados.

if exist "C:\TDM-GCC-64\bin\g++.exe" (
  set "PATH=C:\TDM-GCC-64\bin;%PATH%"
)

where g++.exe >nul 2>nul
if not errorlevel 1 (
  where mingw32-make.exe >nul 2>nul
  if not errorlevel 1 (
    set "GENERATOR=MinGW Makefiles"
    set "BUILD_DIR=build-mingw"
    goto configure
  )
)

where cl.exe >nul 2>nul
if not errorlevel 1 (
  where nmake.exe >nul 2>nul
  if not errorlevel 1 (
    set "GENERATOR=NMake Makefiles"
    set "BUILD_DIR=build-nmake"
    goto configure
  )
)

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" (
  for /f "usebackq delims=" %%I in (`"%VSWHERE%" -latest -products * -version "[17.0,18.0)" -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VS_INSTALL=%%I"
  if defined VS_INSTALL (
    cmake --help | findstr /C:"Visual Studio 17 2022" >nul
    if not errorlevel 1 (
      set "GENERATOR=Visual Studio 17 2022"
      set "BUILD_DIR=build-vs2022"
      goto configure_vs
    )
  )
)

where ninja.exe >nul 2>nul
if not errorlevel 1 (
  where g++.exe >nul 2>nul
  if not errorlevel 1 (
    set "GENERATOR=Ninja"
    set "BUILD_DIR=build-ninja"
    goto configure
  )
)

echo.
echo Nenhum compilador C++ compativel foi encontrado.
echo Instale o Visual Studio Build Tools com a carga de trabalho
echo "Desenvolvimento para desktop com C++" ou instale MinGW-w64.
echo Download: https://visualstudio.microsoft.com/visual-cpp-build-tools/
start "" "https://visualstudio.microsoft.com/visual-cpp-build-tools/"
goto failed

:configure
echo Usando %GENERATOR% em %BUILD_DIR%.
cmake -S . -B "%BUILD_DIR%" -G "%GENERATOR%" -DCMAKE_BUILD_TYPE=Release "-DCMAKE_POLICY_VERSION_MINIMUM=3.5"
goto build

:configure_vs
echo Usando %GENERATOR% em %BUILD_DIR%.
cmake -S . -B "%BUILD_DIR%" -G "%GENERATOR%" -A x64 "-DCMAKE_POLICY_VERSION_MINIMUM=3.5"

:build
if errorlevel 1 (
  echo Falha na configuracao.
  pause
  exit /b 1
)
cmake --build "%BUILD_DIR%" --config Release --parallel 4
if errorlevel 1 (
  echo Falha na compilacao.
  pause
  exit /b 1
)
echo.
echo Compilacao concluida. Procure Matterbloom.exe em %BUILD_DIR%.
pause
exit /b 0

:find_cmake
where.exe cmake.exe >nul 2>nul
if not errorlevel 1 exit /b 0
for %%D in ("%ProgramFiles%\CMake\bin" "%ProgramFiles(x86)%\CMake\bin" "%LOCALAPPDATA%\Programs\CMake\bin" "%USERPROFILE%\scoop\apps\cmake\current\bin" "%ProgramData%\chocolatey\bin" "%LOCALAPPDATA%\Microsoft\WinGet\Links") do (
  if exist "%%~D\cmake.exe" (
    set "PATH=%%~D;%PATH%"
    exit /b 0
  )
)
exit /b 1

:find_git
where.exe git.exe >nul 2>nul
if not errorlevel 1 exit /b 0
for %%D in ("%ProgramFiles%\Git\cmd" "%ProgramFiles(x86)%\Git\cmd" "%LOCALAPPDATA%\Programs\Git\cmd" "%USERPROFILE%\scoop\apps\git\current\cmd" "%ProgramData%\chocolatey\bin" "%LOCALAPPDATA%\Microsoft\WinGet\Links") do (
  if exist "%%~D\git.exe" (
    set "PATH=%%~D;%PATH%"
    exit /b 0
  )
)
exit /b 1

:install_tool
where.exe winget.exe >nul 2>nul
if errorlevel 1 (
  echo WinGet nao esta disponivel neste computador.
  goto install_manually
)

echo Instalando %~1 pelo WinGet. O Windows pode solicitar permissao...
winget.exe install --id "%~2" --exact --source winget --silent --accept-package-agreements --accept-source-agreements
if errorlevel 1 (
  echo Nao foi possivel instalar %~1 automaticamente.
  goto install_manually
)

call :refresh_path
if /I "%~1"=="CMake" (
  call :find_cmake
) else (
  call :find_git
)
if not errorlevel 1 (
  echo %~1 instalado e localizado com sucesso.
  exit /b 0
)

echo A instalacao de %~1 terminou, mas o programa ainda nao foi localizado.
echo Feche o terminal, abra um novo PowerShell e execute este script novamente.
exit /b 1

:refresh_path
where.exe powershell.exe >nul 2>nul
if errorlevel 1 exit /b 0
for /f "usebackq delims=" %%P in (`powershell.exe -NoProfile -Command "[Environment]::GetEnvironmentVariable('Path','Machine') + ';' + [Environment]::GetEnvironmentVariable('Path','User')"`) do set "PATH=%%P;%PATH%"
exit /b 0

:install_manually
echo Download oficial de %~1: %~3
echo O navegador sera aberto para a instalacao manual.
start "" "%~3"
exit /b 1

:failed
echo.
echo A compilacao nao foi iniciada. Verifique a mensagem acima.
pause
exit /b 1
