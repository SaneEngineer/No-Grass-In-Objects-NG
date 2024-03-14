#pragma once

#include <SKSE/SKSE.h>
#include "GrassControl/Util.h"

namespace GrassControl
{
	class Config
	{
	private:
		using ISetting = AutoTOML::ISetting;

	public:
		using bSetting = AutoTOML::bSetting;
		using fSetting = AutoTOML::fSetting;
		using iSetting = AutoTOML::iSetting;
		using sSetting = AutoTOML::sSetting;

	    static inline bSetting DebugLogEnable{ "Debug", "Debug-Log-Enable", true };

		static inline bSetting RayCast{ "RayCastConfig", "Ray-cast-enabled", true };
		static inline fSetting RayCastHeight{ "RayCastConfig", "Ray-cast-height", 150.0f };
		static inline fSetting RayCastDepth{ "RayCastConfig", "Ray-cast-depth", 5.0f };
		static inline sSetting RayCastCollisionLayers{ "RayCastConfig", "Ray-cast-collision-layers", "1 2 13 20 31" };
		static inline sSetting RayCastIgnoreForms{ "RayCastConfig", "Ray-cast-ignore-forms", "" };

	    static inline bSetting SuperDenseGrass{ "GrassConfig", "Super-dense-grass", false };
		static inline iSetting SuperDenseMode{ "GrassConfig", "Super-dense-mode", 8 };
		static inline bSetting ProfilerReport{ "GrassConfig", "Profiler-report", false };
		static inline bSetting UseGrassCache{ "GrassConfig", "Use-grass-cache", false };
		static inline bSetting ExtendGrassDistance{ "GrassConfig", "Extend-grass-distance", false };
		static inline bSetting ExtendGrassCount{ "GrassConfig", "Extend-grass-count", true };
		static inline iSetting EnsureMaxGrassTypesPerTextureSetting{ "GrassConfig", "Ensure-max-grass-types-setting", 7 };
		static inline fSetting OverwriteGrassDistance{ "GrassConfig", "Overwrite-grass-distance", 6000.0f };
		static inline fSetting OverwriteGrassFadeRange{ "GrassConfig", "Overwrite-grass-fade-range", 3000.0f };
		static inline iSetting OverwriteMinGrassSize{ "GrassConfig", "Overwrite-min-grass-size", -1 };
		static inline fSetting GlobalGrassScale{ "GrassConfig", "Global-grass-scale", 1.0f };
		static inline bSetting OnlyLoadFromCache{ "GrassConfig", "Only-load-from-cache", false };
		static inline sSetting SkipPregenerateWorldSpaces{ "GrassConfig", "Skip-pregenerate-world-spaces", "DLC2ApocryphaWorld;DLC01Boneyard;WindhelmPitWorldspace" };
		static inline sSetting OnlyPregenerateWorldSpaces{ "GrassConfig", "Only-pregenerate-world-spaces", "" };
		static inline iSetting DynDOLODGrassMode{ "GrassConfig", "DynDOLOD-Grass-Mode", 0 };
		
		static void load()
		{
			try {
				const auto table = toml::parse_file("Data/SKSE/Plugins/GrassControl.toml"s);
				for (const auto& setting : ISetting::get_settings()) {
					try {
						setting->load(table);
					} catch (const std::exception& e) {
						logger::warn(fmt::runtime(e.what()));
					}
				}
			} catch (const toml::parse_error& e) {
				std::ostringstream ss;
				ss
					<< "Error parsing file \'" << *e.source().path << "\':\n"
					<< '\t' << e.description() << '\n'
					<< "\t\t(" << e.source().begin << ')';
				logger::error(fmt::runtime(ss.str()));
				throw std::runtime_error("failed to load settings"s);
			}
		}
	};
}
