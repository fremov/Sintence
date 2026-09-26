@echo off
rem Пульт сбора матчей Sintence: двойной щелчок — и он откроется в браузере.
rem Первый запуск ставит зависимости интерфейса (npm install), каждый —
rem пересобирает его (пара секунд), потом поднимает сервер пульта
rem crawler_dashboard\server.py на http://127.0.0.1:8790.
rem Закрыть это окно — остановить пульт и идущий сбор.
chcp 65001 >nul
title Sintence - пульт сбора матчей
cd /d "%~dp0"

where python >nul 2>nul
if errorlevel 1 goto no_python

where npm >nul 2>nul
if errorlevel 1 goto no_node

pushd crawler_dashboard\web
if not exist node_modules\ call npm install --no-fund --no-audit
if errorlevel 1 goto build_failed
call npm run build --silent
if errorlevel 1 goto build_failed
popd
goto run

:no_node
if exist crawler_dashboard\web\dist\index.html goto run_prebuilt
echo Нужен Node.js 20+ (https://nodejs.org), чтобы собрать интерфейс пульта.
pause
exit /b 1

:run_prebuilt
echo Node.js не найден - запускаю ранее собранный интерфейс.
goto run

:build_failed
popd
echo Интерфейс пульта не собрался - подробности выше.
pause
exit /b 1

:no_python
echo Нужен Python 3.11+ (https://python.org), с галочкой "Add to PATH".
pause
exit /b 1

:run
echo.
echo Пульт: http://127.0.0.1:8790 - откроется в браузере.
echo Закройте это окно, чтобы остановить пульт и идущий сбор.
echo.
python crawler_dashboard\server.py %*
if errorlevel 1 pause
