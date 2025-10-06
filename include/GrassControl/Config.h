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
		static inline std::string RayCastIgnoreGrassForms = "";
		static inline double RayCastTextureWidth = 5.0;
		static inline bool GrassCliffs = true;
		static inline std::string GrassCliffsForms = "13303:Skyrim.esm;B8D38:Skyrim.esm;5092D:Skyrim.esm;24E48:Skyrim.esm;B018B:Skyrim.esm;B018C:Skyrim.esm;56AD2:Skyrim.esm;10A7F5:Skyrim.esm;5B82C:Skyrim.esm;55A69:Skyrim.esm;5C164:Skyrim.esm;97065:Skyrim.esm;135E2:Skyrim.esm;BAD90:Skyrim.esm;4D709:Skyrim.esm;B018A:Skyrim.esm;B018D:Skyrim.esm;56AD5:Skyrim.esm;22763:Skyrim.esm;5B7E1:Skyrim.esm;D37C0:Skyrim.esm;55A6B:Skyrim.esm;8976D:Skyrim.esm;897C5:Skyrim.esm;16D5F:Skyrim.esm;50928:Skyrim.esm;3ACD3:Skyrim.esm;39A8E:Skyrim.esm;B0187:Skyrim.esm;B018E:Skyrim.esm;56ACD:Skyrim.esm;1EF4A:Skyrim.esm;5B7E2:Skyrim.esm;61924:Skyrim.esm;55A6D:Skyrim.esm;197C2:Skyrim.esm;5C107:Skyrim.esm;8976E:Skyrim.esm;56ADC:Skyrim.esm;60095:Skyrim.esm;B018F:Skyrim.esm;B0190:Skyrim.esm;58EF5:Skyrim.esm;8B025:Skyrim.esm;60096:Skyrim.esm;D37B7:Skyrim.esm;B8A78:Skyrim.esm;B9528:Skyrim.esm;13976:Skyrim.esm;46186:Skyrim.esm;4D715:Skyrim.esm;B425E:Skyrim.esm;B425F:Skyrim.esm;1EF3F:Skyrim.esm;56AD7:Skyrim.esm;5B887:Skyrim.esm;1EF3C:Skyrim.esm;61925:Skyrim.esm;6A8C8:Skyrim.esm;19845:Skyrim.esm;5C16E:Skyrim.esm;9707C:Skyrim.esm;8976F:Skyrim.esm;5092B:Skyrim.esm;13A5B:Skyrim.esm;BC5CB:Skyrim.esm;6E0DD:Skyrim.esm;56AD8:Skyrim.esm;1EF4B:Skyrim.esm;5B7DE:Skyrim.esm;61923:Skyrim.esm;6A8C9:Skyrim.esm;19856:Skyrim.esm;659EE:Skyrim.esm;50931:Skyrim.esm;B0189:Skyrim.esm;98BE2:Skyrim.esm;998C9:Skyrim.esm;13974:Skyrim.esm;DFA3A:Skyrim.esm;42A88:Skyrim.esm;39B63:Skyrim.esm;B0188:Skyrim.esm;B0191:Skyrim.esm;56AD9:Skyrim.esm;D1EC2:Skyrim.esm;5B7E3:Skyrim.esm;D1EC3:Skyrim.esm;8A03E:Skyrim.esm;19885:Skyrim.esm;43947:Skyrim.esm;8976C:Skyrim.esm;897C6:Skyrim.esm";
		static inline int RayCastMode = 1;
		static inline double RayCastWidth = 0.0f;
		static inline double RayCastWidthMult = 0.3f;
		static inline bool RayCastError = true;

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
