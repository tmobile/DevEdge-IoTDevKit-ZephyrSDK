@echo off

rem adds any out of tree drivers built as modules to the zephyr build path
set LOCATION=%~dp0
if "%ZEPHYR_EXTRA_MODULES%" == "" (
    set ZEPHYR_EXTRA_MODULES=%LOCATION%
) else (
    set ZEPHYR_EXTRA_MODULES=%ZEPHYR_EXTRA_MODULES%;%LOCATION%
)

echo %ZEPHYR_EXTRA_MODULES%
