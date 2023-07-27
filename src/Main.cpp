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
}

 void InitializeMessaging()
{
	if (!GetMessagingInterface()->RegisterListener([](MessagingInterface::Message* message) {
			switch (message->type) {
			// Skyrim lifecycle events.
			case MessagingInterface::kPostLoad:  // Called after all plugins have finished running SKSEPlugin_Load.
				// It is now safe to do multithreaded operations, or operations against other plugins.
			case MessagingInterface::kPostPostLoad:  // Called after all kPostLoad message handlers have run.
			case MessagingInterface::kInputLoaded:   // Called when all game data has been found.
				break;
			case MessagingInterface::kDataLoaded:  // All ESM/ESL/ESP plugins have loaded, main menu is now active.
				// It is now safe to access form data.
				InitializeHooking();
				MenuOpenCloseEventHandler::Register();
				GrassControl::GrassControlPlugin::init();
				break;
			case MessagingInterface::kDeleteGame:  // The player deleted a saved game from within the load menu.
				break;
			}
		})) {
		report_and_fail("Unable to register message listener.");
	}
}

SKSEPluginLoad(const LoadInterface* skse)
{
#ifndef NDEBUG
	while (!IsDebuggerPresent()) {
		Sleep(100);
	}
#endif
	GrassControl::Config::load();
	InitializeLogging();
	const auto* plugin = PluginDeclaration::GetSingleton();
	auto version = plugin->GetVersion();
	log::info("{} {} is loading...", plugin->GetName(), version);
	Init(skse);
	GrassControl::GrassControlPlugin::InstallHooks();
	GrassControl::GidFileGenerationTask::InstallHooks();
	InitializeMessaging();

	log::info("{} has finished loading.", plugin->GetName());
	return true;
}

namespace GrassControl
{
	bool CanPlaceGrassWrapper(RE::TESObjectLAND* land, const float x, const float y, const float z)
	{
		return GrassControlPlugin::Cache->CanPlaceGrass(land, x, y, z);
	}
	std::intptr_t GrassControlPlugin::addr_MaxGrassPerTexture;
	int GrassControlPlugin::_did_mainMenu = 0;

	void GrassControlPlugin::Update()
	{
		if (!*Config::UseGrassCache) 
			return;
		
		if (_did_mainMenu == 0)
			return;

		if (_did_mainMenu == 1)
		{
			_did_mainMenu++;
		}
		else if (_did_mainMenu == 2)
		{
			_did_mainMenu++;

			GidFileGenerationTask::cur_state = 1;

			GidFileGenerationTask::_lastDidSomething = static_cast<volatile long long>(GetTickCount64());
			std::thread t(GidFileGenerationTask::run_freeze_check);
			t.detach();

			auto gf = std::make_unique<GidFileGenerationTask>();
			GidFileGenerationTask::cur_instance = std::move(gf);
		}
	}

