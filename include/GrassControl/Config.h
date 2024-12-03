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
