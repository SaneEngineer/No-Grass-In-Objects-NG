#include "GrassControl/Main.h"
#include "GrassControl/Events.h"

using namespace SKSE;
using namespace SKSE::log;
using namespace SKSE::stl;


void InitializeHooking()
{
	log::trace("Initializing trampoline...");
	auto& trampoline = GetTrampoline();
	trampoline.create(2048);
	log::trace("Trampoline initialized.");
	GrassControl::GrassControlPlugin::InstallHooks();
    GrassControl::GidFileGenerationTask::InstallHooks();
    GrassControl::DistantGrass::InstallHooks();
}

 void InitializeMessaging()
{
	if (!GetMessagingInterface()->RegisterListener([](MessagingInterface::Message* message) {
			switch (message->type) {
			// Skyrim lifecycle events.
			case MessagingInterface::kPostLoad:  // Called after all plugins have finished running SKSEPlugin_Load.
				// It is now safe to do multithreaded operations, or operations against other plugins.
				break;
			case MessagingInterface::kInputLoaded:   // Called when all game data has been found.
				GrassControl::GrassControlPlugin::init();
				break;
			case MessagingInterface::kDataLoaded:  // All ESM/ESL/ESP plugins have loaded, main menu is now active.
				// It is now safe to access form data.
				MenuOpenCloseEventHandler::Register();
				break;
			default:
			    break;
			}
		})) {
		report_and_fail("Unable to register message listener.");
	}
}


#ifdef SKYRIM_AE
extern "C" DLLEXPORT constinit auto SKSEPlugin_Version = []() {
	SKSE::PluginVersionData v;
	v.PluginVersion(Version::MAJOR);
	v.PluginName(Version::PROJECT);
	v.AuthorName("DewemerEngineer");
	v.UsesAddressLibrary();
	v.UsesUpdatedStructs();
	v.CompatibleVersions({ SKSE::RUNTIME_LATEST });

	return v;
}();
#else
extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Query(const SKSE::QueryInterface* a_skse, SKSE::PluginInfo* a_info)
{
	a_info->infoVersion = SKSE::PluginInfo::kVersion;
	a_info->name = Version::PROJECT.data();
	a_info->version = Version::MAJOR;

	if (a_skse->IsEditor()) {
		logger::critical("Loaded in editor, marking as incompatible"sv);
		return false;
	}

	const auto ver = a_skse->RuntimeVersion();
	if (ver <
#	ifdef SKYRIMVR
		SKSE::RUNTIME_VR_1_4_15
#	else
		SKSE::RUNTIME_1_5_97
#	endif
	) {
		logger::critical(FMT_STRING("Unsupported runtime version {}"), ver.string());
		return false;
	}

	return true;
}
#endif

void InitializeLog() {
		auto path = logger::log_directory();
	    if (!path) {
		    report_and_fail("Failed to find standard logging directory"sv);
	    }

	    *path /= Version::PROJECT;
	    *path += ".log"sv;
		auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
#ifndef NDEBUG
		const auto level = spdlog::level::trace;
#else
		const auto level = spdlog::level::info;
#endif

		auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));
		log->set_level(level);
		log->flush_on(level);

		spdlog::set_default_logger(std::move(log));
	    spdlog::set_pattern("[%H:%M:%S:%e] %v"s);

	    logger::info(FMT_STRING("{} v{}"), Version::PROJECT, Version::NAME);
}


extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
{
#ifndef NDEBUG
	while (!IsDebuggerPresent()) {
		Sleep(100);
	}
#endif

	InitializeLog();

	logger::info("Game version : {}", a_skse->RuntimeVersion().string());

	SKSE::Init(a_skse);

	GrassControl::Config::load();
	InitializeHooking();
	InitializeMessaging();

	return true;
}

