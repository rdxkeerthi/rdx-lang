@echo off
setlocal enabledelayedexpansion
set PASS=0
set FAIL=0
set RDX=.\rdx_bin.exe

echo ============================================================
echo  RDX Step 2 + 3: Structs and Arrays Test Suite
echo ============================================================

:: ----- Step 2: Struct Tests -----
echo.
echo [Step 2: Structs]

%RDX% examples\sensor_reading.rdx 2>&1 | findstr /C:"Sensor ID: 1" >nul
if %errorlevel%==0 (echo [PASS] struct field read - sensor id & set /a PASS+=1) else (echo [FAIL] struct field read - sensor id & set /a FAIL+=1)

%RDX% examples\sensor_reading.rdx 2>&1 | findstr /C:"Temperature: 23.5" >nul
if %errorlevel%==0 (echo [PASS] struct flt field & set /a PASS+=1) else (echo [FAIL] struct flt field & set /a FAIL+=1)

%RDX% examples\sensor_reading.rdx 2>&1 | findstr /C:"Updated sensor temp: 98.6" >nul
if %errorlevel%==0 (echo [PASS] struct field mutation & set /a PASS+=1) else (echo [FAIL] struct field mutation & set /a FAIL+=1)

%RDX% examples\sensor_reading.rdx 2>&1 | findstr /C:"100 Celsius = 212.0" >nul
if %errorlevel%==0 (echo [PASS] celsius_to_fahrenheit function & set /a PASS+=1) else (echo [FAIL] celsius_to_fahrenheit function & set /a FAIL+=1)

%RDX% examples\sensor_reading.rdx 2>&1 | findstr /C:"P1 x: 0" >nul
if %errorlevel%==0 (echo [PASS] Point struct field p1.x & set /a PASS+=1) else (echo [FAIL] Point struct field p1.x & set /a FAIL+=1)

%RDX% examples\sensor_reading.rdx 2>&1 | findstr /C:"P2 x: 3" >nul
if %errorlevel%==0 (echo [PASS] Point struct field p2.x & set /a PASS+=1) else (echo [FAIL] Point struct field p2.x & set /a FAIL+=1)

:: ----- Step 3: Array Tests -----
echo.
echo [Step 3: Arrays]

%RDX% examples\arrays_demo.rdx 2>&1 | findstr /C:"First prime: 2" >nul
if %errorlevel%==0 (echo [PASS] array literal index 0 & set /a PASS+=1) else (echo [FAIL] array literal index 0 & set /a FAIL+=1)

%RDX% examples\arrays_demo.rdx 2>&1 | findstr /C:"Fifth prime: 11" >nul
if %errorlevel%==0 (echo [PASS] array literal index 4 & set /a PASS+=1) else (echo [FAIL] array literal index 4 & set /a FAIL+=1)

%RDX% examples\arrays_demo.rdx 2>&1 | findstr /C:"Updated scores[2]: 999" >nul
if %errorlevel%==0 (echo [PASS] array element mutation & set /a PASS+=1) else (echo [FAIL] array element mutation & set /a FAIL+=1)

%RDX% examples\arrays_demo.rdx 2>&1 | findstr /C:"  3" >nul
if %errorlevel%==0 (echo [PASS] array loop iteration & set /a PASS+=1) else (echo [FAIL] array loop iteration & set /a FAIL+=1)

%RDX% examples\arrays_demo.rdx 2>&1 | findstr /C:"  13" >nul
if %errorlevel%==0 (echo [PASS] array loop last element & set /a PASS+=1) else (echo [FAIL] array loop last element & set /a FAIL+=1)

%RDX% examples\arrays_demo.rdx 2>&1 | findstr /C:"word0: hello" >nul
if %errorlevel%==0 (echo [PASS] string array index 0 & set /a PASS+=1) else (echo [FAIL] string array index 0 & set /a FAIL+=1)

%RDX% examples\arrays_demo.rdx 2>&1 | findstr /C:"word2: rdx" >nul
if %errorlevel%==0 (echo [PASS] string array index 2 & set /a PASS+=1) else (echo [FAIL] string array index 2 & set /a FAIL+=1)

:: ----- Summary -----
echo.
echo ============================================================
echo  Results: %PASS% passed, %FAIL% failed
echo ============================================================
if %FAIL%==0 (echo  All tests PASSED! && exit /b 0) else (echo  Some tests FAILED! && exit /b 1)
