@echo off

if not defined DevEnvDir (
    call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
)

set opts=
rem set cl_opts=-FC -GR- -EHa- -nologo -Zi -Fetest %opts%
set cl_opts=/I "%cd%\..\common\include"
set code=%cd%\source
pushd build
cl %cl_opts% %code%\main.c
popd
