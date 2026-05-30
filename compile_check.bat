@echo off
setlocal
set INC=-IC:\Users\utkarsh_kumar\Desktop\sara\sara_agent\include -IC:\Users\utkarsh_kumar\Desktop\sara\shared -IC:\Users\utkarsh_kumar\Desktop\sara\plugins\spotify
set FLAGS=-DUNICODE -D_UNICODE -std=gnu++20

echo === Compiling main.cpp === >> C:\Users\utkarsh_kumar\Desktop\sara\compile_out.txt
C:\msys64\ucrt64\bin\g++.exe %FLAGS% %INC% -c C:\Users\utkarsh_kumar\Desktop\sara\sara_agent\src\main.cpp -o C:\Users\utkarsh_kumar\Desktop\sara\build\main_test.obj >> C:\Users\utkarsh_kumar\Desktop\sara\compile_out.txt 2>&1
echo main.cpp exit: %ERRORLEVEL% >> C:\Users\utkarsh_kumar\Desktop\sara\compile_out.txt

echo. >> C:\Users\utkarsh_kumar\Desktop\sara\compile_out.txt
echo === Compiling WinAPIExecutor.cpp === >> C:\Users\utkarsh_kumar\Desktop\sara\compile_out.txt
C:\msys64\ucrt64\bin\g++.exe %FLAGS% %INC% -c C:\Users\utkarsh_kumar\Desktop\sara\sara_agent\src\WinAPIExecutor.cpp -o C:\Users\utkarsh_kumar\Desktop\sara\build\winapi_test.obj >> C:\Users\utkarsh_kumar\Desktop\sara\compile_out.txt 2>&1
echo WinAPIExecutor.cpp exit: %ERRORLEVEL% >> C:\Users\utkarsh_kumar\Desktop\sara\compile_out.txt

echo. >> C:\Users\utkarsh_kumar\Desktop\sara\compile_out.txt
echo === Done === >> C:\Users\utkarsh_kumar\Desktop\sara\compile_out.txt
