#pragma once

#include "CasualLibrary/CasualLibrary.hpp"
#include "GrassControl/Config.h"
#include "GrassControl/DistantGrass.h"
#include "GrassControl/GidFileCache.h"
#include "GrassControl/Logging.h"
#include "GrassControl/Profiler.h"
#include "GrassControl/RaycastHelper.h"

#include "Shared/Utility/Memory.h"
#include "Shared/Utility/Assembly.h"



namespace GrassControl
{

	class GrassControlPlugin final
	{
		inline static Profiler* profiler = new Profiler();

		static std::intptr_t addr_MaxGrassPerTexture;

		static int _did_mainMenu;

	public:
		inline static std::unique_ptr<RaycastHelper> Cache;

	    static void init();

		static void warn_extend_without_cache();

		static GrassControlPlugin* GetSingleton()
		{
			static GrassControlPlugin handler;
			return &handler;
		}

		static void InstallHooks()
		{
			Hooks::Install();
		}

		static void OnMainMenuOpen();

		static void Update();


	protected:
		struct Hooks
		{
			struct MainUpdate_Nullsub
			{
				static void thunk()
				{
					func();
					GetSingleton()->Update();
				}
				static inline REL::Relocation<decltype(thunk)> func;
			};

			// Track when console is opened.
			struct ConsoleOpen
			{
				static void thunk(RE::PlayerCharacter* playerCharacter)
				{
					func(playerCharacter);
					profiler->Report();
				}
				static inline REL::Relocation<decltype(thunk)> func;
			};

			// Track the time taken in create grass function.
			// 4D10
			struct GrassCreationStart
			{
				static uint64_t thunk(uint64_t a1, uint64_t a2, char* a3)
				{
				    profiler->Begin();

					return func(a1, a2, a3);
				}
				static inline REL::Relocation<decltype(thunk)> func;
			};
			// 5923
			struct GrassCreationEnd
			{
				static void thunk()
				{
					func();
				    profiler->End();
				}
				static inline REL::Relocation<decltype(thunk)> func;
			};
			
			static void Install()
			{
				stl::write_thunk_call<MainUpdate_Nullsub>(RELOCATION_ID(35565, 36564).address() + OFFSET(0x748, 0xC26));
				if (*Config::ProfilerReport) {
					stl::write_thunk_call<ConsoleOpen>(RELOCATION_ID(50155, 51082).address() + OFFSET(334, 0));
					stl::write_thunk_call<GrassCreationStart>(RELOCATION_ID(13148, 13288).address() + OFFSET(0x905, 0));
					stl::write_thunk_jump<GrassCreationStart>(RELOCATION_ID(13138, 13278).address() + OFFSET(0xF, 0));
					stl::write_thunk_call<GrassCreationEnd>(RELOCATION_ID(227915, 175016).address() + OFFSET(3037, 0));
				}
			}
		};
	};
}
