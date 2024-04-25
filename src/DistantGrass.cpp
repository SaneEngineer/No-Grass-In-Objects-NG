#include "GrassControl/DistantGrass.h"

namespace GrassControl
{
	uintptr_t DistantGrass::addr_uGrids = RELOCATION_ID(501244, 359675).address();
	uintptr_t DistantGrass::addr_AllowLoadFile = RELOCATION_ID(501125, 359439).address();
	uintptr_t DistantGrass::addr_DataHandler = RELOCATION_ID(514141, 400269).address();
	uintptr_t DistantGrass::addr_uLargeRef = RELOCATION_ID(501554, 360374).address();
	uintptr_t DistantGrass::addr_QueueLoadCellUnkGlobal = RELOCATION_ID(514741, 400899).address();

	DistantGrass::cell_info::cell_info(const int _x, const int _y) :
		x(_x), y(_y)
	{
	}

	DistantGrass::cell_info::cell_info() :
		x(0), y(0)
	{
	}

	bool DistantGrass::cell_info::checkHasFile(const std::string& wsName, const bool lod) const
	{
		if (wsName.empty()) {
			return false;
		}

		std::string x_str = x < 0 ? std::string(3, '0').append(std::to_string(x)) : std::string(4, '0').append(std::to_string(x));
		std::string y_str = y < 0 ? std::string(3, '0').append(std::to_string(y)) : std::string(4, '0').append(std::to_string(y));
		std::string fpath = "Data/Grass/" + wsName + "x" + x_str + "y" + y_str;

		if (lod) {
			fpath = fpath + ".dgid";
		} else {
			fpath = fpath + ".cgid";
		}

		bool has = false;
		try {
			if (std::filesystem::exists(fpath)) {
				has = true;
			}
		} catch (...) {
		}

		return has;
	}

	DistantGrass::CellInfoContainer::CellInfoContainer() :
		grid(std::vector<cell_info>(16641))  // 129 * 129
	{
		for (int x = 0; x <= 128; x++) {
			for (int y = 0; y <= 128; y++) {
				grid[x * 129 + y] = cell_info(x - 64, y - 64);
			}
		}
	}

	std::optional<DistantGrass::cell_info> DistantGrass::CellInfoContainer::GetFromGrid(int x, int y) const
	{
		if (x < -64 || y < -64 || x > 64 || y > 64)
			return {};

		x += 64;
		y += 64;
		return grid[x * 129 + y];
	}

	void DistantGrass::CellInfoContainer::unsafe_ForeachWithState(const std::function<bool(cell_info)>& action)
	{
		try {
			for (auto& [fst, snd] : map) {
				if (!action(snd)) {
					map.erase(fst);
				}
			}
		} catch (...) {
			//Terrible idea but fuck it. Erase keeps throwing a read access violation.
		}
	}

	std::optional<DistantGrass::cell_info> DistantGrass::CellInfoContainer::FindByCell(RE::TESObjectCELL* cell)
	{
		{
			auto it = this->map.find(cell);
			if (it != this->map.end()) {
				return it->second;
			}
			return {};
		}
	}

	int DistantGrass::LoadOnlyCellInfoContainer2::GetCount() const
	{
		int count = 0;
		{
			std::scoped_lock lock(NRlocker());
			for (const auto& val : map | std::views::values) {
				if (val->State != _cell_data::_cell_states::None) {
					count++;
				}
			}
		}
		return count;
	}

	std::string DistantGrass::LoadOnlyCellInfoContainer2::MakeKey(const std::string& ws, int x, int y)
	{
		std::string rtn;
		try {
			rtn = fmt::format("{}_{}_{}", x, y, ws);
		} catch (fmt::format_error& e) {
			logger::error("Error occured while extending Grass Distance:");
			logger::error(fmt::runtime(e.what()));
		}

		return rtn;
	}

	void DistantGrass::LoadOnlyCellInfoContainer2::UpdatePositionWithRemove(RE::TESWorldSpace* ws, int addType, int nowX, int nowY, int grassRadius)
	{
		unsigned int wsId = ws != nullptr ? ws->formID : 0;

		{
			std::scoped_lock lock(locker());
			for (const auto& val : this->map | std::views::values) {
				if (val->State == _cell_data::_cell_states::None) {
					continue;
				}

				bool everythingIsFineHereNothingToDoFurther = addType >= 0 && val->WsId == wsId && std::abs(val->X - nowX) <= grassRadius && std::abs(val->Y - nowY) <= grassRadius;
				if (everythingIsFineHereNothingToDoFurther) {
					continue;
				}

				_DoUnload(val);
			}
		}
	}

	void DistantGrass::LoadOnlyCellInfoContainer2::QueueLoad(RE::TESWorldSpace* ws, int x, int y)
	{
		if (ws == nullptr)
			return;

		std::string key = MakeKey(ws->editorID.c_str(), x, y);
		if (key.empty())
			return;

		std::shared_ptr<_cell_data> d;
		{
			std::scoped_lock lock(NRlocker());
			if (!this->map.contains(key)) {
				d = std::make_shared<_cell_data>();
				this->map.insert_or_assign(key, d);

				d->X = x;
				d->Y = y;
				d->WsId = ws != nullptr ? ws->formID : 0;
			}
		}

		if (d == nullptr)
			return;

		{
			std::scoped_lock lock(NRlocker());
			if (d->State != _cell_data::_cell_states::None) {
				return;
			}

			d->State = _cell_data::_cell_states::Loading;

			if (d->DummyCell_Ptr == nullptr) {
				auto tempPtr = new char[sizeof(RE::TESObjectCELL)];
				memset(tempPtr, 0, sizeof(RE::TESObjectCELL));

				d->DummyCell_Ptr = reinterpret_cast<RE::TESObjectCELL*>(tempPtr);

				auto exteriorData = new RE::EXTERIOR_DATA();
				exteriorData->cellX = x;
				exteriorData->cellY = y;

				d->DummyCell_Ptr->cellData.exterior = exteriorData;
				d->DummyCell_Ptr->worldSpace = ws;
			}

			using func_t = void (*)(RE::TESObjectCELL*);
			REL::Relocation<func_t> func{ RELOCATION_ID(13137, 13277) };

			func(d->DummyCell_Ptr);
		}
	}

	void DistantGrass::LoadOnlyCellInfoContainer2::_DoUnload(const std::shared_ptr<_cell_data>& d)
	{
		if (d == nullptr)
			return;

		{
			std::scoped_lock lock(NRlocker());
			if (d->State == _cell_data::_cell_states::None || d->State == _cell_data::_cell_states::Abort) {
				return;
			}

			if (d->State == _cell_data::_cell_states::Loading) {
				d->State = _cell_data::_cell_states::Abort;
				return;
			}
			REL::Relocation<void (*)(RE::BGSGrassManager*, RE::TESObjectCELL*)> func{ RELOCATION_ID(15207, 15375) };

			func(RE::BGSGrassManager::GetSingleton(), d->DummyCell_Ptr);
			REL::Relocation<void (*)(RE::TESObjectCELL*, uintptr_t)> func2{ RELOCATION_ID(11932, 12071) };
			func2(d->DummyCell_Ptr, 0);

			delete d->DummyCell_Ptr->cellData.exterior;

			d->DummyCell_Ptr = nullptr;
			d->State = _cell_data::_cell_states::None;

			for (auto it = map.begin(); it != map.end(); ++it) {
				if (it->second == d) {
					map.erase(it);
					break;
				}
			}
		}
	}

	void DistantGrass::LoadOnlyCellInfoContainer2::Unload(const RE::TESWorldSpace* ws, const int x, const int y)
	{
		if (ws == nullptr)
			return;

		std::string key = MakeKey(ws->editorID.c_str(), x, y);
		std::shared_ptr<_cell_data> d;
		{
			std::scoped_lock lock(NRlocker());
			if (!this->map.empty()) {
				auto it = this->map.find(key);
				d = it == this->map.end() ? nullptr : it->second;
			}
		}

		if (d == nullptr)
			return;

		_DoUnload(d);
	}

