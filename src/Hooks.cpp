#include "GrassControl/Main.h"
using namespace Xbyak;
namespace GrassControl
{
	static float SetScale(float a_input)
	{
		// we need to only interact with floats so the input/ouput register xmm0 is proper precision
		return static_cast<float>(a_input * Config::GlobalGrassScale);
	}

	bool GrassControlPlugin::CanPlaceGrassWrapper(RE::TESObjectLAND* land, const float x, const float y, const float z)
	{
		if (land != nullptr) {
			if (Cache != nullptr && !Cache->CanPlaceGrass(land, x, y, z)) {
				return false;
			}
		}
		return true;
	}

	std::intptr_t GrassControlPlugin::addr_MaxGrassPerTexture;
	int GrassControlPlugin::_did_mainMenu = 0;

	void GrassControlPlugin::Update()
	{
		if (!Config::UseGrassCache)
			return;

		if (_did_mainMenu == 0)
			return;

		if (_did_mainMenu == 1) {
			_did_mainMenu++;
		} else if (_did_mainMenu == 2) {
			_did_mainMenu++;

			GidFileGenerationTask::cur_state = 1;

			if (Config::FreezeCheck) {
				GidFileGenerationTask::_lastDidSomething = GetTickCount64();
				std::thread t(GidFileGenerationTask::run_freeze_check);
				t.detach();
			}

			auto gf = std::make_unique<GidFileGenerationTask>();
			GidFileGenerationTask::cur_instance = std::move(gf);
		}
	}

	void GrassControlPlugin::OnMainMenuOpen()
	{
		auto fi = std::filesystem::path(Util::getProgressFilePath());
		if (Config::UseGrassCache && exists(fi)) {
			_did_mainMenu = 1;
			Memory::Internal::write<uint8_t>(RELOCATION_ID(508798, 380767).address() + 8, 1);  // Skyrim.ini [General] bAlwaysActive=1
			Memory::Internal::write<uint8_t>(RELOCATION_ID(501125, 359439).address() + 8, 0);  // Skyrim.ini [Grass] bAllowLoadGrass=0
		}

		if (Config::RayCast) {
			std::string formsStr = Config::RayCastIgnoreForms;
			auto cachedList = Util::CachedFormList::TryParse(formsStr, "Ray-cast-ignore-forms", true, false);
			if (cachedList != nullptr && cachedList->getAll().empty()) {
				cachedList = nullptr;
			}

			std::string textureFormsStr = Config::RayCastTextureForms;
			auto cachedTextureList = Util::CachedFormList::TryParse(textureFormsStr, "Ray-cast-texture-forms", true, false);
			if (cachedTextureList != nullptr && cachedTextureList->getAll().empty()) {
				cachedTextureList = nullptr;
			}

			Cache = std::make_unique<RaycastHelper>(static_cast<int>(std::stof(SKSE::PluginDeclaration::GetSingleton()->GetVersion().string())), static_cast<float>(Config::RayCastHeight), static_cast<float>(Config::RayCastDepth), Config::RayCastCollisionLayers, cachedList, cachedTextureList);
			logger::info("Created Cache for Raycasting Settings");
		}

		if (Config::ExtendGrassDistance) {
			if (!Config::UseGrassCache || !Config::OnlyLoadFromCache) {
				warn_extend_without_cache();
			}
		}
	}

