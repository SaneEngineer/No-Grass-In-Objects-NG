This is a incomplete attempt to port NGIO to Commonlib-NG. Currently hooks are only for SE, very little work has been done towards AE. The config is different and uses toml++. Raycasting is functional along with all other smaller features. Extended Grass distance is not functional, but is around 80% finished, l the hooks are implimented but is currently not functional and causes grass to not be rendered. All hooks for caching are implemented, but no files are created. 

## Requirements
* [CMake](https://cmake.org/)
	* Add this to your `PATH`
* [The Elder Scrolls V: Skyrim Special Edition](https://store.steampowered.com/app/489830)
	* Add the environment variable `Skyrim64Path` to point to the root installation of your game directory (the one containing `SkyrimSE.exe`).
* [Vcpkg](https://github.com/microsoft/vcpkg)
	* Add the environment variable `VCPKG_ROOT` with the value as the path to the folder containing vcpkg
* [Visual Studio Community 2022](https://visualstudio.microsoft.com/)
	* Desktop development with C++