	 void GrassControlPlugin::OnMainMenuOpen()
	 {
		auto fi = std::filesystem::path(GidFileGenerationTask::getProgressFilePath());
		if (*Config::UseGrassCache && exists(fi)) {
			_did_mainMenu = 1;
			Memory::Internal::write<uint8_t>(RELOCATION_ID(508798, 380767).address() + 8, 1);  // Skyrim.ini [General] bAlwaysActive=1
			Memory::Internal::write<uint8_t>(RELOCATION_ID(501125, 359439).address() + 8, 0);  // Skyrim.ini [Grass] bAllowLoadGrass=0
		}

		if (*Config::RayCast) {
			std::string formsStr = *Config::RayCastIgnoreForms;
			auto cachedList = CachedFormList::TryParse(formsStr, "GrassControl", "RayCastIgnoreForms", false, true);
			if (cachedList != nullptr && cachedList->getAll().empty()) {
				cachedList = nullptr;
			}
			const auto* plugin = PluginDeclaration::GetSingleton();
			Cache = std::make_unique<RaycastHelper>(std::stoi(plugin->GetVersion().string()), *Config::RayCastHeight, *Config::RayCastDepth, *Config::RayCastCollisionLayers, cachedList);
			logger::info("Created Cache");

			if (auto addr = (RELOCATION_ID(15212, 15381).address() + (0x723A - 0x6CE0)); REL::make_pattern<"F3 0F 10 75 B8">().match(RELOCATION_ID(15212, 15381).address() + (0x723A - 0x6CE0))) {
				//Memory::WriteHook(new HookParameters(){ Address = addr, IncludeLength = 5, ReplaceLength = 5, Before = [&](std::any ctx)
				struct Patch : Xbyak::CodeGenerator
				{
					Patch(std::uintptr_t b_func, std::uintptr_t a_target)
					{
						Xbyak::Label funcLabel;
						Xbyak::Label retnLabel;

						Xbyak::Label jump;

						Xbyak::Label notIf;

						Xbyak::Reg64 land = rsi;
						Xbyak::Address x = ptr[rsp + 0x40];
						Xbyak::Address y = ptr[rsp + 0x44];
						Xbyak::Xmm z = xmm7;

						cmp(land, 0);
						je(notIf);

						if (Cache != nullptr) {
							movss(xmm3, z);
							movss(xmm2, y);
							movss(xmm1, x);
							mov(rcx, land);

							sub(rsp, 0x20);
							call(ptr[rip + funcLabel]);  // call our function
							add(rsp, 0x20);

							movzx(eax, al);
							test(eax, eax);
							jne(notIf);
							jmp(ptr[rip + jump]);
						} else {
							jmp(notIf);
						}

						L(notIf);
						movss(xmm6, ptr[rbp - 0x48]);
						jmp(ptr[rip + retnLabel]);

						L(jump);
						dq(a_target + 0x5 + (0x661 - 0x23F));

						L(funcLabel);
						dq(b_func);

						L(retnLabel);
						dq(a_target + 0x5);
					}
				};
				Patch patch(reinterpret_cast<uintptr_t>(CanPlaceGrassWrapper), addr);
				patch.ready();

				auto& trampoline = SKSE::GetTrampoline();
				trampoline.write_branch<5>(addr, trampoline.allocate(patch));
			}
		}

		if (*Config::ExtendGrassDistance) {
			if (!*Config::UseGrassCache || !*Config::OnlyLoadFromCache) {
			    warn_extend_without_cache();
			}
		}
	 }

	auto to_vector(const float f)
	{
		// get vector of the right size
		std::vector<unsigned char> data(sizeof(f));
		// copy the bytes
		std::memcpy(data.data(), &f, sizeof(f));
		return data;
	}

