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

		static void InstallHooks()
		{
			Hooks::Install();
		}

		static void OnMainMenuOpen();

        static bool CanPlaceGrassWrapper(RE::TESObjectLAND* land, float x, float y, float z);
		static void Update();

        static RE::TESObjectREFR* PlaceAtMe(RE::TESObjectREFR* self, RE::TESForm* a_form, std::uint32_t count, bool forcePersist, bool initiallyDisabled)
		{
			using func_t = RE::TESObjectREFR*(RE::BSScript::Internal::VirtualMachine*, RE::VMStackID, RE::TESObjectREFR*, RE::TESForm*, std::uint32_t, bool, bool);
			RE::VMStackID frame = 0;

			REL::Relocation<func_t> func{ RELOCATION_ID(55672, 56203) };
			auto vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();

			return func(vm, frame, self, a_form, count, forcePersist, initiallyDisabled);
		};


	protected:
		struct Hooks
		{
			struct MainUpdate_Nullsub
			{
				static void thunk()
				{
					func();
					Update();
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

			struct GrassScale
            {
                static double thunk(int64_t a1, uint32_t a2)
                {
                    return *Config::GlobalGrassScale * func(a1, a2);
                }
                static inline REL::Relocation<decltype(thunk)> func;
            };	
			
			static void Install()
			{
				stl::write_thunk_call<MainUpdate_Nullsub>(RELOCATION_ID(35565, 36564).address() + OFFSET(0x748, 0xC26));
				if (*Config::ProfilerReport) {
					stl::write_thunk_call<ConsoleOpen>(RELOCATION_ID(50155, 51082).address() + OFFSET(334, 334));
					stl::write_thunk_call<GrassCreationStart>(RELOCATION_ID(13148, 13288).address() + OFFSET(0x905, 0x905));
					stl::write_thunk_jump<GrassCreationStart>(RELOCATION_ID(13138, 13278).address() + OFFSET(0xF, 0xF));
					stl::write_thunk_call<GrassCreationEnd>(RELOCATION_ID(227915, 175016).address() + OFFSET(3037, 3037));
				}
				if (*Config::GlobalGrassScale != 1.0 && *Config::GlobalGrassScale > 0.0001) {
					auto addr = RELOCATION_ID(15212, 15381).address() + OFFSET(0x949, 0x74F);
                    #ifndef SKYRIM_AE 
			        Utility::Memory::SafeWrite(addr + 7, Utility::Assembly::NoOperation8);
                    #endif
				    stl::write_thunk_call<GrassScale>(addr);
				}
			}
		};
	};
}
