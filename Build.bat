@echo off

ctime -begin CTime.txt

set Name=vlog
REM set Core=..\src\vlog\*.cpp ..\src\vlog\reliances\*.cpp ..\src\vlog\common\*.cpp ..\src\vlog\backward\*.cpp ..\src\vlog\forward\*.cpp ..\src\vlog\magic\*.cpp ..\src\vlog\inmemory\*.cpp ..\src\vlog\trident\*.cpp ..\src\vlog\cycles\*.cpp ..\src\vlog\ml\*.cpp ..\src\vlog\deps\*.cpp ..\src\launcher\vloglayer.cpp ..\src\launcher\vlogscan.cpp
set Core=..\src\core.cpp
set Main=..\src\launcher\main.cpp

set ZLibPath=..\..\Libraries\zlib
set CurlPath=..\..\Libraries\curl
set Lz4Path=..\..\Libraries\lz4
set SparsehashPath=..\..\Libraries\sparsehash
set KognacPath=..\..\kognac
set TridentPath=..\..\trident

IF NOT EXIST build (
  mkdir build
) 

IF NOT EXIST build-core (
  mkdir build-core
) 

IF "%~1" == "clean" (
  echo Cleaning up...

  pushd build
    @del /q *.exe 2>NUL
    @del /q *.dll 2>NUL
    @del /q *.lib 2>NUL
    @del /q *.pdb 2>NUL
    @del /q *.ilk 2>NUL
    @del /q *.exp 2>NUL
    @del /q *.obj 2>NUL
  popd

  pushd build-core
    @del /q * 2>NUL
  popd

  echo Build folder clean

  goto end
) 

IF "%~1" == "release" (
  echo Setting release parameters

  set Optimization=-O2
  set Version=Release
  set VersionSuffix=""
  set CurlDll="libcurl.dll"
) ELSE (
  echo Setting debug parameters

  set Optimization=-Od
  set Version=Debug
  set VersionSuffix=d
  set CurlDll="libcurl-d.dll"
)

set CompilerFlags=-nologo -MP -Zi -EHsc -w -std:c11 -MD%VersionSuffix% %Optimization%
set LinkerFlags=
set Libraries=%ZLibPath%\lib\zlib%VersionSuffix%.lib %Lz4Path%\lib\lz4%VersionSuffix%.lib %CurlPath%\lib\libcurl%VersionSuffix%.lib %KognacPath%\win64\x64\%Version%\kognac-core.lib %TridentPath%\win64\x64\%Version%\trident-core.lib %TridentPath%\win64\x64\%Version%\trident-sparql.lib
set Includes=-I..\include -I%SparsehashPath%\src -I%ZLibPath%\include -I%Lz4Path%\include -I%CurlPath%\include -I%KognacPath%\include -I%TridentPath%\include -I%TridentPath%\rdf3x\include
set CoreDefines=-DVLOG_SHARED_LIB -DWIN32  -DNDEBUG -DVLOGCORE_EXPORTS -D_WINDOWS -D_USRDLL -D_WINDLL -D_MBCS
set Defines=-DWIN32 -DNDEBUG -D_CONSOLE -D_MBCS 

pushd build-core
  cl /bigobj /LD %CoreDefines% %Includes% %CompilerFlags% %Core% -Fe:%Name%-core.dll /link %LinkerFlags% %Libraries%

  @copy %KognacPath%\win64\x64\%Version%\kognac-core.dll ..\build\kognac-core.dll
  @copy %TridentPath%\win64\x64\%Version%\trident-core.dll ..\build\trident-core.dll
  @copy %TridentPath%\win64\x64\%Version%\trident-sparql.dll ..\build\trident-sparql.dll
  @copy %ZLibPath%\bin\zlib%VersionSuffix%1.dll ..\build\zlib%VersionSuffix%1.dll
  @copy %Lz4Path%\bin\lz4%VersionSuffix%.dll ..\build\lz4%VersionSuffix%.dll
  @copy %CurlPath%\bin\%CurlDll% ..\build\%CurlDll%
  @copy %Name%-core.dll ..\build\%Name%-core.dll
popd

pushd build
  cl %Defines% %Includes% %CompilerFlags% %Main% -Fe:%Name%.exe /link %LinkerFlags% %Libraries% ..\build-core\%Name%-core.lib
popd

:end

ctime -end CTime.txt