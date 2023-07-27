#pragma once

#include <SKSE/SKSE.h>
#include "Util.h"

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
						logger::warn(e.what());
					}
				}
			} catch (const toml::parse_error& e) {
				std::ostringstream ss;
				ss
					<< "Error parsing file \'" << *e.source().path << "\':\n"
					<< '\t' << e.description() << '\n'
					<< "\t\t(" << e.source().begin << ')';
				logger::error(ss.str());
				throw std::runtime_error("failed to load settings"s);
			}
		}
	};



	/// <summary>
	/// Cached form list for lookups later.
	/// </summary>
	class CachedFormList final
	{
		/// <summary>
		/// Prevents a default instance of the <see cref="CachedFormList"/> class from being created.
		/// </summary>
	private:
		CachedFormList();

		/// <summary>
		/// The forms.
		/// </summary>
		std::vector<RE::TESForm*> Forms = std::vector<RE::TESForm*>();

		/// <summary>
		/// The ids.
		/// </summary>
		std::unordered_set<RE::FormID> Ids = std::unordered_set<RE::FormID>();

		/// <summary>
		/// Tries to parse from input. Returns null if failed.
		/// </summary>
		/// <param name="input">The input.</param>
		/// <param name="pluginForLog">The plugin for log.</param>
		/// <param name="settingNameForLog">The setting name for log.</param>
		/// <param name="warnOnMissingForm">If set to <c>true</c> warn on missing form.</param>
		/// <param name="dontWriteAnythingToLog">Don't write any errors to log if failed to parse.</param>
		/// <returns></returns>
	public:
		static CachedFormList* TryParse(const std::string& input, std::string pluginForLog, std::string settingNameForLog, bool warnOnMissingForm = true, bool dontWriteAnythingToLog = false);

		/// <summary>
		/// Determines whether this list contains the specified form.
		/// </summary>
		/// <param name="form">The form.</param>
		/// <returns></returns>
		bool Contains(RE::TESForm* form);

		/// <summary>
		/// Determines whether this list contains the specified form identifier.
		/// </summary>
		/// <param name="formId">The form identifier.</param>
		/// <returns></returns>
		bool Contains(unsigned int formId);

		/// <summary>
		/// Gets all forms in this list.
		/// </summary>
		/// <value>
		/// All.
		/// </value>
		std::vector<RE::TESForm*> getAll() const;
	};
	
}
