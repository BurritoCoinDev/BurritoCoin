Building BurritoCoin Core with Visual Studio
========================================

Status: does not build
---------------------
**The Visual Studio (MSVC) build is not supported and does not currently build.** These project files predate the Litecoin MWEB code BurritoCoin inherited and were never ported to it. As of 2026-11-06 the build stops before MSBuild compiles anything:

- `msvc-autogen.py` crashes copying `libsecp256k1_config.h` into `src/secp256k1/`, which no longer exists. The library is now `src/secp256k1-zkp`.
- `libsecp256k1/libsecp256k1.vcxproj` and the include path in `common.init.vcxproj` still point at `src/secp256k1`. The zkp library also needs the modules MWEB uses: generator, recovery, commitment, bulletproof, aggsig, schnorrsig and extrakeys, with experimental enabled (see the `--enable-module-*` list in `configure.ac`).
- There is no project for `libmw` (`src/libmw`, the MWEB library), nor for its include paths: `libmw/include` and `libmw/deps/{caches,ghc,mio,crypto}/include`. `vcpkg.json` doesn't list `fmt`, which libmw links. Any file that includes `primitives/transaction.h` fails to compile, which is most of the codebase.
- The `.appveyor.yml` job that would catch this isn't connected to CI.

Windows releases are built with the mingw-w64 cross-compiler instead ([doc/build-windows.md](https://github.com/BurritoCoinDev/BurritoCoin/blob/master/doc/build-windows.md), automated in `.github/workflows/build-windows.yml`).

Making MSVC work again needs four things. Switch the secp256k1 paths and module defines to `secp256k1-zkp`. Add a `libmw` StaticLibrary project (via `msvc-autogen.py`'s `libs` list and a `libmw.vcxproj.in`) and reference it from every executable project. Add libmw's include paths to `common.init.vcxproj`. Add `fmt` to `vcpkg.json`. The hand-maintained `libburritocoin_qt.vcxproj` already lists every current Qt source, including the Mine tab. The rest of this file describes the build as it worked before MWEB.

Introduction
---------------------
Solution and project files to build the BurritoCoin Core applications `msbuild` or Visual Studio can be found in the `build_msvc` directory. Before MWEB, the build was tested with Visual Studio 2017 and 2019.

Building with Visual Studio is an alternative to the Linux based [cross-compiler build](https://github.com/BurritoCoinDev/BurritoCoin/blob/master/doc/build-windows.md).

Quick Start
---------------------
The minimal steps required to build BurritoCoin Core with the msbuild toolchain are below. More detailed instructions are contained in the following sections.

```
cd build_msvc
py -3 msvc-autogen.py
msbuild /m burritocoin.sln /p:Platform=x64 /p:Configuration=Release /t:build
```

Dependencies
---------------------
A number of [open source libraries](https://github.com/BurritoCoinDev/BurritoCoin/blob/master/doc/dependencies.md) are required in order to be able to build BurritoCoin Core.

Options for installing the dependencies in a Visual Studio compatible manner are:

- Use Microsoft's [vcpkg](https://docs.microsoft.com/en-us/cpp/vcpkg) to download the source packages and build locally. This is the recommended approach.
- Download the source code, build each dependency, add the required include paths, link libraries and binary tools to the Visual Studio project files.
- Use [nuget](https://www.nuget.org/) packages with the understanding that any binary files have been compiled by an untrusted third party.

The [external dependencies](https://github.com/BurritoCoinDev/BurritoCoin/blob/master/doc/dependencies.md) required for building are listed in the `build_msvc/vcpkg.json` file. The `msbuild` project files are configured to automatically install the `vcpkg` dependencies.

Qt
---------------------
In order to build the BurritoCoin Core a static build of Qt is required. The runtime library version (e.g. v141, v142) and platform type (x86 or x64) must also match.

Some prebuilt x64 versions of Qt can be downloaded from [here](https://github.com/sipsorcery/qt_win_binary/releases). Please be aware these downloads are NOT officially sanctioned by BurritoCoin Core and are provided for developer convenience only. They should NOT be used for builds that will be used in a production environment or with real funds.

To determine which Qt prebuilt version to download open the `.appveyor.yml` file and note the `QT_DOWNLOAD_URL`. When extracting the zip file the destination path must be set to `C:\`. This is due to the way that Qt includes, libraries and tools use internal paths.

To build BurritoCoin Core without Qt unload or disable the `burritocoin-qt`, `libburritocoin_qt` and `test_burritocoin-qt` projects.

Building
---------------------
The instructions below use `vcpkg` to install the dependencies.

- Install [`vcpkg`](https://github.com/Microsoft/vcpkg).

- Use Python to generate `*.vcxproj` from Makefile

```
PS >py -3 msvc-autogen.py
```

- An optional step is to adjust the settings in the `build_msvc` directory and the `common.init.vcxproj` file. This project file contains settings that are common to all projects such as the runtime library version and target Windows SDK version. The Qt directories can also be set.

- To build from the command line with the Visual Studio 2017 toolchain use:

```
msbuild /m burritocoin.sln /p:Platform=x64 /p:Configuration=Release /p:PlatformToolset=v141 /t:build
```

- To build from the command line with the Visual Studio 2019 toolchain use:

```
msbuild /m burritocoin.sln /p:Platform=x64 /p:Configuration=Release /t:build
```

- Alternatively open the `build_msvc/burritocoin.sln` file in Visual Studio.

AppVeyor
---------------------
The .appveyor.yml in the root directory is suitable to perform builds on [AppVeyor](https://www.appveyor.com/) Continuous Integration servers. The simplest way to perform an AppVeyor build is to fork BurritoCoin Core and then configure a new AppVeyor Project pointing to the forked repository.

For safety reasons the BurritoCoin Core .appveyor.yml file has the artifact options disabled. The build will be performed but no executable files will be available. To enable artifacts on a forked repository uncomment the lines shown below:

```
    #- 7z a burritocoin-%APPVEYOR_BUILD_VERSION%.zip %APPVEYOR_BUILD_FOLDER%\build_msvc\%platform%\%configuration%\*.exe
    #- path: burritocoin-%APPVEYOR_BUILD_VERSION%.zip
```
