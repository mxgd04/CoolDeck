@echo off
echo Cerrando CoolDeck bridge (libera el puerto COM para subir firmware)...
powershell -NoProfile -Command "$p = Get-CimInstance Win32_Process | Where-Object { $_.CommandLine -like '*streamdeck.py*' -and $_.ProcessId -ne $PID }; if ($p) { $p | ForEach-Object { Stop-Process -Id $_.ProcessId -Force }; 'Cerrado.' } else { 'No estaba en marcha.' }"
pause
