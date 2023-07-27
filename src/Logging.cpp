#include "GrassControl/Logging.h"

#include <SKSE/SKSE.h>

#include "GrassControl/Config.h"

using namespace SKSE;
using namespace SKSE::log;
using namespace SKSE::stl;

void InitializeLogging() {
    auto path = log_directory();
    if (!path) {
        report_and_fail("Unable to lookup SKSE logs directory.");
    }
    *path /= PluginDeclaration::GetSingleton()->GetName();
    *path += L".log";

    std::shared_ptr<spdlog::logger> log;
	/*
    if (IsDebuggerPresent()) {
        log = std::make_shared<spdlog::logger>(
            "Global", std::make_shared<spdlog::sinks::msvc_sink_mt>());
    } else {
        log = std::make_shared<spdlog::logger>(
            "Global", std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true));
    }
    */
	log = std::make_shared<spdlog::logger>(
		"Global", std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true));

	const auto& IsDebug = *GrassControl::Config::DebugLogEnable ? spdlog::level::trace : spdlog::level::info;
	log->set_level(IsDebug);
	log->flush_on(IsDebug);

    spdlog::set_default_logger(std::move(log));
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] [%t] [%s:%#] %v");
}
