This is a port of No Grass in Objects rewritten using Commonlib. Currently hooks are only for SE, very little work has been done towards AE. The config is roughly the same with only a change to toml. To my knowledge all features should be working in SE, but I have not done much testing. 

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