	void GrassControlPlugin::init()
	{
		std::stringstream s;
		auto list = { std::to_string(static_cast<int>(RE::COL_LAYER::kStatic)), std::to_string(static_cast<int>(RE::COL_LAYER::kAnimStatic)),  std::to_string(static_cast<int>(RE::COL_LAYER::kTerrain)), std::to_string(static_cast<int>(RE::COL_LAYER::kDebrisLarge)),  std::to_string(static_cast<int>(RE::COL_LAYER::kStairHelper)) };
		std::ranges::copy(list.begin(), list.end(), std::ostream_iterator<std::string, char>(s, " "));

		*Config::RayCastCollisionLayers = s.str();

		auto fi = std::filesystem::path(GidFileGenerationTask::getProgressFilePath());
		if (*Config::UseGrassCache && exists(fi))
		{
			*Config::OnlyLoadFromCache = false;

			GidFileGenerationTask::apply();

			*Config::ExtendGrassDistance = false;
			*Config::DynDOLODGrassMode = 0;
		}

		switch (*Config::DynDOLODGrassMode)
		{
		case 1:
	        {
			    *Config::OverwriteGrassDistance = 999999.0f;
			    *Config::OverwriteGrassFadeRange = 0.0f;
			    *Config::ExtendGrassDistance = false;
	        }
		break;

		case 2:
		    {
			    *Config::OverwriteGrassDistance = 999999.0f;
			    *Config::OverwriteGrassFadeRange = 0.0f;
			    *Config::ExtendGrassDistance = true;
		    }
		break;
		}
		
		if (*Config::SuperDenseGrass)
		{
			// Make amount big.
			if(auto addr = RELOCATION_ID(15202, 15370).address() + (0xAE5 - 0x890); REL::make_pattern<"C1 E1 07">().match(RELOCATION_ID(15202, 15370).address() + (0xAE5 - 0x890)))
			{
			    int mode = std::max(0, std::min(12, static_cast<int>(*Config::SuperDenseMode)));
				if (mode != 7)
				{
					Memory::Internal::write<uint8_t>(addr + 2, static_cast<unsigned char>(mode), true);
				}
			}
		}

		if (*Config::ExtendGrassCount)
		{
			// Create more grass shapes if one becomes full.
			if(auto addr = RELOCATION_ID(15220, 15389).address() + (0x433 - 0x3C0); REL::make_pattern<"0F 84">().match(RELOCATION_ID(15220, 15389).address() + (0x433 - 0x3C0)))
			{
				Utility::Memory::SafeWrite(addr, Utility::Assembly::NoOperation6);
			}
			if(auto addr = RELOCATION_ID(15214, 15383).address() + (0x960 - 0x830); REL::make_pattern<"48 39 18 74 0A">().match(RELOCATION_ID(15214, 15383).address() + (0x960 - 0x830)))
			{
			//Util::Memory::WriteHook(new HookParameters(){ Address = addr, IncludeLength = 0, ReplaceLength = 5, Before = [&](std::any ctx)
				struct Patch : Xbyak::CodeGenerator
				{
					Patch(uintptr_t a_target)
					{
						Xbyak::Label notIf;
						Xbyak::Label retnLabel;
						Xbyak::Label jump;
					
						cmp(ptr[rax], rbx);
						jne(notIf);
						mov(rdx, ptr[rax + 8]);
						cmp(rdx, 0);
						je(notIf);
						mov(rdx, ptr[rdx]);
						cmp(rdx, 0);
						je(notIf);
						mov(ecx, ptr[rdx + 0x190]);
						imul(ecx, ptr[rdx + 0x194]);
						shl(ecx, 1);
						cmp(ecx, 0x20000);
						jge(notIf);
						jmp(ptr[rip + jump]);
						L(notIf);
						jmp(ptr[rip + retnLabel]);

						L(retnLabel);
						dq(a_target + 0x5);

						L(jump);
						dq(a_target + 0xF);
					}
				};
				Patch patch(addr);
				patch.ready();

				auto& trampoline = SKSE::GetTrampoline();
				trampoline.write_branch<5>(addr, trampoline.allocate(patch));
			}
			else
			{
				SKSE::stl::report_and_fail("Failed to create more grass shapes");
			}
		}

		if (*Config::UseGrassCache)
		{
			GidFileCache::FixFileFormat(*Config::OnlyLoadFromCache);
		}

		if (*Config::ExtendGrassDistance)
		{
			DistantGrass::ReplaceGrassGrid(*Config::OnlyLoadFromCache);
		}

		if (*Config::EnsureMaxGrassTypesPerTextureSetting > 0)
		{
			addr_MaxGrassPerTexture = RELOCATION_ID(501615, 360443).address();

			if(auto addr = RELOCATION_ID(18342, 18758).address() + (0xD63 - 0xCF0); REL::make_pattern<"44 8B 25">().match(RELOCATION_ID(18342, 18758).address() + (0xD63 - 0xCF0)))
			//Memory::WriteHook(new HookParameters(){ Address = addr, IncludeLength = 0, ReplaceLength = 7, Before = [&](std::any ctx)
			{
				uint32_t max = std::max(static_cast<int>(*Config::EnsureMaxGrassTypesPerTextureSetting), Memory::Internal::read<int>(addr_MaxGrassPerTexture + 8));

				struct Patch : Xbyak::CodeGenerator
				{
					Patch(uint32_t max, std::uintptr_t a_target)
					{
						Xbyak::Label retnLabel;

						mov(r12, max);
						jmp(ptr[rip + retnLabel]);

						L(retnLabel);
						dq(a_target + 0x7);
					}
				};
				Patch patch(max, addr);
				patch.ready();

				auto& trampoline = SKSE::GetTrampoline();
				Utility::Memory::SafeWrite(addr + 5, Utility::Assembly::NoOperation2);
				trampoline.write_branch<5>(addr, trampoline.allocate(patch));
			}
		}

		if (*Config::OverwriteGrassDistance >= 0.0f)
		{
			const float Distance = *Config::OverwriteGrassDistance;

			/*
			auto setting = RE::INISettingCollection::GetSingleton()->GetSetting("fGrassStartFadeDistance:Grass");
			setting->data.f = *Config::OverwriteGrassDistance;
			RE::INISettingCollection::GetSingleton()->WriteSetting(setting);
			*/
			uintptr_t addr;

			if(addr = RELOCATION_ID(528751, 15379).address(); REL::make_pattern<"F3 0F 10 05">().match(RELOCATION_ID(528751, 15379).address()))
			//Memory::WriteHook(new HookParameters(){ Address = addr, IncludeLength = 0, ReplaceLength = 8, Before = [&](std::any ctx)
			{
			    struct Patch : Xbyak::CodeGenerator
				{
					Patch(float distance, std::uintptr_t a_target)
					{
						Xbyak::Label retnLabel;

						mov(edx, distance);
						movd(xmm0, edx);

						jmp(ptr[rip + retnLabel]);

						L(retnLabel);
						dq(a_target + 0x8);
					}
				};
				Patch patch(Distance, addr);
				patch.ready();

				auto& trampoline = SKSE::GetTrampoline();
				Utility::Memory::SafeWrite(addr + 6, Utility::Assembly::NoOperation2);
				trampoline.write_branch<6>(addr, trampoline.allocate(patch));
			}
		
			if(addr = RELOCATION_ID(528751, 15379).address() + (0xC10 - 0xBE0); REL::make_pattern<"F3 0F 10 15">().match(RELOCATION_ID(528751, 15379).address() + (0xC10 - 0xBE0)))
			//Memory::WriteHook(new HookParameters(){ Address = addr, IncludeLength = 0, ReplaceLength = 8, Before = [&](std::any ctx)
			{
				struct Patch : Xbyak::CodeGenerator
				{
					Patch(float distance, uintptr_t a_target)
					{
						Xbyak::Label retnLabel;

						mov(edx, distance);
						movd(xmm2, edx);

						jmp(ptr[rip + retnLabel]);

						L(retnLabel);
						dq(a_target + 0x8);
					}
				};
				Patch patch(Distance, addr);
				patch.ready();

				auto& trampoline = SKSE::GetTrampoline();
				Utility::Memory::SafeWrite(addr + 6, Utility::Assembly::NoOperation2);
				trampoline.write_branch<6>(addr, trampoline.allocate(patch));
			}
			// WHY WONT YOU WORK
			
			if(addr = RELOCATION_ID(15210, 15370).address() + (0xBD - 0xA0); REL::make_pattern<"F3 0F 10 05">().match(RELOCATION_ID(15210, 15370).address() + (0xBD - 0xA0)))
			//Memory::WriteHook(new HookParameters(){ Address = addr, IncludeLength = 0, ReplaceLength = 8, Before = [&](std::any ctx)
			{
				struct Patch : Xbyak::CodeGenerator
				{
					Patch(float distance, uintptr_t a_target)
					{
						Xbyak::Label retnLabel;

						mov(edx, distance);
						movd(xmm0, edx);

						jmp(ptr[rip + retnLabel]);

						L(retnLabel);
						dq(a_target + 0x8);
					}
				};
				Patch patch(Distance, addr);
				patch.ready();

				auto& trampoline = SKSE::GetTrampoline();
				Utility::Memory::SafeWrite(addr, Utility::Assembly::NoOperation2);
				trampoline.write_branch<6>(addr, trampoline.allocate(patch));
			}
			
			if(addr = RELOCATION_ID(15202, 15370).address() + (0x4B1B - 0x4890); REL::make_pattern<"F3 0F 10 15">().match(RELOCATION_ID(15202, 15370).address() + (0x4B1B - 0x4890)))
			//Memory::WriteHook(new HookParameters(){ Address = addr, IncludeLength = 0, ReplaceLength = 8, Before = [&](std::any ctx)
			{
				struct Patch : Xbyak::CodeGenerator
				{
					Patch(float distance, std::uintptr_t a_target)
					{
						Xbyak::Label retnLabel;

						mov(edx, distance);
						movd(xmm2, edx);

						jmp(ptr[rip + retnLabel]);

						L(retnLabel);
						dq(a_target + 0x8);
					}
				};
				Patch patch(Distance, addr);
				patch.ready();

				auto& trampoline = SKSE::GetTrampoline();
				Utility::Memory::SafeWrite(addr + 6, Utility::Assembly::NoOperation2);
				trampoline.write_branch<6>(addr, trampoline.allocate(patch));
			}

			if(addr = RELOCATION_ID(15202, 15370).address() + (0x4AF3 - 0x4890); REL::make_pattern< "F3 0F 58 05">().match(RELOCATION_ID(15202, 15370).address() + (0x4AF3 - 0x4890)))
			//Memory::WriteHook(new HookParameters(){ Address = addr, IncludeLength = 0, ReplaceLength = 8, Before = [&](std::any ctx)
			{
				struct Patch : Xbyak::CodeGenerator
				{
					Patch(float distance, std::uintptr_t a_target)
					{
						Xbyak::Label retnLabel;

						movss(xmm4, xmm1);
						mov(eax, distance);
						movd(xmm1, eax);
						addss(xmm0, xmm1);
						movss(xmm1, xmm4);

						jmp(ptr[rip + retnLabel]);

						L(retnLabel);
						dq(a_target + 0x8);
					}
				};
				Patch patch(Distance, addr);
				patch.ready();

				auto& trampoline = SKSE::GetTrampoline();
				Utility::Memory::SafeWrite(addr + 6, Utility::Assembly::NoOperation2);
				trampoline.write_branch<6>(addr, trampoline.allocate(patch));
			}

			if(addr = RELOCATION_ID(15202, 15370).address() + (0x49F7 - 0x4890); REL::make_pattern<"F3 0F 10 05">().match(RELOCATION_ID(15202, 15370).address() + (0x49F7 - 0x4890)))
			//Memory::WriteHook(new HookParameters(){ Address = addr, IncludeLength = 0, ReplaceLength = 8, Before = [&](std::any ctx)
			{
				struct Patch : Xbyak::CodeGenerator
				{
					Patch(float distance, std::uintptr_t a_target)
					{
						Xbyak::Label retnLabel;

						mov(eax, distance);
						movd(xmm0, eax);

						jmp(ptr[rip + retnLabel]);

						L(retnLabel);
						dq(a_target + 0x8);
					}
				};
				Patch patch(Distance, addr);
				patch.ready();

				auto& trampoline = SKSE::GetTrampoline();
				Utility::Memory::SafeWrite(addr + 6, Utility::Assembly::NoOperation2);
				trampoline.write_branch<6>(addr, trampoline.allocate(patch));
			}
			
		}

		if (*Config::OverwriteGrassFadeRange >= 0.0f)
		{
			/*
			auto setting = RE::INISettingCollection::GetSingleton()->GetSetting("fGrassFadeRange:Grass");
			setting->data.f = *Config::OverwriteGrassFadeRange;
			RE::INISettingCollection::GetSingleton()->WriteSetting(setting);
			*/
		
			if(auto addr = RELOCATION_ID(15202, 15370).address() + (0xAEB - 0x890); REL::make_pattern<"F3 0F 10 05">().match(RELOCATION_ID(15202, 15370).address() + (0xAEB - 0x890)))
			//Memory::WriteHook(new HookParameters(){ Address = addr, IncludeLength = 0, ReplaceLength = 8, Before = [&](std::any ctx)
			{
				float Range = *Config::OverwriteGrassFadeRange;

				struct Patch : Xbyak::CodeGenerator
				{
					Patch(float range, std::uintptr_t a_target)
					{
						Xbyak::Label retnLabel;

						push(rax);
						mov(eax, range);
						movd(xmm0, eax);
						pop(rax);

						jmp(ptr[rip + retnLabel]);

						L(retnLabel);
						dq(a_target + 0x8);
					}
				};
				Patch patch(Range, addr);
				patch.ready();

				auto& trampoline = SKSE::GetTrampoline();
				Utility::Memory::SafeWrite(addr + 5, Utility::Assembly::NoOperation3);
				trampoline.write_branch<5>(addr, trampoline.allocate(patch));
			}

			if(auto addr = RELOCATION_ID(528751, 15379).address() + 0xB; REL::make_pattern<"F3 0F 58 05">().match(RELOCATION_ID(528751, 15379).address() + 0xB))
			//Memory::WriteHook(new HookParameters(){ Address = addr, IncludeLength = 0, ReplaceLength = 8, Before = [&](std::any ctx)
			{
				float Range = *Config::OverwriteGrassFadeRange;

				struct Patch : Xbyak::CodeGenerator
				{
					Patch(float range, std::uintptr_t a_target)
					{
						Xbyak::Label retnLabel;

						push(rax);
						movss(xmm4, xmm1);
						mov(eax, range);
						movd(xmm1, eax);
						addss(xmm0, xmm1);
						pop(rax);
						movss(xmm1, xmm4);

						jmp(ptr[rip + retnLabel]);

						L(retnLabel);
						dq(a_target + 0x8);
					}
				};
				Patch patch(Range, addr);
				patch.ready();

				auto& trampoline = SKSE::GetTrampoline();
				Utility::Memory::SafeWrite(addr + 5, Utility::Assembly::NoOperation3);
				trampoline.write_branch<5>(addr, trampoline.allocate(patch));
			}
			
		}

		if (*Config::OverwriteMinGrassSize >= 0)
		{
			auto setting = RE::INISettingCollection::GetSingleton()->GetSetting("iMinGrassSize:Grass");
			setting->data.i = *Config::OverwriteMinGrassSize;
			RE::INISettingCollection::GetSingleton()->WriteSetting(setting);

			/*
			if(auto addr = RELOCATION_ID(15202, 15370).address() + (0x4B4E - 0x4890); REL::make_pattern<"66 0F 6E 05">().match(RELOCATION_ID(15202, 15370).address() + (0x4B4E - 0x4890)))
			//Memory::WriteHook(new HookParameters(){ Address = addr, IncludeLength = 0, ReplaceLength = 8 + 3, Before = [&](std::any ctx)
			{
				uint32_t Max = std::max(1, static_cast<int>(*Config::OverwriteMinGrassSize));

				struct Patch : Xbyak::CodeGenerator
				{
					Patch(uint32_t max, std::uintptr_t a_target)
					{
						Xbyak::Label retnLabel;

						mov(edx, max);
						movd(xmm0, edx);

						jmp(ptr[rip + retnLabel]);

						L(retnLabel);
						dq(a_target + 0xB);
					}
				};
				Patch patch(Max, addr);
				patch.ready();

				auto& trampoline = SKSE::GetTrampoline();
				Utility::Memory::SafeWrite(addr + 5, Utility::Assembly::NoOperation6);
				trampoline.write_branch<5>(addr, trampoline.allocate(patch));
			} 

			if(auto addr = RELOCATION_ID(15212, 15381).address() + (0x6DBB - 0x6CE0); REL::make_pattern<"66 0F 6E 0D">().match(RELOCATION_ID(15212, 15381).address() + (0x6DBB - 0x6CE0)))
			//Memory::WriteHook(new HookParameters(){ Address = addr, IncludeLength = 0, ReplaceLength = 8 + 3, Before = [&](std::any ctx)
			{
				uint32_t Max = std::max(1, static_cast<int>(*Config::OverwriteMinGrassSize));

				struct Patch : Xbyak::CodeGenerator
				{
					Patch(uint32_t max, std::uintptr_t a_target)
					{
						Xbyak::Label retnLabel;

						mov(eax, max);
						movd(xmm1, eax);

						jmp(ptr[rip + retnLabel]);

						L(retnLabel);
						dq(a_target + 0xB);
					}
				};
				Patch patch(Max, addr);
				patch.ready();

				auto& trampoline = SKSE::GetTrampoline();
				Utility::Memory::SafeWrite(addr + 5, Utility::Assembly::NoOperation6);
				trampoline.write_branch<5>(addr, trampoline.allocate(patch));
			}
			*/
		}

		if (*Config::GlobalGrassScale != 1.0f && *Config::GlobalGrassScale > 0.0001f)
		{
			auto floatAlloc = malloc(8);
			Memory::Internal::write<float>(reinterpret_cast<uintptr_t>(floatAlloc), *Config::GlobalGrassScale);

			unsigned long long vid = 15212;
			int baseOffset = 0x6CE0;

			std::vector<unsigned char> fcode;
			{
				std::vector<unsigned char> code = { 0xF3, 0x41, 0x0F, 0x59, 0xC6, 0xF3, 0x41, 0x0F, 0x5C, 0xC3, 0xF3, 0x0F, 0x59, 0x46, 0x10, 0xF3, 0x41, 0x0F, 0x58, 0xC3, 0x48, 0xB9 };
				auto floatPtr = static_cast<float*>(floatAlloc);
				auto Array1 = to_vector(*floatPtr);
				code.insert(code.end(), Array1.begin(), Array1.end());
				auto Array2 = std::vector<unsigned char> {0xF3, 0x0F, 0x59, 0x01, 0xF3, 0x41, 0x0F, 0x5C, 0xC3, 0x48, 0xB9};
				code.insert(code.end(), Array2.begin(), Array2.end());
				std::vector<unsigned char> Array3;
				if(REL::make_pattern<"E8">().match(vid + (0x7629 - baseOffset)))
				{
					Array3 = to_vector(vid + (0x7629 - baseOffset));
				}
				code.insert(code.end(), Array3.begin(), Array3.end());
				auto Array4 = std::vector<unsigned char> {0xFF, 0xE1};
				code.insert(code.end(), Array4.begin(), Array4.end());

				fcode = code;
			}

			/*
			; xmm0 is the final scale - 1.0f
			; xmm11 is 1.0f

			; original game code

			mulss   xmm0, xmm14
			subss   xmm0, xmm11
			mulss   xmm0, dword ptr [rsi+0x10]

			; our code to apply extra multiplier

			addss xmm0, xmm11
			movabs rcx, 0
			mulss xmm0, dword ptr [rcx]
			subss xmm0, xmm11

			; continue execution in game code

			movabs rcx, 0
			jmp rcx
			*/

			auto alloc = malloc(fcode.size() + 0x10);

			Utility::Memory::SafeWrite(reinterpret_cast<uintptr_t>(alloc), fcode, true);

			if(auto addr = vid + (0x761A - baseOffset); REL::make_pattern<"F3 41 0F 59 C6 F3 41 0F 5C C3 F3 0F 59 46 10">().match(vid + (0x761A - baseOffset)))
			{
				auto wr = std::vector<unsigned char>{ 0x48, 0xB9 };
				auto floatPtr = static_cast<float*>(alloc);
				auto array1 = to_vector(*floatPtr);
				wr.insert(wr.end(), array1.begin(), array1.end());
				auto array2 = std::vector<unsigned char>{ 0xFF, 0xE1, 0x90, 0x90, 0x90 };
				wr.insert(wr.end(), array2.begin(), array2.end());
				//.insert(BitConverter::GetBytes((intptr_t)alloc)).insert(std::vector<unsigned char> {0xFF, 0xE1, 0x90, 0x90, 0x90});
				if (wr.size() != 15)
				{
					throw std::invalid_argument("GrassControl:GlobalGrassScale (wr.Length != 15)");
				}
				Utility::Memory::SafeWrite(addr, wr, true);
			}
		}
	}