	void DistantGrass::LoadOnlyCellInfoContainer2::_DoLoad(const RE::TESWorldSpace* ws, const int x, const int y) const
	{
		if (ws == nullptr)
			return;

		std::string key = MakeKey(ws->editorID.c_str(), x, y);
		std::shared_ptr<_cell_data> d;
		{
			std::scoped_lock lock(NRlocker());
			auto it = this->map.find(key);
			d = it == this->map.end() ? nullptr : it->second;
		}

		if (d == nullptr)
			return;

		{
			std::scoped_lock lock(NRlocker());
			if (d->State == _cell_data::_cell_states::Loaded) {
				// This shouldn't happen
			} else if (d->State == _cell_data::_cell_states::Loading) {
				REL::Relocation<void (*)(RE::BGSGrassManager*, RE::TESObjectCELL*)> func{ RELOCATION_ID(15206, 15374) };

				if (d->DummyCell_Ptr == nullptr)
					return;

				func(RE::BGSGrassManager::GetSingleton(), d->DummyCell_Ptr);
				d->State = _cell_data::_cell_states::Loaded;
			} else if (d->DummyCell_Ptr != nullptr) {
				d->DummyCell_Ptr = nullptr;
				d->State = _cell_data::_cell_states::None;
			} else {
				d->State = _cell_data::_cell_states::None;  // this shouldn't happen?
			}
		}
	}

	void DistantGrass::RemoveGrassHook(RE::TESObjectCELL* cell, uintptr_t arg_1)
	{
		if (cell != nullptr) {
			auto c = Map->FindByCell(cell);
			if (c && (c->self_data & 0xFF) != 0) {
				logger::debug("RemoveGrassDueToAdd({}, {})", c->x, c->y);

				REL::Relocation<void (*)(uintptr_t, RE::TESObjectCELL*)> func{ RELOCATION_ID(15207, 15375) };
				func(arg_1, cell);
			}

			if (!c) {
				logger::debug("RemoveGrassDueToAdd(null): warning: c == null");
			}
		} else if (Config::DebugLogEnable) {
			logger::debug("RemoveGrassDueToAdd(null)");
		}
	}

	void DistantGrass::CellUnloadHook(bool did, RE::TESObjectCELL* cellObj)
	{
		logger::debug("DataHandler::UnloadCell({}, {}) did: {}", cellObj->GetCoordinates()->cellX, cellObj->GetCoordinates()->cellY, did);
	}

	void ThrowOurMethodException(const uint8_t ourMethod)
	{
		stl::report_and_fail("GrassControl.dll: unexpected ourMethod: " + std::to_string(ourMethod));
	}

	unsigned char DistantGrass::CellLoadHook(const int x, const int y)
	{
		unsigned char isOurMethod = 0;
		auto c = Map->GetFromGrid(x, y);
		if (c && InterlockedCompareExchange64(&c->furtherLoad, 2, 1) == 1) {
			isOurMethod = 1;
		}
		return isOurMethod;
	}

