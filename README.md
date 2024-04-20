## No Grass in Objects NG

This is a port of No Grass in Objects rewritten using Commonlib, The main features of Caching, Raycasting (No grass in objects part), and extended grass distance are working in AE. The config is roughly the same with a change to toml, minor renaming of settings and an option for debug logs. To my knowledge all features except for grass scale should be working in SE, the same goes for AE.

---

## MO2 Installation Instructions:

1. Download and install the Mod Organizer 2 plugin from the original mod into your MO2 plugins directory, located here: [`Grass Generation MO2 Plugin v1 (fix)`](https://www.nexusmods.com/skyrimspecialedition/mods/42161?tab=files).
 - You will need to edit lines 56 and 57 to reflect `grassPluginPath = "SKSE/Plugins/"` and `grassPlugin = "*NGIO-NG.dll"`, as shown below.

    ![Screenshot 2024-04-20 080244](https://github.com/AllstaRawR/No-Grass-In-Objects-NG/assets/164591926/af9cced5-e1f9-49b8-8014-863e6195d87f)

2. You will then need to install the latest version of NGIO-NG located in the releases section [here on Github](https://github.com/SaneEngineer/No-Grass-In-Objects-NG/releases/). This needs to be installed to the `\Data\SKSE\Plugins\...` directory of your Skyrim installation.
3. You will now clock on the `Tools -> Plugins -> Precache Grass` option in your MO2 toolbar, which will launch (and re-launch) Skyrim to generate grass cache in your `Overwrites` folder per the settings in `GrassControl.ini`.
4. After you have waited, you should get a pop up message that says, `Grass Caching complete`, at which point you can copy the `\Grass\` folder from `Overwrites` into a new mod in MO2.

---

## Vortex/Manual Installation Instructions:
1. Install the latest version of NGIO-NG located in the releases section [here on Github](https://github.com/SaneEngineer/No-Grass-In-Objects-NG/releases/). This needs to be installed to the `\Data\SKSE\Plugins\...` directory of your Skyrim installation.
2. In order to start the Grass Cache, set `Use-grass-cache = true, Extend-grass-distance = true, Only-load-from-cache = true` in `GrasscControl.ini`
3. Create a new text document called `PrecacheGrass.txt` in the root folder of Skyrim Next to SkyrimSE.exe.
This can be found by opening Steam and locating Skyrim SE in your game library, right-click it and navigate to Manage > Browse Local Files
5. The next time you open the game, caching should begin. Caching is expected to crash repeately, just reopen the game and it should continue.

---

##Common Issues:
* Instant completion of Caching or completion of caching for only some worldspaces.
  - Check `Skip-pregenerate-world-spaces` and `Only-pregenerate-world-spaces`, make sure that both of these options are only one line.
* Crashing while Caching
  - Check `NGIO-NG/log`, `PrecacheGrass.txt`, and `Data\Grass\` to see if any progress is being done. If no progress is being made check any crash logs as the crash is likely caused by an issue with a mod affecting the area. This can be a texture, mesh, patch, or mod editing the area. It should be mentioned by the crash log.
  - I recommend using [Crash Logger SSE AE VR](https://www.nexusmods.com/skyrimspecialedition/mods/59818?tab=description) over [Trainwreck](https://www.nexusmods.com/skyrimspecialedition/mods/106440). As it provides much more helpful and readable crash logs. 


## Requirements to Build

- [Casual Library](https://github.com/CasualCoder91/CasualLibrary/)
  - Compile or add libs from the github releases and add to `/external/CasualLibrary1.0/`
- [CMake](https://cmake.org/)
  - Add this to your `PATH`
- [The Elder Scrolls V: Skyrim Special Edition](https://store.steampowered.com/app/489830)
  - Add the environment variable `Skyrim64Path` to point to the root installation of your game directory (the one containing `SkyrimSE.exe`).
- [Vcpkg](https://github.com/microsoft/vcpkg)
  - Add the environment variable `VCPKG_ROOT` with the value as the path to the folder containing vcpkg
- [Visual Studio Community 2022](https://visualstudio.microsoft.com/)
  - Desktop development with C++
