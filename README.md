This is a very crude attempt to port NGIO to Commonlib-NG. It uses toml for config. Raycasting is functional along with all other smaller features. Extended Grass distance is not functional, but is close maybe 80%, only needs debugging for all grass being removed. Caching doesn't work, needs debugging. 

## Requirements
* [CMake](https://cmake.org/)
	* Add this to your `PATH`
* [The Elder Scrolls V: Skyrim Special Edition](https://store.steampowered.com/app/489830)
	* Add the environment variable `Skyrim64Path` to point to the root installation of your game directory (the one containing `SkyrimSE.exe`).
* [Vcpkg](https://github.com/microsoft/vcpkg)
	* Add the environment variable `VCPKG_ROOT` with the value as the path to the folder containing vcpkg
* [Visual Studio Community 2022](https://visualstudio.microsoft.com/)
	* Desktop development with C++

