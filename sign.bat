@echo off

if exist "bin\release\sign.exe" (
	set sign="bin\release\sign.exe"
) else (
if exist "bin\debug\sign.exe" (
	set sign="bin\release\sign.exe"
) else (
	echo ȱ��ǩ���������
	goto quit
)
)

if exist "bin\release\start.dll" (
	echo ǩ��release�汾
	set start="bin\release\start.dll"
) else (
if exist "bin\debug\start.dll" (
	echo ǩ��debug�汾
	set start="bin\debug\start.dll"
) else (
	echo ȱ��ǩ�������Դ
	goto quit
)
)

copy %start% assets /Y > nul
cd assets
..\%sign% -m start.dll sence.jpg progress1.png progress2.png -o ..\assets.zip
del start.dll
cd ..
echo ǩ�����

:quit