	void DistantGrass::ReplaceGrassGrid(const bool _loadOnly)
	{
		DidApply = true;
		load_only = _loadOnly;

		if (load_only) {
			LO2Map = std::make_unique<LoadOnlyCellInfoContainer2>();
			logger::debug("LO2Map created at {:x}", reinterpret_cast<uintptr_t>(LO2Map.get()));
		} else {
			Map = std::make_unique<CellInfoContainer>();
			logger::debug("Map created at {:x}", reinterpret_cast<uintptr_t>(Map.get()));
		}

		// Disable grass fade.
		if (!load_only) {
			auto addr = RELOCATION_ID(38141, 39098).address();
			Memory::Internal::write<uint8_t>(addr, 0xC3, true);

			// There's a chance this below isn't actually used so if it fails no problem.
			try {
				if (addr = (RELOCATION_ID(13240, 13391).address() + (0x55E - 0x480)); REL::make_pattern<"E8">().match(addr)) {
					Utility::Memory::SafeWrite(addr, Utility::Assembly::NoOperation5);
				}
			} catch (...) {
			}
		}

		// Auto use correct iGrassCellRadius.
		if (!load_only) {
			int radius = getChosenGrassGridRadius();
			auto setting = RE::INISettingCollection::GetSingleton()->GetSetting("iGrassCellRadius:Grass");
			setting->data.i = radius;
		}

		// Unload old grass, load new grass. But we have replaced how the grid works now.
		uintptr_t addr;

		if (addr = (RELOCATION_ID(13148, 13288).address() + OFFSET(0xA06 - 0x220, 0xA1D)); REL::make_pattern<"8B 3D">().match(addr)) {
			//Memory::WriteHook(new HookParameters() { Address = addr, IncludeLength = 0, ReplaceLength = 6, Before = [&] (std::any ctx)

			struct Patch : Xbyak::CodeGenerator
			{
				Patch(std::uintptr_t a_func, uintptr_t a_target)
				{
					Xbyak::Label funcLabel;
					Xbyak::Label retnLabel;

					push(rdx);
					push(rcx);

					mov(r9d, 0);
#ifdef SKYRIM_AE
					mov(r8d, edi);   // movedY
					mov(edx, r14d);  // movedX
#else
					mov(r8d, r14d);  // movedY
					mov(edx, ebp);   // movedX
#endif
					mov(rcx, rbx);

					sub(rsp, 0x20);
					call(ptr[rip + funcLabel]);  // call our function
					add(rsp, 0x20);

					pop(rdx);
					pop(rcx);
					jmp(ptr[rip + retnLabel]);

					L(funcLabel);
					dq(a_func);

					L(retnLabel);
#ifdef SKYRIM_AE
					dq(a_target + 0x15C);
#else
					dq(a_target + (0xB5F - 0xA0C));
#endif
				}
			};
			Patch patch(reinterpret_cast<uintptr_t>(UpdateGrassGridNow), addr);
			patch.ready();

			auto& trampoline = SKSE::GetTrampoline();
			trampoline.write_branch<6>(addr, trampoline.allocate(patch));
		} else {
			stl::report_and_fail("Failed to Unload Old Grass");
		}

		if (addr = RELOCATION_ID(13190, 13335).address(); REL::make_pattern<"48 89 74 24 10">().match(addr)) {
			//Memory::WriteHook(new HookParameters() { Address = addr, IncludeLength = 0, ReplaceLength = 5, Before = [&] (std::any ctx)

			struct Patch : Xbyak::CodeGenerator
			{
				Patch(std::uintptr_t a_func, uintptr_t a_target)
				{
					Xbyak::Label funcLabel;
					Xbyak::Label retnLabel;

					push(rcx);
					mov(r9d, 1);
					mov(r8d, 0);
					mov(edx, 0);

					sub(rsp, 0x20);
					call(ptr[rip + funcLabel]);  // call our function
					add(rsp, 0x20);

					pop(rcx);
					jmp(ptr[rip + retnLabel]);

					L(funcLabel);
					dq(a_func);

					L(retnLabel);
					dq(a_target + 0x5);
				}
			};
			Patch patch(reinterpret_cast<uintptr_t>(UpdateGrassGridNow), addr);
			patch.ready();

			auto& trampoline = SKSE::GetTrampoline();
			trampoline.write_branch<5>(addr, trampoline.allocate(patch));
		} else {
			stl::report_and_fail("Failed to load New Grass");
		}

		Memory::Internal::write<uint8_t>(addr + 5, 0xC3, true);

		if (addr = RELOCATION_ID(13191, 13336).address(); REL::make_pattern<"48 89 74 24 10">().match(addr)) {
			//Memory::WriteHook(new HookParameters() { Address = addr, IncludeLength = 0, ReplaceLength = 5, Before = [&] (std::any ctx)
			struct Patch : Xbyak::CodeGenerator
			{
				Patch(std::uintptr_t a_func, uintptr_t a_target)
				{
					Xbyak::Label funcLabel;
					Xbyak::Label retnLabel;

					push(rcx);
					mov(r9d, -1);
					mov(r8d, 0);
					mov(edx, 0);

					sub(rsp, 0x20);
					call(ptr[rip + funcLabel]);  // call our function
					add(rsp, 0x20);

					pop(rcx);
					jmp(ptr[rip + retnLabel]);

					L(funcLabel);
					dq(a_func);

					L(retnLabel);
					dq(a_target + 0x5);
				}
			};
			Patch patch(reinterpret_cast<uintptr_t>(UpdateGrassGridNow), addr);
			patch.ready();

			auto& trampoline = SKSE::GetTrampoline();
			trampoline.write_branch<5>(addr, trampoline.allocate(patch));
		} else {
			stl::report_and_fail("Failed to load New Grass");
		}

		Memory::Internal::write<uint8_t>(addr + 5, 0xC3, true);

		// cell dtor
		if (!load_only) {
			if (addr = (RELOCATION_ID(18446, 18877).address() + (0xC4 - 0x50)); REL::make_pattern<"48 85 C0">().match(addr)) {
				//Memory::WriteHook(new HookParameters() { Address = addr, IncludeLength = 0, ReplaceLength = (0xE9 - 0xC9), Before = [&] (std::any ctx)
				struct Patch : Xbyak::CodeGenerator
				{
					Patch(uintptr_t a_func, uintptr_t a_target)
					{
						Xbyak::Label funcLabel;
						Xbyak::Label retnLabel;
						Xbyak::Label NotIf;
						Xbyak::Label addr_ClearGrassHandles;

						mov(rcx, rdi);

						sub(rsp, 0x20);
						call(ptr[rip + funcLabel]);  // call our function
						add(rsp, 0x20);

						test(rax, rax);
						je(NotIf);
						lea(rcx, ptr[rdi + 0x48]);

						sub(rsp, 0x20);
						call(ptr[rip + addr_ClearGrassHandles]);  // call our function
						add(rsp, 0x20);
						jmp(NotIf);

						L(NotIf);
						jmp(ptr[rip + retnLabel]);

						L(funcLabel);
						dq(a_func);

						L(addr_ClearGrassHandles);
						dq(RELOCATION_ID(11931, 12070).address());

						L(retnLabel);
						dq(a_target + 0x5 + (0xE9 - 0xC9));
					}
				};
				Patch patch(reinterpret_cast<uintptr_t>(Handle_RemoveGrassFromCell_Call), addr);
				patch.ready();

				auto& trampoline = SKSE::GetTrampoline();
				DWORD flOldProtect = 0;
				BOOL change_protection = VirtualProtect((LPVOID)addr, 0x13, PAGE_EXECUTE_READWRITE, &flOldProtect);
				if (change_protection) {
					memset((void*)(addr + 5), 0x90, (0xE9 - 0xC9));
				}
				trampoline.write_branch<5>(addr, trampoline.allocate(patch));
			} else {
				stl::report_and_fail("Failed to load New Grass");
			}
		}

		// unloading cell
		if (!load_only) {
			if (addr = RELOCATION_ID(13623, 13721).address() + OFFSET(0xC0F8 - 0xBF90, 0x1E8); REL::make_pattern<"E8">().match(addr)) {
				//Memory::WriteHook(new HookParameters() { Address = addr, IncludeLength = 0, ReplaceLength = 5, Before = [&] (std::any ctx)
				Utility::Memory::SafeWrite(addr, Utility::Assembly::NoOperation5);
				// This is not necessary because we want to keep grass on cell unload.
				//Handle_RemoveGrassFromCell_Call(ctx.CX, ctx.DX, "unload");
			} else {
				stl::report_and_fail("Failed to unload cell");
			}
		}

		// remove just before read grass so there's not so noticeable fade-in / fade-out, there is still somewhat noticeable effect though
		if (!load_only) {
			if (addr = RELOCATION_ID(15204, 15372).address() + OFFSET(0x8F - 0x10, 0x7D); REL::make_pattern<"E8">().match(addr)) {
				//Memory::WriteHook(new HookParameters() { Address = addr, IncludeLength = 5, ReplaceLength = 5, After = [&] (std::any ctx)
				struct Patch : Xbyak::CodeGenerator
				{
					Patch(std::uintptr_t a_func, uintptr_t a_target)
					{
						Xbyak::Label funcLabel;
						Xbyak::Label funcLabel2;
						Xbyak::Label retnLabel;

						Xbyak::Label NotIf;

						sub(rsp, 0x20);
						call(ptr[rip + funcLabel2]);  // call our function
						add(rsp, 0x20);

						mov(rcx, rsi);
						mov(rdx, r13);

						sub(rsp, 0x20);
						call(ptr[rip + funcLabel]);  // call our function
						add(rsp, 0x20);

						jmp(ptr[rip + retnLabel]);

						L(funcLabel);
						dq(a_func);

						L(funcLabel2);
						dq(RELOCATION_ID(12210, 13129).address());

						L(retnLabel);
						dq(a_target + 0x5);
					}
				};
				Patch patch(reinterpret_cast<uintptr_t>(RemoveGrassHook), addr);
				patch.ready();

				auto& trampoline = SKSE::GetTrampoline();
				trampoline.write_branch<5>(addr, trampoline.allocate(patch));
			} else {
				stl::report_and_fail("Failed to Remove Grass");
			}
		}

		// Fix weird shape selection.
		// Vanilla game groups shape selection by 12 x 12 cells, we want a shape per cell.
#ifndef SKYRIM_AE
		if (addr = RELOCATION_ID(15204, 15372).address() + (0x5005 - 0x4D1C); REL::make_pattern<"E8">().match(RELOCATION_ID(15204, 15372).address() + (0x5005 - 0x4D10))) {
			//Memory::WriteHook(new HookParameters() { Address = addr, IncludeLength = 5, ReplaceLength = 5, Before = [&] (std::any ctx)
			struct Patch : Xbyak::CodeGenerator
			{
				Patch(uintptr_t a_target)
				{
					Xbyak::Label retnLabel;

					mov(r8d, ptr[rsp + 0x40]);
					mov(r9d, ptr[rsp + 0x3C]);

					jmp(ptr[rip + retnLabel]);

					L(retnLabel);
					dq(a_target + 0x9);
				}
			};
			Patch patch(addr);
			patch.ready();

			auto& trampoline = SKSE::GetTrampoline();
			Utility::Memory::SafeWrite(addr + 5, Utility::Assembly::NoOperation4);
			trampoline.write_branch<5>(addr, trampoline.allocate(patch));
		} else {
			stl::report_and_fail("Failed to fix shape selection");
		}
#endif
		addr = RELOCATION_ID(15205, 15373).address() + OFFSET(0x617, 0x583);
		struct Patch : Xbyak::CodeGenerator
		{
			Patch(uintptr_t a_target)
			{
				Xbyak::Label retnLabel;

				mov(r8d, ptr[rsp + 0x38]);
				mov(r9d, ptr[rsp + 0x3C]);

				jmp(ptr[rip + retnLabel]);

				L(retnLabel);
				dq(a_target + 0x8);
			}
		};
		Patch patch(addr);
		patch.ready();

		auto& trampoline = SKSE::GetTrampoline();
		Utility::Memory::SafeWrite(addr + 5, Utility::Assembly::NoOperation3);
		trampoline.write_branch<5>(addr, trampoline.allocate(patch));

// Some reason in AE doesnt't generate a functional hook, using a thunk instead should work the same
#ifndef SKYRIM_AE
		if (addr = RELOCATION_ID(15206, 15374).address() + (0x645C - 0x620D); REL::make_pattern<"E8">().match(RELOCATION_ID(15206, 15374).address() + (0x645C - 0x6200))) {
			//Memory::WriteHook(new HookParameters() { Address = addr, IncludeLength = 5, ReplaceLength = 5, Before = [&] (std::any ctx)
			struct Patch : Xbyak::CodeGenerator
			{
				Patch(uintptr_t a_target)
				{
					Xbyak::Label retnLabel;

					mov(r8d, ptr[rsp + 0x34]);
					mov(r9d, ptr[rsp + 0x30]);

					jmp(ptr[rip + retnLabel]);

					L(retnLabel);
					dq(a_target + 0x6);
				}
			};
			Patch patch(addr);
			patch.ready();

			auto& trampoline = SKSE::GetTrampoline();
			trampoline.write_branch<6>(addr, trampoline.allocate(patch));
		} else {
			stl::report_and_fail("Failed to fix shape selection");
		}
#endif

		addr = RELOCATION_ID(15214, 15383).address() + OFFSET(0x78B7 - 0x7830, 0x7E);
		//Memory::WriteHook(new HookParameters() { Address = addr, IncludeLength = 0, ReplaceLength = 0xC2 - 0xB7, Before = [&] (std::any ctx)

		struct Patch2 : Xbyak::CodeGenerator
		{
			explicit Patch2(uintptr_t a_target)
			{
				Xbyak::Label retnLabel;

#ifdef SKYRIM_AE
				mov(ptr[rsp + 0x48 + 4], r14w);
				mov(ptr[rsp + 0x48 + 6], bx);

				movsx(eax, r14w);  // x
				mov(r14, rcx);
#else
				mov(ptr[rsp + 0x48 + 4], r15w);
				mov(ptr[rsp + 0x48 + 6], bx);

				movsx(eax, r15w);  // x
				mov(r15, rcx);
#endif
				cdq();
				mov(ecx, 12);  // x/12
				idiv(ecx);
#ifdef SKYRIM_AE
				mov(rcx, r14);
				mov(r14d, eax);
#else
				mov(rcx, r15);
				mov(r15d, eax);
#endif

				movsx(eax, bx);  // y
				mov(rbx, rcx);
				cdq();
				mov(ecx, 12);  // y/12
				idiv(ecx);
				mov(rcx, rbx);
				mov(ebx, eax);

#ifdef SKYRIM_AE
				mov(ptr[rsp + 0x258 + 0x58], r14d);  // x
#else
				mov(ptr[rsp + 0x258 + 0x58], r15d);  // x
#endif
				mov(ptr[rsp + 0x258 + 0x60], ebx);  // y

				jmp(ptr[rip + retnLabel]);

				L(retnLabel);
				dq(a_target + (0xC2 - 0xB7));
			}
		};
		Patch2 patch2(addr);
		patch2.ready();

		DWORD flOldProtect = 0;
		BOOL change_protection = VirtualProtect((LPVOID)addr, (0xC2 - 0xB7), PAGE_EXECUTE_READWRITE, &flOldProtect);
		// Pass address of the DWORD variable ^
		if (change_protection) {
			memset((void*)(addr), 0x90, (0xC2 - 0xB7));
		}
		trampoline.write_branch<5>(addr, trampoline.allocate(patch2));

		// Exterior cell buffer must be extended if grass radius is outside of ugrids.
		// Reason: cell may get deleted while it still has grass and we can not keep grass there then.
		if (!load_only) {
			if (addr = (RELOCATION_ID(13233, 13384).address() + (0xB2 - 0x60)); REL::make_pattern<"8B 05">().match(addr)) {
				//Memory::WriteHook(new HookParameters() { Address = addr, IncludeLength = 0, ReplaceLength = 6, Before = [&] (std::any ctx)
				int ugrids = Memory::Internal::read<int>(addr_uGrids + 8);
				int ggrids = getChosenGrassGridRadius() * 2 + 1;
				int Max = std::max(ugrids, ggrids);
				struct Patch : Xbyak::CodeGenerator
				{
					explicit Patch(uintptr_t a_target, int* max)
					{
						Xbyak::Label retnLabel;

						mov(rax, (uintptr_t)max);

						jmp(ptr[rip + retnLabel]);

						L(retnLabel);
						dq(a_target + 0x6);
					}
				};
				Patch patch(addr, &Max);
				patch.ready();
				auto& trampoline = SKSE::GetTrampoline();
				trampoline.write_branch<6>(addr, trampoline.allocate(patch));
			} else {
				stl::report_and_fail("Failed to extend cell buffer");
			}
		}

		// Allow grass distance to extend beyond uGrids * 4096 units (20480).
		if (addr = RELOCATION_ID(15202, 15370).address() + OFFSET(0xB06 - 0x890, 0x279); REL::make_pattern<"C1 E0 0C">().match(addr)) {
			Memory::Internal::write<uint8_t>(addr + 2, 16, true);
		} else {
			stl::report_and_fail("Failed to extend grass");
		}

		if (addr = RELOCATION_ID(528751, 15379).address() + OFFSET(0xFE - 0xE0, 0x1E); REL::make_pattern<"C1 E0 0C">().match(addr)) {
			Memory::Internal::write<uint8_t>(addr + 2, 16, true);
		} else {
			stl::report_and_fail("Failed to extend grass");
		}

		// Allow create grass without a land mesh. This is still necessary!
		if (!load_only) {
			if (addr = (RELOCATION_ID(15204, 15372).address() + (0xED2 - 0xD10)); REL::make_pattern<"E8">().match(addr)) {
				//Memory::WriteHook(new HookParameters() { Address = addr, IncludeLength = 0, ReplaceLength = 5, Before = [&] (std::any ctx)
				struct Patch : Xbyak::CodeGenerator
				{
					explicit Patch(uintptr_t a_target)
					{
						Xbyak::Label retnLabel;

						mov(rax, 1);
						jmp(ptr[rip + retnLabel]);

						L(retnLabel);
						dq(a_target + 0x5);
					}
				};
				Patch patch(addr);
				patch.ready();

				auto& trampoline = SKSE::GetTrampoline();
				trampoline.write_branch<5>(addr, trampoline.allocate(patch));
			} else {
				stl::report_and_fail("Failed to Create Grass");
			}
		}

		// Cell unload should clear queued task. Otherwise it will crash or not allow creating grass again later.
		if (!load_only) {
			if (addr = (RELOCATION_ID(18655, 19130).address() + OFFSET(0xC888 - 0xC7C0, 0xCA)); REL::make_pattern<"E8">().match(addr)) {
				//Memory::WriteHook(new HookParameters() { Address = addr, IncludeLength = 0, ReplaceLength = 5, Before = [&] (std::any ctx)
				struct Patch : Xbyak::CodeGenerator
				{
					Patch(std::uintptr_t a_func, uintptr_t b_func, uintptr_t a_target)
					{
						Xbyak::Label funcLabel;
						Xbyak::Label funcLabel2;
						Xbyak::Label retnLabel;

						mov(r15, rax);
						mov(rcx, rdi);

						sub(rsp, 0x20);
						call(ptr[rip + funcLabel2]);  // call our function
						add(rsp, 0x20);

						movzx(ecx, al);
						mov(rdx, rdi);

						sub(rsp, 0x20);
						call(ptr[rip + funcLabel]);  // call our function
						add(rsp, 0x20);

						mov(rax, r15);

						jmp(ptr[rip + retnLabel]);

						L(funcLabel);
						dq(a_func);

						L(funcLabel2);
						dq(b_func);

						L(retnLabel);
						dq(a_target + 0x5);
					}
				};
				Patch patch(reinterpret_cast<uintptr_t>(CellUnloadHook), reinterpret_cast<uintptr_t>(ClearCellAddGrassTask), addr);
				patch.ready();

				auto& trampoline = SKSE::GetTrampoline();
				trampoline.write_branch<5>(addr, trampoline.allocate(patch));
			} else {
				stl::report_and_fail("Failed to unload cell");
			}
		}

		// Create custom way to load cell.
		if (!load_only) {
			addr = RELOCATION_ID(18137, 18527).address() + OFFSET(0x17, 0x25);
			struct Patch : Xbyak::CodeGenerator
			{
				Patch(std::uintptr_t a_func, std::uintptr_t b_func, uintptr_t a_target)
				{
					Xbyak::Label funcLabel;
					Xbyak::Label funcLabel2;
					Xbyak::Label retnLabel;
					Xbyak::Label skip;
					Xbyak::Label Call;

					Xbyak::Label j_else;
					Xbyak::Label notIf;
					Xbyak::Label include;

					mov(al, ptr[rbx + 0x3E]);  //if ourMethod == 1
					cmp(al, 1);
					jne(j_else);

					mov(rcx, ptr[rbx + 0x20]);  // ws
					mov(edx, ptr[rbx + 0x30]);  // x
					mov(r8d, ptr[rbx + 0x34]);  // y

					sub(rsp, 0x20);
					call(ptr[rip + funcLabel]);  // call our function
					add(rsp, 0x20);

					jmp(ptr[rip + skip]);

					L(j_else);
					cmp(al, 0);
					je(notIf);

					mov(cl, al);

					sub(rsp, 0x20);
					call(ptr[rip + funcLabel2]);  // call our function
					add(rsp, 0x20);

					jmp(include);

					L(notIf);
					jmp(include);

					L(include);
#ifdef SKYRIM_AE
					call(ptr[rip + Call]);
#else
					mov(rcx, ptr[rbx + 0x18]);
					lea(rdx, ptr[rbx + 0x20]);
#endif
					jmp(ptr[rip + retnLabel]);

					L(funcLabel);
					dq(a_func);

					L(funcLabel2);
					dq(b_func);

					L(retnLabel);
#ifdef SKYRIM_AE
					dq(a_target + 0x5);
#else
					dq(a_target + 0x8);
#endif

					L(Call);
					dq(RELOCATION_ID(18637, 19111).address());

					L(skip);
#ifdef SKYRIM_AE
					dq(a_target + 0x141);
#else
					dq(a_target + 0x8 + (0xA9 - 0x8F));
#endif
				}
			};
			Patch patch(reinterpret_cast<uintptr_t>(CellLoadNow_Our), reinterpret_cast<uintptr_t>(ThrowOurMethodException), addr);
			patch.ready();

			auto& trampoline = SKSE::GetTrampoline();
#ifdef SKYRIM_AE
			trampoline.write_branch<5>(addr, trampoline.allocate(patch));
#else
			Utility::Memory::SafeWrite(addr + 5, Utility::Assembly::NoOperation3);
			trampoline.write_branch<5>(addr, trampoline.allocate(patch));
#endif

			addr = RELOCATION_ID(18150, 18541).address() + OFFSET_3(0xB094 - 0xAF20, 0x1CA, 0x177);
			struct Patch2 : Xbyak::CodeGenerator
			{
				Patch2(uintptr_t a_func, uintptr_t a_target)
				{
					Xbyak::Label funcLabel;
					Xbyak::Label retnLabel;

#ifndef SKYRIMVR
#	ifdef SKYRIM_AE
					movzx(eax, ptr[rsp + 0xA0]);
#	else
					mov(esi, ptr[rsp + 0x30]);
#	endif
					mov(ptr[rbx + 0x3D], al);

					mov(rcx, ptr[rbx + 0x30]);  // x
					mov(rdx, ptr[rbx + 0x34]);  // y

					sub(rsp, 0x20);
					call(ptr[rip + funcLabel]);
					add(rsp, 0x20);

					mov(byte[rbx + 0x3E], al);

					jmp(ptr[rip + retnLabel]);

					L(funcLabel);
					dq(a_func);

					L(retnLabel);
#	ifdef SKYRIM_AE
					dq(a_target + 0xB);
#	else
					dq(a_target + 0x7);
#	endif
#else  // VR
					mov(esi, ptr[rsp + 0x30]);
					mov(ptr[rbx + 0x45], al);

					mov(rcx, ptr[rbx + 0x38]);  // x
					mov(rdx, ptr[rbx + 0x3c]);  // y

					sub(rsp, 0x20);
					call(ptr[rip + funcLabel]);
					add(rsp, 0x20);

					mov(byte[rbx + 0x46], al);

					jmp(ptr[rip + retnLabel]);

					L(funcLabel);
					dq(a_func);

					L(retnLabel);
					dq(a_target + 0x7);
#endif
				}
			};
			Patch2 patch2(reinterpret_cast<uintptr_t>(CellLoadHook), addr);
			patch2.ready();
#ifdef SKYRIM_AE
			Utility::Memory::SafeWrite(addr + 5, Utility::Assembly::NoOperation6);
#else
			Utility::Memory::SafeWrite(addr + 5, Utility::Assembly::NoOperation2);
#endif
			trampoline.write_branch<5>(addr, trampoline.allocate(patch2));

			addr = RELOCATION_ID(18149, 18540).address() + OFFSET_3(0xE1B - 0xCC0, 0x167, 0x15E);
			struct Patch3 : Xbyak::CodeGenerator
			{
				explicit Patch3(uintptr_t a_target)
				{
					Xbyak::Label retnLabel;
#ifndef SKYRIMVR
					mov(byte[rbx + 0x3E], 0);

					mov(byte[rbx + 0x3C], 1);

#	ifdef SKYRIM_AE
					mov(ptr[rbx + 0x3D], bpl);
#	else
					mov(ptr[rbx + 0x3D], r15b);
#	endif
#else
					mov(byte[rbx + 0x46], 0);

					mov(byte[rbx + 0x44], 1);

					mov(ptr[rbx + 0x45], r15b);
#endif  // VR
					jmp(ptr[rip + retnLabel]);

					L(retnLabel);
					dq(a_target + 0x8);
				}
			};
			Patch3 patch3(addr);
			patch3.ready();

			Utility::Memory::SafeWrite(addr + 6, Utility::Assembly::NoOperation2);
			trampoline.write_branch<5>(addr, trampoline.allocate(patch3));

			addr = RELOCATION_ID(13148, 13288).address() + OFFSET(0x2630 - 0x2220, 0x4D0);
			struct Patch4 : Xbyak::CodeGenerator
			{
				Patch4(std::uintptr_t a_func, uintptr_t a_target)
				{
					Xbyak::Label funcLabel;
					Xbyak::Label retnLabel;
					Xbyak::Label notIf;
					Xbyak::Label jump;
					Xbyak::Label jumpAd;

					push(r9);
					push(rdx);
					push(rax);
					push(rsi);
#ifdef SKYRIM_AE
					mov(r8d, r12w);  //movedX
					mov(r9d, r15w);  //movedY
#else
					mov(r8d, ptr[rsp + 0x90]);  //movedX
					mov(r9d, r13w);             //movedY
#endif
					mov(esi, r8d);

					mov(rax, ptr[rbx + 0x140]);  // ws
					test(rax, rax);
					jne(notIf);

					pop(r9);
					pop(rdx);
					pop(rax);
					pop(rsi);
					cmp(esi, 0);
					jmp(ptr[rip + retnLabel]);

					L(notIf);
					mov(ecx, ptr[rbx + 0xB8]);  // prevX
					mov(edx, ptr[rbx + 0xBC]);  // prevY

					sub(rsp, 0x20);
					call(ptr[rip + funcLabel]);
					add(rsp, 0x20);

					pop(r9);
					pop(rdx);
					pop(rax);
					pop(rsi);
#ifdef SKYRIM_AE
					test(r12w, r12w);
					jz(jump);
#else
					cmp(esi, 0);
#endif
					jmp(ptr[rip + retnLabel]);

					L(funcLabel);
					dq(a_func);

					L(jumpAd);
					dq(a_target + 0x2C);

					L(jump);
					jmp(ptr[rip + jumpAd]);

					L(retnLabel);
#ifdef SKYRIM_AE
					dq(a_target + 0x6);
#else
					dq(a_target + 0x9);
#endif
				}
			};
			Patch4 patch4(reinterpret_cast<uintptr_t>(UpdateGrassGridQueue), addr);
			patch4.ready();
#ifdef SKYRIM_AE
			trampoline.write_branch<6>(addr, trampoline.allocate(patch4));
#else
			Utility::Memory::SafeWrite(addr + 5, Utility::Assembly::NoOperation4);
			trampoline.write_branch<5>(addr, trampoline.allocate(patch4));
#endif

			addr = RELOCATION_ID(13148, 13288).address() + OFFSET(0x29AF - 0x2220, 0x9A9);
			struct Patch5 : Xbyak::CodeGenerator
			{
				Patch5(uintptr_t a_func, uintptr_t a_target)
				{
					Xbyak::Label TES_Sub;
					Xbyak::Label funcLabel;
					Xbyak::Label retnLabel;

					call(ptr[rip + TES_Sub]);

					push(rcx);
					push(rdx);
					mov(edx, ptr[rbx + 0xB0]);   // nowX
					mov(r8d, ptr[rbx + 0xB4]);   // nowY
					mov(rcx, ptr[rbx + 0x140]);  // ws

					sub(rsp, 0x20);
					call(ptr[rip + funcLabel]);
					add(rsp, 0x20);

					pop(rcx);
					pop(rdx);

					jmp(ptr[rip + retnLabel]);

					L(TES_Sub);
					dq(RELOCATION_ID(13213, 13362).address());

					L(funcLabel);
					dq(a_func);

					L(retnLabel);
					dq(a_target + 0x5);
				}
			};
			Patch5 patch5(reinterpret_cast<uintptr_t>(UpdateGrassGridEnsureLoad), addr);
			patch5.ready();

			trampoline.write_branch<5>(addr, trampoline.allocate(patch5));
		}
	}

