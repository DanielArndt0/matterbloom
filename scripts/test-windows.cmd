@echo off
setlocal EnableExtensions
cd /d "%~dp0.."

if exist "C:\TDM-GCC-64\bin\g++.exe" set "PATH=C:\TDM-GCC-64\bin;%PATH%"
where g++.exe >nul 2>nul
if errorlevel 1 (
  echo Compilador g++ nao encontrado.
  exit /b 1
)

if not exist "build-tests" mkdir "build-tests"
set "SOURCES=tests\MaterialDBFake.cpp src\WorldEngine.cpp src\WorldEnvironment.cpp src\WorldPhysics.cpp src\WorldArt.cpp src\Biology.cpp src\Chunk.cpp"

for %%T in (WorldSmokeTest NatureTest ChemistryTest PhysicsTest FlowAimUnitsTest StructuralLaboratoryTest StructuralDynamicsTest SimulationSettingsTest) do (
  echo Compilando %%T...
  g++ -std=c++17 -O2 -Itests\fakes -Isrc "tests\%%T.cpp" %SOURCES% -o "build-tests\%%T.exe"
  if errorlevel 1 exit /b 1
  "build-tests\%%T.exe"
  if errorlevel 1 exit /b 1
)

echo.
echo Testes da Matterbloom aprovados.
exit /b 0
