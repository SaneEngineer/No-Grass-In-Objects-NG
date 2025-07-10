#pragma once

#include "GrassControl/Util.h"
#include <SKSE/SKSE.h>

namespace GrassControl
{
	class Config
	{
	public:
		static void ReadSettings();
		static void WriteSettings();

		static inline bool DebugLogEnable = false;

		static inline bool RayCast = true;
		static inline double RayCastHeight = 150.0;
		static inline double RayCastDepth = 5.0;
		static inline std::string RayCastCollisionLayers = "1 2 13 20 31";
		static inline std::string RayCastIgnoreForms = "";
		static inline std::string RayCastTextureForms = "";
		static inline double RayCastTextureWidth = 5.0;
		static inline bool GrassCliffs = true;
		static inline std::string GrassCliffsForms = "22763:Skyrim.esm;1EF4A:Skyrim.esm;5B887:Skyrim.esm;1EF4B:Skyrim.esm;D1EC2:Skyrim.esm;5B82C:Skyrim.esm;5B7E1:Skyrim.esm;5B7E2:Skyrim.esm;1EF3C:Skyrim.esm;5B7DE:Skyrim.esm;5B7E3:Skyrim.esm;B8D38:Skyrim.esm;5092D:Skyrim.esm;BAD90:Skyrim.esm;50928:Skyrim.esm;5092B:Skyrim.esm;BC5CB:Skyrim.esm;50931:Skyrim.esm;13303:Skyrim.esm;135e2:Skyrim.esm;16d5f:Skyrim.esm;13976:Skyrim.esm;13a5b:Skyrim.esm;13974:Skyrim.esm;d37c0:Skyrim.esm;61924:Skyrim.esm;61925:Skyrim.esm;61923:Skyrim.esm;d1ec3:Skyrim.esm;24e48:Skyrim.esm;4d709:Skyrim.esm;3acd3:Skyrim.esm;4d715:Skyrim.esm;6e0dd:Skyrim.esm;42a88:Skyrim.esm;55a69:Skyrim.esm;55a6b:Skyrim.esm;55a6d:Skyrim.esm;6a8c8:Skyrim.esm;6a8c9:Skyrim.esm;8a03e:Skyrim.esm;56ad2:Skyrim.esm;56ad5:Skyrim.esm;56acd:Skyrim.esm;56ad7:Skyrim.esm;56ad8:Skyrim.esm;56ad9:Skyrim.esm;b018b:Skyrim.esm;b018a:Skyrim.esm;b0187:Skyrim.esm;b425e:Skyrim.esm;b0189:Skyrim.esm;b0188:Skyrim.esm;b018c:Skyrim.esm;b018a:Skyrim.esm;b018e:Skyrim.esm;b425f:Skyrim.esm;b0191:Skyrim.esm";
		static inline int RayCastMode = 0;
		static inline double RayCastWidth = 0.0f;

		static inline bool SuperDenseGrass = false;
		static inline int SuperDenseMode = 8;
		static inline bool ProfilerReport = false;
		static inline bool UseGrassCache = false;
		static inline bool ExtendGrassDistance = false;
		static inline bool ExtendGrassCount = true;
		static inline int EnsureMaxGrassTypesPerTextureSetting = 7;
		static inline double OverwriteGrassDistance = 6000.0;
		static inline double OverwriteGrassFadeRange = 3000.0;
		static inline int OverwriteMinGrassSize = -1;
		static inline double GlobalGrassScale = 1.0;
		static inline bool OnlyLoadFromCache = false;
		static inline std::string SkipPregenerateWorldSpaces = "DLC2ApocryphaWorld;DLC01Boneyard;WindhelmPitWorldspace";
		static inline std::string OnlyPregenerateWorldSpaces = "";
		static inline int DynDOLODGrassMode = 0;
		static inline int MaxFailures = 2;
		static inline bool FreezeCheck = true;
		static inline bool Updating = false;

		constexpr static inline std::string_view iniPath = "Data/SKSE/Plugins/GrassControl.ini";
	};
}