	void GrassControlPlugin::init()
	{
		std::stringstream s;
		auto list = { std::to_string(static_cast<int>(RE::COL_LAYER::kStatic)), std::to_string(static_cast<int>(RE::COL_LAYER::kAnimStatic)), std::to_string(static_cast<int>(RE::COL_LAYER::kTerrain)), std::to_string(static_cast<int>(RE::COL_LAYER::kDebrisLarge)), std::to_string(static_cast<int>(RE::COL_LAYER::kStairHelper)) };
		std::ranges::copy(list.begin(), list.end(), std::ostream_iterator<std::string, char>(s, " "));

		Config::RayCastCollisionLayers = s.str();

		auto fi = std::filesystem::path(Util::getProgressFilePath());
		if (Config::UseGrassCache && exists(fi)) {
#ifdef SKYRIMVR
			auto setting = RE::INISettingCollection::GetSingleton()->GetSetting("bLoadVRPlayroom:VR");
			if (!setting) {
				setting = RE::INIPrefSettingCollection::GetSingleton()->GetSetting("bLoadVRPlayroom:VR");
				if (!setting) {
					logger::error("Failed to find bLoadVRPlayroom");
					return;
				}
			}
			setting->data.b = false;
#endif

			Config::OnlyLoadFromCache = false;

			GidFileGenerationTask::apply();

			Config::ExtendGrassDistance = false;
			Config::DynDOLODGrassMode = 0;
		} else if (Config::UseGrassCache) {
			logger::info("Grass Cache is Enabled. PrecacheGrass.txt is not detected, assuming normal usage.");
		} else {
			logger::info("Grass Cache is Disabled");
		}

		switch (Config::DynDOLODGrassMode) {
		case 1:
			{
				Config::OverwriteGrassDistance = 999999.0;
				Config::OverwriteGrassFadeRange = 0.0;
				Config::ExtendGrassDistance = false;
			}
			break;

		case 2:
			{
				Config::OverwriteGrassDistance = 999999.0;
				Config::OverwriteGrassFadeRange = 0.0;
				Config::ExtendGrassDistance = true;
			}
			break;
		default:
			break;
		}

		if (Config::RayCast) {
			auto addr = RELOCATION_ID(15212, 15381).address() + REL::Relocate((0x723A - 0x6CE0), 0x664, 0x56C);
			struct Patch : Xbyak::CodeGenerator
			{
				Patch(std::uintptr_t b_func, std::uintptr_t a_target,
					std::uintptr_t a_rspOffset,
					Xbyak::Reg a_z,
					Xbyak::Reg a_rcxSource,
					std::uintptr_t a_rbpOffset,
					std::uintptr_t a_targetJumpOffset)
				{
					Xbyak::Label funcLabel;
					Xbyak::Label retnLabel;

					Xbyak::Label jump;
					Xbyak::Label notIf;
					movss(xmm1, ptr[rsp + a_rspOffset]);        // x
					movss(xmm2, ptr[rsp + a_rspOffset + 0x4]);  // y
					movss(xmm3, a_z);                           // z

					mov(rcx, a_rcxSource);

					sub(rsp, 0x30);
					call(ptr[rip + funcLabel]);  // call our function
					add(rsp, 0x30);

					test(al, al);
					jne(notIf);
					jmp(ptr[rip + jump]);

					L(notIf);
					movss(xmm6, ptr[rbp - a_rbpOffset]);
					jmp(ptr[rip + retnLabel]);

					L(jump);
					dq(a_target + a_targetJumpOffset);

					L(funcLabel);
					dq(b_func);

					L(retnLabel);
					dq(a_target + 0x5);
				}
			};
			Patch patch(reinterpret_cast<uintptr_t>(CanPlaceGrassWrapper), addr,
				REL::Relocate(0x40, 0x50, 0x50),
				Xmm(REL::Relocate(7, 7, 14)),
				Reg64(REL::Relocate(Reg::RSI, Reg::RDI, Reg::RBX)),
				REL::Relocate(0x48, 0x68, 0x38),
				REL::Relocate(0x5 + (0x661 - 0x23F), -0x156, 0x5 + 0x510));
			patch.ready();

			auto& trampoline = SKSE::GetTrampoline();
			trampoline.write_branch<5>(addr, trampoline.allocate(patch));
		}

		if (Config::SuperDenseGrass) {
			// Make amount big.
			auto addr = RELOCATION_ID(15202, 15370).address() + REL::Relocate(0xAE5 - 0x890, 0x258);
			int mode = std::max(0, std::min(12, static_cast<int>(Config::SuperDenseMode)));
			if (mode != 7) {
				Memory::Internal::write<uint8_t>(addr + 2, static_cast<unsigned char>(mode), true);
			}
		}

		if (Config::ExtendGrassCount) {
			// Create more grass shapes if one becomes full.
			if (auto addr = RELOCATION_ID(15220, 15385).address() + REL::Relocate(0x433 - 0x3C0, 0x68); REL::make_pattern<"0F 84">().match(addr)) {
				Utility::Memory::SafeWrite(addr, Utility::Assembly::NoOperation6);
			}
			if (auto addr = RELOCATION_ID(15214, 15383).address() + REL::Relocate(0x960 - 0x830, 0x129); REL::make_pattern<"48 39 18 74 0A">().match(addr)) {
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
			} else {
				stl::report_and_fail("Failed to create more grass shapes");
			}
		}

		if (Config::UseGrassCache) {
			GidFileCache::FixFileFormat(Config::OnlyLoadFromCache);
		}

		if (Config::ExtendGrassDistance) {
			DistantGrass::InstallHooks();
			DistantGrass::ReplaceGrassGrid(Config::OnlyLoadFromCache);
		}

		if (Config::EnsureMaxGrassTypesPerTextureSetting > 0) {
			addr_MaxGrassPerTexture = RELOCATION_ID(501615, 360443).address();

			if (auto addr = RELOCATION_ID(18342, 18758).address() + REL::Relocate(0xD63 - 0xCF0, 0x68); REL::make_pattern<"44 8B 25">().match(addr))
			//Memory::WriteHook(new HookParameters(){ Address = addr, IncludeLength = 0, ReplaceLength = 7, Before = [&](std::any ctx)
			{
				uint32_t max = std::max(static_cast<int>(Config::EnsureMaxGrassTypesPerTextureSetting), Memory::Internal::read<int>(addr_MaxGrassPerTexture + 8));

				struct Patch : Xbyak::CodeGenerator
				{
					Patch(uint32_t max, std::uintptr_t a_target)
					{
						Xbyak::Label retnLabel;

#ifdef SKYRIM_AE
						mov(edi, max);
#else
						mov(r12d, max);
#endif
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

		if (Config::OverwriteGrassDistance >= 0.0) {
			auto setting = RE::INISettingCollection::GetSingleton()->GetSetting("fGrassStartFadeDistance:Grass");
			if (!setting) {
				setting = RE::INIPrefSettingCollection::GetSingleton()->GetSetting("fGrassStartFadeDistance:Grass");
				if (!setting) {
					logger::error("Failed to find fGrassStartFadeDistance");
					return;
				}
			}
			setting->data.f = static_cast<float>(Config::OverwriteGrassDistance);
		}

		if (Config::OverwriteGrassFadeRange >= 0.0) {
			auto setting = RE::INISettingCollection::GetSingleton()->GetSetting("fGrassFadeRange:Grass");
			if (!setting) {
				setting = RE::INIPrefSettingCollection::GetSingleton()->GetSetting("fGrassFadeRange:Grass");
				if (!setting) {
					logger::error("Failed to find fGrassFadeRange");
					return;
				}
			}
			setting->data.f = static_cast<float>(Config::OverwriteGrassFadeRange);
		}

		if (Config::OverwriteMinGrassSize >= 0) {
			auto setting = RE::INISettingCollection::GetSingleton()->GetSetting("iMinGrassSize:Grass");
			if (!setting) {
				setting = RE::INIPrefSettingCollection::GetSingleton()->GetSetting("iMinGrassSize:Grass");
				if (!setting) {
					logger::error("Failed to find iMinGrassSize");
					return;
				}
			}
			setting->data.i = Config::OverwriteMinGrassSize;
		}

		if (Config::GlobalGrassScale != 1.0 && Config::GlobalGrassScale > 0.0001) {
			auto addr = RELOCATION_ID(15212, 15381).address() + REL::Relocate(0x93a, 0xab4, 0xa0c);
			struct Patch : Xbyak::CodeGenerator
			{
				Patch(uintptr_t a_target, uintptr_t a_func,
					Xbyak::Xmm a_sourceReg_4656,  // 4.656613e-10
					Xbyak::Xmm a_sourceReg_1,     // 1.0
					Xbyak::Reg a_baseReg)
				{
					Xbyak::Label funcLabel;
					Xbyak::Label retnLabel;

					mulss(xmm0, a_sourceReg_4656);       // finalScale *= 4.656613e-10; // original code
					subss(xmm0, a_sourceReg_1);          // xmm0 -= 1; original code
					mulss(xmm0, ptr[a_baseReg + 0x10]);  // original code
					addss(xmm0, a_sourceReg_1);          // finalScale = xmm0 + 1;

					// function call
					sub(rsp, 0x20);
					mov(al, 1);                  // one float parameter
					call(ptr[rip + funcLabel]);  // finalScale = SetScale(finalScale);
					add(rsp, 0x20);
					subss(xmm0, a_sourceReg_1);  // xmm0 = finalScale - 1; // original code
					jmp(ptr[rip + retnLabel]);

					L(funcLabel);
					dq(a_func);

					L(retnLabel);
					dq(a_target);
				}
			};
			Patch patch(addr + REL::Relocate(0xf, 0xf, 0x10), reinterpret_cast<uintptr_t>(SetScale), Xbyak::Xmm(REL::Relocate(14, 14, 12)), Xbyak::Xmm(REL::Relocate(11, 10, 11)), Xbyak::Reg64(REL::Relocate(Reg::RSI, Reg::RBX, Reg::R13)));
			patch.ready();

			auto& trampoline = SKSE::GetTrampoline();
			trampoline.write_branch<5>(addr, trampoline.allocate(patch));
		}
	}

	void GrassControlPlugin::warn_extend_without_cache()
	{
		auto ls = std::vector<std::string>();
		ls.emplace_back("Warning!! You have enabled Extend-Grass-Distance without using pre-generated grass. This could lead to unstable game. Either disable Extend-Grass-Distance or pre-generate grass cache files. In order to use pre-generated grass cache you will need UseGrassCache=True and Only-Load-From-Cache=true");
		ls.emplace_back("Check nexus page of 'No Grass In Objects' mod for more information on how to do this.");
		ls.emplace_back("This warning won't be shown again next time you start game.");

		try {
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
		} catch (...) {
		}
		std::stringstream s;
		std::ranges::copy(ls.begin(), ls.end(), std::ostream_iterator<std::string, char>(s, "\n\n"));
		logger::debug(fmt::runtime(s.str()));
		RE::DebugMessageBox(s.str().c_str());
	}
}