	void GrassControlPlugin::warn_extend_without_cache()
	{
		auto ls = std::vector<std::string>();
		ls.emplace_back("Warning!! You have enabled ExtendGrassDistance without using pre-generated grass. This could lead to unstable game. Either disable ExtendGrassDistance or pre-generate grass cache files. In order to use pre-generated grass cache you will need UseGrassCache=True and OnlyLoadFromCache=True");
		ls.emplace_back("Check nexus page of 'No Grass In Objects' mod for more information on how to do this.");
		//ls.Add("This warning won't be shown again next time you start game.");

		try
		{
			auto fi = std::filesystem::path("Data/SKSE/Plugins/GrassControl.warned.txt");
			if (exists(fi))
				return;

			std::ofstream sw;
			sw.open(fi);
			sw << "Dummy file to track whether the following warning was shown: \n";
			sw << "\n";
			std::stringstream s;
			std::string Newline = "\r\n";
			std::string NewNewLine = "\r\n" + Newline;
			std::ranges::copy(ls.begin(), ls.end(), std::ostream_iterator<std::string, char>(s, NewNewLine.c_str()));
			sw << s.str();
		}
		catch (...)
		{

		}
		std::stringstream s;
		std::ranges::copy(ls.begin(), ls.end(), std::ostream_iterator<std::string, char>(s, "\n\n"));
		log::debug(s.str());
		RE::DebugMessageBox(s.str().c_str());
	}
}