	bool DistantGrass::_canUpdateGridNormal = false;
	bool DistantGrass::load_only = false;

	float DistantGrass::getChosenGrassFadeRange()
	{
		float r = Config::OverwriteGrassFadeRange;
		if (r < 0.0f) {
			auto setting = RE::INISettingCollection::GetSingleton()->GetSetting("fGrassFadeRange:Grass");
			r = setting->data.f;
			//auto addr = RELOCATION_ID(501110, 359416).address();
			//r = Memory::Internal::read<float>(addr + 8);
		}

		return r;
	}

	int DistantGrass::getChosenGrassGridRadius()
	{
		int r = _chosenGrassGridRadius;
		if (r < 0) {
			float dist = Config::OverwriteGrassDistance;
			if (dist < 0.0f) {
				auto addr = RELOCATION_ID(501108, 359413).address();
				dist = Memory::Internal::read<float>(addr + 8);
			}

			float range = getChosenGrassFadeRange();

			float total = std::max(0.0f, dist) + std::max(0.0f, range);
			float cells = total / 4096.0f;

			int icells = static_cast<int>(std::ceil(cells));

			switch (Config::DynDOLODGrassMode) {
			case 1:
				icells = Memory::Internal::read<int>(addr_uGrids + 8) / 2;
				break;

			case 2:
				icells = Memory::Internal::read<int>(addr_uLargeRef + 8) / 2;
				break;
			default:
				break;
			}

			if (icells < 2) {
				r = 2;
			} else if (icells > 32) {
				logger::info("GrassControl: calculated iGrassCellRadius is {}! Using 32 instead. This probably means fGrassFadeRange or fGrassFadeStartDistance is very strange value.", icells);
				r = 32;
			} else {
				r = icells;
			}

			int imin = 0;
			if (Config::OverwriteMinGrassSize >= 0) {
				imin = Config::OverwriteMinGrassSize;
			} else {
				try {
					imin = Memory::Internal::read<int>(RELOCATION_ID(501113, 359421).address() + 8);
				} catch (...) {
				}
			}

			logger::debug("ChosenGrassGridRadius(null) chose: {}, dist: {}, range: {}, iMinGrassSize: {}", r, dist, range, imin);

			logger::info("ChosenGrassSettings: grid: {}, dist: {}, range: {}, iMinGrassSize: {}", r, dist, range, imin);

			_chosenGrassGridRadius = r;
		}

		return r;
	}

