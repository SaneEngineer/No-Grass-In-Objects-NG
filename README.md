This is a port of No Grass in Objects rewritten using Commonlib. All hooks except for extended grass distance without cache are working for both SE and AE. The config is roughly the same with only a change to toml. To my knowledge all features except for grass count should be working in SE, the same goes for AE with the additon of extended distance without cache also not working.

## Requirements
* [Casual Library](https://github.com/CasualCoder91/CasualLibrary/)
	* Compile or add libs from the github releases and add to `/external/CasualLibrary1.0/`
* [CMake](https://cmake.org/)
	* Add this to your `PATH`
* [The Elder Scrolls V: Skyrim Special Edition](https://store.steampowered.com/app/489830)
	* Add the environment variable `Skyrim64Path` to point to the root installation of your game directory (the one containing `SkyrimSE.exe`).
* [Vcpkg](https://github.com/microsoft/vcpkg)
	* Add the environment variable `VCPKG_ROOT` with the value as the path to the folder containing vcpkg
* [Visual Studio Community 2022](https://visualstudio.microsoft.com/)
	* Desktop development with C++

