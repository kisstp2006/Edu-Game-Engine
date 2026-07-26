# EDU Game Engine
Simple 3D game engine for educational purposes

<a href="https://scan.coverity.com/projects/d0n3val-edu-game-engine">
  <img alt="Coverity Scan Build Status"
       src="https://scan.coverity.com/projects/9706/badge.svg"/>
</a>
[![Build status](https://ci.appveyor.com/api/projects/status/p6ihib1lnw7ulyq1?svg=true)](https://ci.appveyor.com/project/d0n3val/edu-game-engine)

## Usage

Download the code and play with it to learn, there is no formal installation process.

## CMake build (Windows x64)

Requirements:

- CMake 3.24 or newer
- Visual Studio 2022 with the **Desktop development with C++** workload
- Git

The root build helper automatically detects Visual Studio 2022 or 2026,
configures CMake, downloads the pinned dependencies, and builds the engine:

```powershell
build.bat
```

Common commands:

```powershell
build.bat Release
build.bat Debug vs2026 generate
build.bat Release vs2022 rebuild
build.bat Debug auto clean
```

Arguments can be supplied in any order. Run `build.bat help` for the full
syntax. The `generate` action only creates the Visual Studio solution, while
`build` and `rebuild` build the Engine DLL plus the Editor and Runtime apps.

You can still invoke CMake directly:

```powershell
cmake --preset vs2022
cmake --build --preset debug --target Editor Runtime
```

The Debug outputs are written to `build/<preset>/bin/Debug`:

- `Engine.dll` contains the shared engine implementation.
- `Editor.exe` starts the engine with the editor module.
- `Runtime.exe` starts the engine without the editor module.

Run either application with `Game` as its working directory:

```powershell
Set-Location Game
..\build\vs2022\bin\Debug\Editor.exe
..\build\vs2022\bin\Debug\Runtime.exe
```

Visual Studio 2026 users can use the equivalent `vs2026`,
`debug-vs2026`, and `release-vs2026` presets when invoking CMake directly.

The build downloads pinned versions of SDL2, GLEW, Assimp, Bullet,
PhysicsFS, DirectXTex, tinygltf, and miniaudio. The customized Dear ImGui
docking snapshot, MathGeoLib, Thekla Atlas, TinySpline, and ImGui Node Editor
remain isolated source targets because the engine relies on their legacy APIs.

## Credits

Carlos Fuentes<br>
Ricard Pillosu



## License

This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <http://unlicense.org/>

<br>
The dependecies licenses may wary on the project and projects license doesnt cover them 