	int DistantGrass::_chosenGrassGridRadius = -1;
	bool DistantGrass::DidApply = false;

	void DistantGrass::CellLoadNow_Our(uintptr_t ws, int x, int y)
	{
		auto c = Map->GetFromGrid(x, y);
		if (!c || InterlockedCompareExchange64(&c->furtherLoad, 0, 2) != 2) {
			return;
		}

		if (ws == 0)
			return;

		REL::Relocation<RE::TESObjectCELL* (*)(uintptr_t, int, int)> func{ RELOCATION_ID(20026, 20460) };

		auto cell = func(ws, x, y);
		if (IsValidLoadedCell(cell, false)) {
			if (*cell->GetName()) {
				logger::debug(fmt::runtime("FurtherLoadSuccessFirst({}, {}) Name: " + std::string(cell->GetName()) + ", FormId: {:x}"), x, y, cell->formID);
			} else {
				logger::debug(fmt::runtime("FurtherLoadSuccessFirst({}, {}) Name: Wilderness, FormId: {:x}"), x, y, cell->formID);
			}
		} else if (cell != nullptr) {
			auto landPtr = cell->cellLand;
			if (landPtr != nullptr && (Memory::Internal::read<uint8_t>(&(landPtr->data.flags)) & 8) == 0) {
				REL::Relocation<void (*)(RE::TESObjectLAND*, int, int)> fnc{ RELOCATION_ID(18331, 18747) };

				fnc(landPtr, 0, 1);

				if (IsValidLoadedCell(cell, false)) {
					logger::debug(fmt::runtime("FurtherLoadSuccessSecond({}, {}) {}"), c->x, c->y, cell->formID);
				}
			}
		}
	}

