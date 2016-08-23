@echo off

if exist "bin\release\sign.exe" (
	set sign=bin\release\sign.exe
) else (
if exist "bin\debug\sign.exe" (
	set sign=bin\debug\sign.exe
) else (
	echo 缺少签名打包程序
	goto quit
)
)

if exist "bin\release\start.dll" (
	echo 签名release版本
	set start=bin\release\start.dll
) else (
if exist "bin\debug\start.dll" (
	echo 签名debug版本
	set start=bin\debug\start.dll
) else (
	echo 缺少签名打包资源
	goto quit
)
)

copy "%start%" assets /Y > nul
cd assets
..\%sign% -i start.dll sence.jpg progress1.png progress2.png -o ..\assets.zip -m "89EF9A73-72CC-4DB0-8D48-CB770EA6E67A"
del start.dll
cd ..
echo 签名完成

:quit