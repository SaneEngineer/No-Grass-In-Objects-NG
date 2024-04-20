#include <GrassControl/config.h>
#include <SimpleIni.h>

namespace GrassControl
{
	void ReadBoolSetting(CSimpleIniA& a_ini, const char* a_sectionName, const char* a_settingName, bool& a_setting)
	{
		const char* bFound = nullptr;
		bFound = a_ini.GetValue(a_sectionName, a_settingName);
		if (bFound) {
			a_setting = a_ini.GetBoolValue(a_sectionName, a_settingName);
		}
	}

	void ReadIntSetting(CSimpleIniA& a_ini, const char* a_sectionName, const char* a_settingName, int& a_setting)
	{
		const char* bFound = nullptr;
		bFound = a_ini.GetValue(a_sectionName, a_settingName);
		if (bFound) {
			a_setting = static_cast<int>(a_ini.GetLongValue(a_sectionName, a_settingName));
		}
	}

	void ReadDoubleSetting(CSimpleIniA& a_ini, const char* a_sectionName, const char* a_settingName, double& a_setting)
	{
		const char* bFound = nullptr;
		bFound = a_ini.GetValue(a_sectionName, a_settingName);
		if (bFound) {
			a_setting = a_ini.GetDoubleValue(a_sectionName, a_settingName);
		}
	}

	void ReadStringSetting(CSimpleIniA& a_ini, const char* a_sectionName, const char* a_settingName,
		std::string& a_setting)
	{
		const char* bFound = nullptr;
		bFound = a_ini.GetValue(a_sectionName, a_settingName);
		if (bFound) {
			a_setting = a_ini.GetValue(a_sectionName, a_settingName);
		}
	}

	void Config::ReadSettings()
	{
		const auto readIni = [&](auto a_path) {
			CSimpleIniA ini;
			ini.SetUnicode();
			ini.SetMultiLine();

			if (ini.LoadFile(a_path.data()) >= 0) {
				// Debug
				ReadBoolSetting(ini, "Debug", "Debug-Log-Enable", DebugLogEnable);

				//Raycast
				ReadBoolSetting(ini, "RayCastConfig", "Ray-cast-enabled", RayCast);
				ReadDoubleSetting(ini, "RayCastConfig", "Ray-cast-height", RayCastHeight);
				ReadDoubleSetting(ini, "RayCastConfig", "Ray-cast-depth", RayCastDepth);
				ReadStringSetting(ini, "RayCastConfig", "Ray-cast-collision-layers", RayCastCollisionLayers);
				ReadStringSetting(ini, "RayCastConfig", "Ray-cast-ignore-forms", RayCastIgnoreForms);

				//GrassConfig
				ReadBoolSetting(ini, "GrassConfig", "Super-dense-grass", SuperDenseGrass);
				ReadIntSetting(ini, "GrassConfig", "Super-dense-mode", SuperDenseMode);
				ReadBoolSetting(ini, "GrassConfig", "Profiler-report", ProfilerReport);
				ReadBoolSetting(ini, "GrassConfig", "Use-grass-cache", UseGrassCache);
				ReadBoolSetting(ini, "GrassConfig", "Extend-grass-distance", ExtendGrassDistance);
				ReadBoolSetting(ini, "GrassConfig", "Extend-grass-count", ExtendGrassCount);
				ReadIntSetting(ini, "GrassConfig", "Ensure-max-grass-types-setting", EnsureMaxGrassTypesPerTextureSetting);
				ReadDoubleSetting(ini, "GrassConfig", "Overwrite-grass-distance", OverwriteGrassDistance);
				ReadDoubleSetting(ini, "GrassConfig", "Overwrite-grass-fade-range", OverwriteGrassFadeRange);
				ReadIntSetting(ini, "GrassConfig", "Overwrite-min-grass-size", OverwriteMinGrassSize);
				ReadDoubleSetting(ini, "GrassConfig", "Global-grass-scale", GlobalGrassScale);
				ReadBoolSetting(ini, "GrassConfig", "Only-load-from-cache", OnlyLoadFromCache);
				ReadStringSetting(ini, "GrassConfig", "Skip-pregenerate-world-spaces", SkipPregenerateWorldSpaces);
				ReadStringSetting(ini, "GrassConfig", "Only-pregenerate-world-spaces", OnlyPregenerateWorldSpaces);
				ReadIntSetting(ini, "GrassConfig", "DynDOLOD-Grass-Mode", DynDOLODGrassMode);

				return true;
			}
			return false;
		};

		logger::info("Reading .ini...");
		if (readIni(iniPath)) {
			logger::info("...success");
		} else {
			logger::info("...ini not found, creating a new one");
			WriteSettings();
		}
	}

	void Config::WriteSettings()
	{
		logger::info("Writing .ini...");

		CSimpleIniA ini;
		ini.SetUnicode();

		ini.LoadFile(iniPath.data());

		// Debug
		ini.SetBoolValue("Debug", "Debug-Log-Enable", DebugLogEnable);

		//Raycast
		ini.SetBoolValue("RayCastConfig", "Ray-cast-enabled", RayCast);
		ini.SetDoubleValue("RayCastConfig", "Ray-cast-height", RayCastHeight);
		ini.SetDoubleValue("RayCastConfig", "Ray-cast-depth", RayCastDepth);
		ini.SetValue("RayCastConfig", "Ray-cast-collision-layers", RayCastCollisionLayers.c_str());
		ini.SetValue("RayCastConfig", "Ray-cast-ignore-forms", RayCastIgnoreForms.c_str());

		//GrassConfig
		ini.SetBoolValue("GrassConfig", "Super-dense-grass", SuperDenseGrass);
		ini.SetLongValue("GrassConfig", "Super-dense-mode", SuperDenseMode);
		ini.SetBoolValue("GrassConfig", "Profiler-report", ProfilerReport);
		ini.SetBoolValue("GrassConfig", "Use-grass-cache", UseGrassCache);
		ini.SetBoolValue("GrassConfig", "Extend-grass-distance", ExtendGrassDistance);
		ini.SetBoolValue("GrassConfig", "Extend-grass-count", ExtendGrassCount);
		ini.SetLongValue("GrassConfig", "Ensure-max-grass-types-setting", EnsureMaxGrassTypesPerTextureSetting);
		ini.SetDoubleValue("GrassConfig", "Overwrite-grass-distance", OverwriteGrassDistance);
		ini.SetDoubleValue("GrassConfig", "Overwrite-grass-fade-range", OverwriteGrassFadeRange);
		ini.SetLongValue("GrassConfig", "Overwrite-min-grass-size", OverwriteMinGrassSize);
		ini.SetDoubleValue("GrassConfig", "Global-grass-scale", GlobalGrassScale);
		ini.SetBoolValue("GrassConfig", "Only-load-from-cache", OnlyLoadFromCache);
		ini.SetValue("GrassConfig", "Skip-pregenerate-world-spaces", SkipPregenerateWorldSpaces.c_str());
		ini.SetValue("GrassConfig", "Only-pregenerate-world-spaces", OnlyPregenerateWorldSpaces.c_str());
		ini.SetLongValue("GrassConfig", "DynDOLOD-Grass-Mode", DynDOLODGrassMode);

		ini.SaveFile(iniPath.data());

		logger::info("...success");
	}
}