	bool DistantGrass::IsLodCell(RE::TESObjectCELL* cell)
	{
		if (!DidApply || load_only)
			return false;

		if (cell == nullptr)
			return false;

		auto c = Map->FindByCell(cell);
		if (!c)
			return false;

		int d = c->self_data;
		if ((d >> 8 & 0xFF) == static_cast<int>(GrassStates::Lod))
			return true;
		return false;
	}

	bool DistantGrass::ClearCellAddGrassTask(const RE::TESObjectCELL* cell)
	{
		if (cell == nullptr)
			return false;

		uintptr_t task;
		{
			auto alloc = new char[0x10];  // RE::BSExtraData
			Memory::Internal::write<uintptr_t>(reinterpret_cast<uintptr_t>(alloc), 0);

			REL::Relocation<void (*)(const RE::ExtraDataList&, uintptr_t)> func{ RELOCATION_ID(11933, 12072) };
			func(cell->extraList, reinterpret_cast<uintptr_t>(alloc));

			task = Memory::Internal::read<uintptr_t>(alloc);
			delete[] alloc;
		}

		if (task == 0)
			return false;

		Memory::Internal::write<uint8_t>(task + 0x48, 1);
		volatile uint64_t* TaskAdr = &task + 8;
		if (InterlockedDecrement(TaskAdr) == 0) {
			auto vtable = Memory::Internal::read<intptr_t>(task);
			uintptr_t dtor = Memory::Internal::read<intptr_t>(vtable);
			REL::Relocation<void (*)(intptr_t, int)> func{ dtor };

			func(task, 1);
		}

		REL::Relocation<void (*)(const RE::ExtraDataList&, int)> func{ RELOCATION_ID(11932, 12071) };

		func(cell->extraList, 0);

		return true;
	}

	bool DistantGrass::IsValidLoadedCell(RE::TESObjectCELL* cell, bool quickLoad)
	{
		if (cell == nullptr)
			return false;

		if (quickLoad)
			return true;

		auto land = cell->cellLand;
		if (land == nullptr)
			return false;

		auto data = land->loadedData;
		if (data == nullptr)
			return false;

		return true;
	}

	RE::TESObjectCELL* DistantGrass::GetCurrentWorldspaceCell(const RE::TES* tes, RE::TESWorldSpace* ws, int x, int y, bool quickLoad, bool allowLoadNow)
	{
		auto data = Memory::Internal::read<uintptr_t>(addr_DataHandler);
		if (data == 0 || ws == nullptr)
			return nullptr;

		using func_t = RE::TESObjectCELL* (*)(const RE::TES*, int, int);
		REL::Relocation<func_t> func{ RELOCATION_ID(13174, 13319) };

		RE::TESObjectCELL* cell = func(tes, x, y);
		if (IsValidLoadedCell(cell, quickLoad)) {
			if (*cell->GetName()) {
				logger::debug(fmt::runtime("add_GetCell({}, {}) (0): Name: " + std::string(cell->GetName()) + ", FormId: {:X}"), x, y, cell->formID);
			} else {
				logger::debug(fmt::runtime("add_GetCell({}, {}) (0): Name: Wilderness, FormId: {:X}"), x, y, cell->formID);
			}

			return cell;
		}

		if (allowLoadNow) {
			auto c = Map->GetFromGrid(x, y);
			if (c) {
				InterlockedCompareExchange64(&c->furtherLoad, 0, 2);
			}
			{
				REL::Relocation<void (*)()> Func{ RELOCATION_ID(13188, 13333) };
				Func();

				try {
					REL::Relocation<RE::TESObjectCELL* (*)(RE::TESWorldSpace*, int16_t, int16_t)> Func{ RELOCATION_ID(20026, 20460) };
					cell = Func(ws, x, y);
					if (IsValidLoadedCell(cell, quickLoad)) {
						if (*cell->GetName()) {
							logger::debug(fmt::runtime("add_GetCell({}, {}) (0): Name: " + std::string(cell->GetName()) + ", FormId: {:X}"), x, y, cell->formID);
						} else {
							logger::debug(fmt::runtime("add_GetCell({}, {}) (0): Name: Wilderness, FormId: {:X}"), x, y, cell->formID);
						}
						return cell;
					}
				} catch (...) {
					REL::Relocation<void (*)()> Fnc{ RELOCATION_ID(13189, 13334) };
					Fnc();

					logger::debug(fmt::runtime("Error Loading Cell({}, {})"), x, y);
				}
				REL::Relocation<void (*)()> Fnc{ RELOCATION_ID(13189, 13334) };
				Fnc();
			}
			if (cell != nullptr) {
				REL::Relocation<RE::TESObjectLAND* (*)(RE::TESObjectCELL*)> func{ RELOCATION_ID(18513, 18970) };

				auto landPtr = func(cell);
				if (landPtr != 0 && (Memory::Internal::read<uint8_t>(&(landPtr->data.flags)) & 8) == 0) {
					REL::Relocation<void (*)(RE::TESObjectLAND*, int, int)> Func{ RELOCATION_ID(18331, 18747) };

					Func(landPtr, 0, 1);

					if (IsValidLoadedCell(cell, false)) {
						if (*cell->GetName()) {
							logger::debug(fmt::runtime("add_GetCell({}, {}) (0): Name: " + std::string(cell->GetName()) + ", FormId: {:X}"), x, y, cell->formID);
						} else {
							logger::debug(fmt::runtime("add_GetCell({}, {}) (0): Name: Wilderness, FormId: {:X}"), x, y, cell->formID);
						}

						return cell;
					}
				}
			}
		}

		std::string failData = "null";
		if (cell != nullptr) {
			auto cellStates = cell->cellState.get();
			failData = "loadState: " + std::to_string(static_cast<int>(cellStates)) + " hasTemp: " + std::to_string((Memory::Internal::read<uint8_t>(cell + 0x40) & 0x10) != 0 ? 1 : 0);
		}
		logger::debug(fmt::runtime("add_GetCell({}, {}) FAIL " + failData), x, y);

		return nullptr;
	}

