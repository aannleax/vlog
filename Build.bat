@echo off

ctime -begin CTime.txt

set Name=vlog
set Core=..\src\vlog\*.cpp ..\src\vlog\common\*.cpp ..\src\vlog\backward\*.cpp ..\src\vlog\forward\*.cpp ..\src\vlog\magic\*.cpp ..\src\vlog\inmemory\*.cpp ..\src\vlog\trident\*.cpp ..\src\vlog\cycles\*.cpp ..\src\vlog\ml\*.cpp ..\src\vlog\deps\*.cpp ..\src\launcher\vloglayer.cpp ..\src\launcher\vlogscan.cpp
set Main=..\src\launcher\main.cpp

set ZLibPath=..\..\zlib
set CurlPath=..\..\curl
set Lz4Path=..\..\lz4
set SparsehashPath=..\..\sparsehash
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
    @del /q * 2>NUL
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
) ELSE (
  echo Setting debug parameters

  set Optimization=-Od
  set Version=Debug
  set VersionSuffix=d
)

set CompilerFlags=-nologo -Zi -EHsc -W0 -std:c11 -MT%VersionSuffix% %Optimization%
set LinkerFlags=
set Libraries=%ZLibPath%\lib\zlib%VersionSuffix%.lib %Lz4Path%\lib\lz4%VersionSuffix%.lib %CurlPath%\lib\libcurl%VersionSuffix%.lib %KognacPath%\win64\x64\%Version%\kognac-core.lib %TridentPath%\win64\x64\%Version%\trident-core.lib %TridentPath%\win64\x64\%Version%\trident-sparql.lib
set Includes=-I..\include -I%SparsehashPath%\src -I%ZLibPath%\include -I%Lz4Path%\include -I%CurlPath%\include -I%KognacPath%\include -I%TridentPath%\include -I%TridentPath%\rdf3x\include
set CoreDefines=-DVLOG_SHARED_LIB -DWIN32 -DCURL_STATICLIB -DNDEBUG -DVLOGCORE_EXPORTS -D_WINDOWS -D_USRDLL -D_WINDLL -D_MBCS
set Defines=-DWIN32 -DNDEBUG -D_CONSOLE -D_MBCS 

pushd build-core
  cl /LD %CoreDefines% %Includes% %CompilerFlags% %Core% -Fe:%Name%-core.dll /link %LinkerFlags% %Libraries%

  @copy %KognacPath%\win64\x64\%Version%\kognac-core.dll ..\build\kognac-core.dll
  @copy %TridentPath%\win64\x64\%Version%\trident-core.dll ..\build\trident-core.dll
  @copy %TridentPath%\win64\x64\%Version%\trident-sparql.dll ..\build\trident-sparql.dll
  @copy %Name%-core.dll ..\build\%Name%-core.dll
popd

pushd build
  cl %Defines% %Includes% %CompilerFlags% %Main% -Fe:%Name%.exe /link %LinkerFlags% %Libraries% ..\build-core\%Name%-core.lib
popd

:end

ctime -end CTime.txt