	void DistantGrass::Call_AddGrassNow(RE::BGSGrassManager* GrassMgr, RE::TESObjectCELL* cell, uintptr_t customArg)
	{
		if (Memory::Internal::read<uint8_t>(customArg) != 0)
			return;

		{
			std::scoped_lock lock(locker());
			auto c = Map->FindByCell(cell);
			if (!c) {
				logger::debug("AddedGrass(null): warning: c == null");
				return;
			}

			int d = c->self_data;
			int tg = d >> 8 & 0xFF;

			bool quickLoad = (d >> 16 & 0xFF) != 0;
			if (IsValidLoadedCell(cell, quickLoad)) {
				if (*cell->GetName()) {
					logger::debug(fmt::runtime("AddingGrass({}, {}) ca: {}, cell: Name: " + std::string(cell->GetName()) + ", FormId: {:X}"), c->x, c->y, Memory::Internal::read<uint8_t>(customArg), cell->formID);
				} else {
					logger::debug(fmt::runtime("AddingGrass({}, {}) ca: {}, cell: Name: Wilderness, FormId: {:X}"), c->x, c->y, Memory::Internal::read<uint8_t>(customArg), cell->formID);
				}

				using func_t = void (*)(RE::BGSGrassManager*, RE::TESObjectCELL*, uintptr_t);
				REL::Relocation<func_t> func{ RELOCATION_ID(15204, 15372) };

				func(GrassMgr, cell, customArg);

				logger::debug(fmt::runtime("AddedGrass({}, {})"), c->x, c->y);
			} else {
				logger::debug(fmt::runtime("AddedGrassFAIL({}, {}) <NotValidLoadedCell>"), c->x, c->y);
			}

			// Set this anyway otherwise we get stuck.
			c->self_data = tg;
		}
	}
	GrassStates DistantGrass::GetWantState(const cell_info& c, const int curX, const int curY, const int uGrid, const int grassRadius, const bool canLoadFromFile, const std::string& wsName)
	{
		int diffX = std::abs(curX - c.x);
		int diffY = std::abs(curY - c.y);

		if (diffX > grassRadius || diffY > grassRadius) {
			return GrassStates::None;
		}

		if (load_only) {
			return GrassStates::Active;
		}

		int uHalf = uGrid / 2;
		if (diffX > uHalf || diffY > uHalf) {
			// Special case: if we are loading and not generating anyway and already have active file we can take active instead of lod
			if (canLoadFromFile && c.checkHasFile(wsName, false)) {
				return GrassStates::Active;
			}

			return GrassStates::Lod;
		}

		return GrassStates::Active;
	}

	void DistantGrass::Handle_RemoveGrassFromCell_Call(RE::TESObjectCELL* cell)
	{
		auto grassMgr = RE::BGSGrassManager::GetSingleton();
		std::string why = "celldtor";
		if (cell == nullptr) {
			logger::debug(fmt::runtime("RemoveGrassOther(null): <- " + why));
			return;
		}
		{
			std::scoped_lock lock(locker());
			auto c = Map->FindByCell(cell);
			if (c) {
				if (c->self_data >> 24 != 0) {
					logger::debug(fmt::runtime("RemoveGrassOther({}, {}) <- " + why + " warning: busy"), c->x, c->y);
				} else {
					logger::debug(fmt::runtime("RemoveGrassOther({}, {}) <- " + why), c->x, c->y);
				}

				c->self_data = 0;
				c->cell = nullptr;
				Map->map.erase(cell);
				ClearCellAddGrassTask(cell);
			} else {
				if (!cell->IsInteriorCell()) {
					logger::debug(fmt::runtime("cell_dtor({}, {})"), cell->GetCoordinates()->cellX, cell->GetCoordinates()->cellY);
				}
			}
		}
		REL::Relocation<void (*)(RE::BGSGrassManager*, RE::TESObjectCELL*)> func{ RELOCATION_ID(15207, 15375) };

		func(grassMgr, cell);
	}

	unsigned char DistantGrass::CalculateLoadState(const int nowX, const int nowY, const int x, const int y, const int ugrid, const int ggrid)
	{
		int diffX = std::abs(x - nowX);
		int diffY = std::abs(y - nowY);

		if (diffX <= ugrid && diffY <= ugrid)
			return 2;

		if (diffX <= ggrid && diffY <= ggrid)
			return 1;

		return 0;
	}

	void DistantGrass::UpdateGrassGridEnsureLoad(RE::TESWorldSpace* ws, const int nowX, const int nowY)
	{
		if (ws == nullptr)
			return;

		int grassRadius = getChosenGrassGridRadius();
		int uGrids = Memory::Internal::read<int>(addr_uGrids + 8);
		int uHalf = uGrids / 2;
		int bigSide = std::max(grassRadius, uHalf);
		bool canLoadGrass = Memory::Internal::read<uint8_t>(addr_AllowLoadFile + 8) != 0;

		std::string wsName;
		try {
			wsName = ws->editorID.c_str();
		} catch (...) {
		}

		for (int x = nowX - bigSide; x <= nowX + bigSide; x++) {
			for (int y = nowY - bigSide; y <= nowY + bigSide; y++) {
				unsigned char wantState = CalculateLoadState(nowX, nowY, x, y, uHalf, grassRadius);
				if (wantState != 1)
					continue;

				auto c = Map->GetFromGrid(x, y);
				if (!c)
					continue;

				auto xs = static_cast<short>(x);
				auto ys = static_cast<short>(y);

				auto xsu = static_cast<unsigned short>(xs);

				auto ysu = static_cast<unsigned short>(ys);

				unsigned int mask = static_cast<unsigned int>(xsu) << 16;
				mask |= ysu;

				REL::Relocation<RE::TESObjectCELL* (*)(RE::TESWorldSpace*, uintptr_t)> func{ RELOCATION_ID(13549, 13641) };

				auto cell = func(ws, mask);
				if (IsValidLoadedCell(cell, false))
					continue;

				bool quickLoad = false;
				if (canLoadGrass && (c->checkHasFile(wsName, false) || c->checkHasFile(wsName, true))) {
					quickLoad = true;
				}

				if (quickLoad && IsValidLoadedCell(cell, true))
					continue;

				InterlockedCompareExchange64(&c->furtherLoad, 0, 2);

				REL::Relocation<RE::TESObjectCELL* (*)(RE::TESWorldSpace*, int, int)> func2{ RELOCATION_ID(20026, 20460) };

				cell = func2(ws, x, y);
				if (IsValidLoadedCell(cell, false)) {
					logger::debug("InstantLoadSuccessFirst({}, {}) {}", x, y, cell->GetFormEditorID());
				} else if (cell != nullptr) {
					REL::Relocation<RE::TESObjectLAND* (*)(RE::TESObjectCELL*)> Func{ RELOCATION_ID(18513, 18970) };

					auto landPtr = Func(cell);
					if (landPtr != 0 && (Memory::Internal::read<uint8_t>(&(landPtr->data.flags)) & 8) == 0) {
						REL::Relocation<void (*)(RE::TESObjectLAND*, int, int)> fnc{ RELOCATION_ID(18331, 18747) };

						fnc(landPtr, 0, 1);

						if (IsValidLoadedCell(cell, false)) {
							logger::debug("InstantLoadSuccessSecond({}, {}) {}", c->x, c->y, cell->GetFormEditorID());
						}
					}
				}
			}
		}
	}

	void DistantGrass::UpdateGrassGridQueue(const int prevX, const int prevY, const int movedX, const int movedY)
	{
		auto wsObj = RE::TES::GetSingleton()->worldSpace;

		if (wsObj == nullptr)
			return;

		int grassRadius = getChosenGrassGridRadius();
		int uGrids = Memory::Internal::read<int>(addr_uGrids + 8);
		int uHalf = uGrids / 2;
		int bigSide = std::max(grassRadius, uHalf);
		bool canLoadGrass = Memory::Internal::read<uint8_t>(addr_AllowLoadFile + 8) != 0;

		std::string wsName = wsObj->editorID.c_str();

		int nowX = prevX + movedX;
		int nowY = prevY + movedY;

		for (int x = nowX - bigSide; x <= nowX + bigSide; x++) {
			for (int y = nowY - bigSide; y <= nowY + bigSide; y++) {
				unsigned char wantState = CalculateLoadState(nowX, nowY, x, y, uHalf, grassRadius);
				if (wantState != 1)
					continue;

				auto c = Map->GetFromGrid(x, y);
				if (!c)
					continue;

				auto xs = static_cast<short>(x);
				auto ys = static_cast<short>(y);

				auto xsu = static_cast<unsigned short>(xs);

				auto ysu = static_cast<unsigned short>(ys);

				unsigned int mask = static_cast<unsigned int>(xsu) << 16;
				mask |= ysu;

				REL::Relocation<RE::TESObjectCELL* (*)(RE::TESWorldSpace*, intptr_t)> func{ RELOCATION_ID(13549, 13641) };

				auto cell = func(wsObj, mask);
				if (IsValidLoadedCell(cell, false))
					continue;

				bool quickLoad = false;
				if (canLoadGrass && (c->checkHasFile(wsName, false) || c->checkHasFile(wsName, true)))
					quickLoad = true;

				if (quickLoad && IsValidLoadedCell(cell, true))
					continue;

				if (InterlockedCompareExchange64(&c->furtherLoad, 1, 0) == 0) {
					REL::Relocation<void (*)(uintptr_t, RE::TESWorldSpace*, int, int, int)> Func{ RELOCATION_ID(18150, 18541) };

					Func(Memory::Internal::read<uintptr_t>(addr_QueueLoadCellUnkGlobal), wsObj, x, y, 0);
				}
			}
		}
	}

	void DistantGrass::UpdateGrassGridNow(const RE::TES* tes, int movedX, int movedY, int addType)
	{
		if (addType == 1) {
			_canUpdateGridNormal = true;
		} else if (addType == -1) {
			_canUpdateGridNormal = false;
		} else if (addType == 0) {
			if (!_canUpdateGridNormal) {
				return;
			}
		}

		if (load_only) {
			UpdateGrassGridNow_LoadOnly(tes, movedX, movedY, addType);
			return;
		}

		logger::debug("UpdateGrassGridNowBegin({}, {}) type: {}", movedX, movedY, addType);

		int grassRadius = getChosenGrassGridRadius();
		int uGrids = Memory::Internal::read<int>(addr_uGrids + 8);
		int uHalf = uGrids / 2;
		int bigSide = std::max(grassRadius, uHalf);
		auto ws = tes->worldSpace;
		auto grassMgr = RE::BGSGrassManager::GetSingleton();
		bool canLoadGrass = Memory::Internal::read<uint8_t>(addr_AllowLoadFile + 8) != 0;
		std::string wsName;
		if (ws != nullptr) {
			wsName = ws->editorID.c_str();
		}

		int nowX = tes->unk0B0;
		int nowY = tes->unk0B4;

		auto invokeList = std::vector<RE::TESObjectCELL*>();
		if (addType <= 0) {
			{
				std::scoped_lock lock(locker());
				Map->unsafe_ForeachWithState([&](cell_info c) {
					auto want = addType < 0 ? GrassStates::None : GetWantState(c, nowX, nowY, uGrids, grassRadius, false, "");
					if (want == GrassStates::None) {
						auto cell = c.cell;
						c.cell = nullptr;
						c.self_data = 0;
						ClearCellAddGrassTask(cell);

						logger::debug("RemoveGrassGrid({}, {})", c.x, c.y);

						invokeList.push_back(cell);
						return false;
					}

					return true;
				});
			}

			if (!invokeList.empty()) {
				for (auto cell : invokeList) {
					REL::Relocation<void (*)(RE::BGSGrassManager*, RE::TESObjectCELL*)> func{ RELOCATION_ID(15207, 15375) };

					func(grassMgr, cell);
				}

				invokeList.clear();
			}
		}

		if (addType >= 0) {
			{
				std::scoped_lock lock(locker());
				int minX = nowX - bigSide;
				int maxX = nowX + bigSide;
				int minY = nowY - bigSide;
				int maxY = nowY + bigSide;

				logger::debug("X: ({}, {})", minX, maxX);
				logger::debug("NowX: ({}, {})", nowX, 0);
				logger::debug("Bigside: ({}, {})", bigSide, 0);
				logger::debug("X: ({}, {})", minX, maxX);
				logger::debug("NowX: ({}, {})", nowX, 0);
				logger::debug("Y: ({}, {})", minY, maxY);
				logger::debug("NowY: ({}, {})", nowY, 0);
				logger::debug("GrassRadius: ({}, {})", grassRadius, 0);
				logger::debug("uHalf: ({}, {})", uHalf, 0);

				// Add grass after.
				for (int x = minX; x <= maxX; x++) {
					for (int y = minY; y <= maxY; y++) {
						auto c = Map->GetFromGrid(x, y);
						if (!c) {
							continue;
						}

						int d = c->self_data;
						bool busy = d >> 24 != 0;

						auto want = GetWantState(c.value(), nowX, nowY, uGrids, grassRadius, canLoadGrass, wsName);
						if (want > static_cast<GrassStates>(d & 0xFF))  // this check is using > because there's no need to set lod grass if we already have active grass
						{
							bool canQuickLoad = want == GrassStates::Active || (canLoadGrass && c->checkHasFile(wsName, true));
							auto cellPtr = GetCurrentWorldspaceCell(tes, ws, x, y, canQuickLoad, addType > 0);

							if (cellPtr != nullptr) {
								if (c->cell != cellPtr) {
									if (c->cell != nullptr) {
										logger::debug("c.cell({}, {}) warning: already had cell!", c->x, c->y);
									}
									c->cell = cellPtr;
									Map->map.try_emplace(cellPtr, c.value());
								}

								// set busy and target
								c->self_data = 0x01000000 | (canQuickLoad ? 0x00010000 : 0) | (static_cast<int>(want) << 8) | (d & 0xFF);

								logger::debug("QueueGrass({}, {})", x, y);

								if (!busy)
									invokeList.push_back(cellPtr);
							}
						}
					}
				}
			}

			if (!invokeList.empty()) {
				for (auto cell : invokeList) {
					// The way game does it is do it directly if generate grass files is enabled, But we don't want that because it causes a lot of stuttering.
					REL::Relocation<void (*)(RE::TESObjectCELL*)> func{ RELOCATION_ID(13137, 13277) };

					func(cell);
				}
			}
		}

		logger::debug("UpdateGrassGridNowEnd({}, {}) type: {}", movedX, movedY, addType);
	}

	void DistantGrass::UpdateGrassGridNow_LoadOnly(const RE::TES* tes, int movedX, int movedY, int addType)
	{
		logger::debug("UpdateGrassGridNowBegin_LoadOnly({}, {}) type: {}", movedX, movedY, addType);

		int grassRadius = getChosenGrassGridRadius();
		auto ws = tes->worldSpace;
		auto grassMgr = RE::BGSGrassManager::GetSingleton();
		int nowX = tes->unk0B0;
		int nowY = tes->unk0B4;

		if (addType <= 0) {
			LO2Map->UpdatePositionWithRemove(ws, addType, nowX, nowY, grassRadius);
		}

		if (addType >= 0) {
			auto minX = static_cast<short>(nowX - grassRadius);
			auto maxX = static_cast<short>(nowX + grassRadius);
			auto minY = static_cast<short>(nowY - grassRadius);
			auto maxY = static_cast<short>(nowY + grassRadius);

			// Add grass after.
			for (short x = minX; x <= maxX; x++) {
				for (short y = minY; y <= maxY; y++) {
					LO2Map->QueueLoad(ws, x, y);
				}
			}
		}

		logger::debug("UpdateGrassGridNowEnd_LoadOnly({}, {}) type: {}; c_count: {}; shapeCount: {}", movedX, movedY, addType, LO2Map->GetCount(), Memory::Internal::read<int>((uintptr_t)grassMgr + 0x58));
	}
}
