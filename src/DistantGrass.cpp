#include "GrassControl/DistantGrass.h"

namespace GrassControl
{
	uintptr_t DistantGrass::addr_GrassManager = RELOCATION_ID(514292, 400452).address();
	uintptr_t DistantGrass::addr_uGrids = RELOCATION_ID(501244, 359675).address();
	uintptr_t DistantGrass::addr_AllowLoadFile = RELOCATION_ID(501125, 359439).address();
	uintptr_t DistantGrass::addr_DataHandler = RELOCATION_ID(514141, 400269).address();
	uintptr_t DistantGrass::addr_uLargeRef = RELOCATION_ID(501554, 360374).address();
	uintptr_t DistantGrass::addr_QueueLoadCellUnkGlobal = RELOCATION_ID(514741, 400899).address();

	DistantGrass::cell_info::cell_info(const int _x, const int _y) :
		x(_x), y(_y), cell(NULL)
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
		grid(std::vector<std::shared_ptr<cell_info>>(16641)) // 129 * 129
	{
		for (int x = 0; x <= 128; x++) {
			for (int y = 0; y <= 128; y++) {
				grid[x * 129 + y] = std::make_shared<cell_info>(x - 64, y - 64);
			}
		}
	}

	std::shared_ptr<DistantGrass::cell_info> DistantGrass::CellInfoContainer::GetFromGrid(int x, int y) const
	{
		if (x < -64 || y < -64 || x > 64 || y > 64) 
			return nullptr;

		x += 64;
		y += 64;

		return grid[x * 129 + y];
	}

	void DistantGrass::CellInfoContainer::unsafe_ForeachWithState(const std::function<bool(std::shared_ptr<cell_info>)>& action)
	{
		auto all = mapToVector(map);
		for (auto& pair : all) {
			if (!action(get<1>(pair))) {
				auto it = map.find(get<0>(pair));
				map.erase(it);
			}
		}
	}

	std::shared_ptr<DistantGrass::cell_info> DistantGrass::CellInfoContainer::FindByCell(const uintptr_t cell)
	{
		std::shared_ptr<cell_info> c = nullptr;
		{
			auto it = this->map.find(cell);
			if (it != this->map.end()) {
				c = it->second;
			}
		}
		return c;
	}

	int DistantGrass::LoadOnlyCellInfoContainer2::GetCount() const
	{
		int count = 0;
		{
			std::scoped_lock lock(locker());
			for (auto pair : map) {
				if (get<1>(pair)->State != _cell_data::_cell_states::None) {
					count++;
				}
			}
		}
		return count;
	}

	std::string DistantGrass::LoadOnlyCellInfoContainer2::MakeKey(const std::string& ws, int x, int y)
	{
		return fmt::format("{}_{}_{}", x, y, ws);
	}

	void DistantGrass::LoadOnlyCellInfoContainer2::UpdatePositionWithRemove(RE::TESWorldSpace* ws, int addType, int nowX, int nowY, int grassRadius) const
	{
		unsigned int wsId = ws != nullptr ? ws->formID : 0;

		{
			std::scoped_lock lock(locker());
			for (auto pair : this->map) {
				if (get<1>(pair)->State == _cell_data::_cell_states::None) {
					continue;
				}

				bool everythingIsFineHereNothingToDoFurther = addType >= 0 && get<1>(pair)->WsId == wsId && std::abs(get<1>(pair)->X - nowX) <= grassRadius && std::abs(get<1>(pair)->Y - nowY) <= grassRadius;
				if (everythingIsFineHereNothingToDoFurther) {
					continue;
				}

				_DoUnload(get<1>(pair));
			}
		}
	}

	void DistantGrass::LoadOnlyCellInfoContainer2::QueueLoad(RE::TESWorldSpace* ws, int x, int y)
	{
		if (ws == nullptr) {
			return;
		}

		std::string key = MakeKey(ws->editorID.c_str(), x, y);
		std::shared_ptr<_cell_data> d;
		{
			std::scoped_lock lock(locker());
			auto it = this->map.find(key);
			if (it == this->map.end()) {
				d = std::make_shared<_cell_data>();
				this->map[key] = d;

				d->X = x;
				d->Y = y;
				d->WsId = ws != nullptr ? ws->formID : 0;
			} 
		}

		{
			std::scoped_lock lock(locker());
			if (d->State != _cell_data::_cell_states::None) {
				return;
			}

			d->State = _cell_data::_cell_states::Loading;

			if (d->DummyCell_Ptr == nullptr) {
			    auto tempPtr = new char[320];
				memset(tempPtr, 0, 320);
				d->DummyCell_Ptr = reinterpret_cast<RE::TESObjectCELL*>(tempPtr);

				auto exteriorData = new char[8];
				memset(exteriorData,0, 8);
				
				Memory::Internal::write<int>(reinterpret_cast<uintptr_t>(exteriorData), x);
				Memory::Internal::write<int>(reinterpret_cast<uintptr_t>(exteriorData) + 4, y);

				Memory::Internal::write<uintptr_t>(reinterpret_cast<uintptr_t>(d->DummyCell_Ptr + 0x60), reinterpret_cast<uintptr_t>(exteriorData));
				Memory::Internal::write<uintptr_t>(reinterpret_cast<uintptr_t>(d->DummyCell_Ptr + 0x120), reinterpret_cast<uintptr_t>(ws));
			}

			using func_t = void(*)(RE::TESObjectCELL*);
			REL::Relocation<func_t> func{ RELOCATION_ID(13137, 13277) };

			func(d->DummyCell_Ptr);
		}
	}

	void DistantGrass::LoadOnlyCellInfoContainer2::_DoUnload(const std::shared_ptr<_cell_data>& d) const
	{
		if (d == nullptr) {
			return;
		}

		{
			std::scoped_lock lock(locker());
			if (d->State == _cell_data::_cell_states::None || d->State == _cell_data::_cell_states::Abort) {
				return;
			}

			if (d->State == _cell_data::_cell_states::Loading) {
				d->State = _cell_data::_cell_states::Abort;
				return;
			}
			REL::Relocation<void (*)(uintptr_t, RE::TESObjectCELL*)> func{ RELOCATION_ID(15207, 15375) };

			func(Memory::Internal::read<uintptr_t>(RELOCATION_ID(514292, 400452).address()), d->DummyCell_Ptr);
			REL::Relocation<void (*)(RE::TESObjectCELL*, uintptr_t)> func2{ RELOCATION_ID(11932, 12071) };
			func2(d->DummyCell_Ptr, 0);

			auto extData = Memory::Internal::read<RE::BSExtraData*>(d->DummyCell_Ptr + 0x60);

			delete extData;
			delete d->DummyCell_Ptr;
			
			d->DummyCell_Ptr = nullptr;
			d->State = _cell_data::_cell_states::None;
		}
	}

	void DistantGrass::LoadOnlyCellInfoContainer2::Unload(const RE::TESWorldSpace* ws, const int x, const int y) const
	{
		if (ws == nullptr)
			return;

		std::string key = MakeKey(ws->editorID.c_str(), x, y);
		std::shared_ptr<_cell_data> d;
		{
			std::scoped_lock lock(locker());
			auto it = this->map.find(key);
			d = it->second;
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
			std::scoped_lock lock(locker());
			auto it = this->map.find(key);
			d = it->second;
		}

		if (d == nullptr)
			return;

		{
			std::scoped_lock lock(locker());
			if (d->State == _cell_data::_cell_states::Loaded) {
				// This shouldn't happen
			} else if (d->State == _cell_data::_cell_states::Loading) {
				REL::Relocation<void (*)(uintptr_t, RE::TESObjectCELL*)> func{ RELOCATION_ID(15206, 15374) };

				func(Memory::Internal::read<uintptr_t>(RELOCATION_ID(514292, 400452).address()), d->DummyCell_Ptr);
				d->State = _cell_data::_cell_states::Loaded;
			} else if (d->DummyCell_Ptr != nullptr) {
				auto extData = Memory::Internal::read<RE::BSExtraData*>(d->DummyCell_Ptr + 0x60, true);

				delete extData;
				delete d->DummyCell_Ptr;
				d->DummyCell_Ptr = nullptr;
				d->State = _cell_data::_cell_states::None;
			} else {
				d->State = _cell_data::_cell_states::None;  // this shouldn't happen?
			}
		}
	}

	void DistantGrass::GrassLoadHook(const uintptr_t ws, const int x, const int y)
	{
		LO2Map->_DoLoad(reinterpret_cast<RE::TESWorldSpace*>(ws), x, y);
	}

	void DistantGrass::RemoveGrassHook(const uintptr_t cell, uintptr_t arg_1)
	{
		auto c = Map->FindByCell(cell);
		if (c != nullptr && (c->self_data & 0xFF) != 0) {
			SKSE::log::debug("RemoveGrassDueToAdd({}, {})", c->x, c->y);

			REL::Relocation<void (*)(uintptr_t, uintptr_t)> func{ RELOCATION_ID(15207, 15375) };
			func(arg_1, cell);
		}

		if (c == nullptr) {
			SKSE::log::debug("RemoveGrassDueToAdd(null): warning: c == null");
		}
	}

	void DistantGrass::CellUnloadHook(bool did, RE::TESObjectCELL* cellObj)
	{
		SKSE::log::debug("DataHandler::UnloadCell({}, {}) did: {}", cellObj->GetCoordinates()->cellX, cellObj->GetCoordinates()->cellY, did);
	}

	void ThrowOurMethodException(const uint8_t ourMethod)
	{
		throw InvalidOperationException("GrassControl.dll: unexpected ourMethod: " + std::to_string(ourMethod));
	}

	void CallDebugLog()
	{
		SKSE::log::debug("RemoveGrassDueToAdd(null)");
	}

	unsigned char DistantGrass::CellLoadHook(const int x, const int y)
	{
		unsigned char isOurMethod = 0;
		auto c = Map->GetFromGrid(x, y);
		if (c != nullptr && InterlockedCompareExchange64(&c->furtherLoad, 2, 1) == 1)
		{
			isOurMethod = 1;
		}
		Map->GetFromGrid(x, y) = c;
		return isOurMethod;
	}

	void DistantGrass::ReplaceGrassGrid(const bool _loadOnly)
	{
		DidApply = true;
		load_only = _loadOnly;

		if (load_only)
		{
			LO2Map = std::make_unique<LoadOnlyCellInfoContainer2>();
		}
		else
		{
			Map = std::make_unique<CellInfoContainer>();
		}

		// Disable grass fade.
		if (!load_only)
		{

			auto addr = RELOCATION_ID(38141, 39098).address();
			Memory::Internal::write<uint8_t>(addr, 0xC3, true);

			// There's a chance this below isn't actually used so if it fails no problem.
			try
			{
				if (addr = (RELOCATION_ID(13240, 13391).address() + (0x55E - 0x480)); REL::make_pattern<"E8">().match(RELOCATION_ID(13240, 13391).address() + (0x55E - 0x480))) {
					Utility::Memory::SafeWrite(addr, Utility::Assembly::NoOperation5);
				}
			}
			catch (...)
			{

			}
		}

		// Auto use correct iGrassCellRadius.
		if (!load_only)
		{
			int radius = getChosenGrassGridRadius();
			auto setting = RE::INISettingCollection::GetSingleton()->GetSetting("iGrassCellRadius:Grass");
			setting->data.i = radius;
			RE::INISettingCollection::GetSingleton()->WriteSetting(setting);
			/*
			if (auto addr = (RELOCATION_ID(15202, 15370).address() + (0xA0E - 0x890)); REL::make_pattern<"8B 05">().match(RELOCATION_ID(15202, 15370).address() + (0xA0E - 0x890))) {
				//Memory::WriteHook( { Address = addr, IncludeLength = 0, ReplaceLength = 6, Before = [&] (std::any ctx)

				struct Patch : Xbyak::CodeGenerator
				{
					explicit Patch(uintptr_t a_target, int Radius)
					{
						Xbyak::Label retnLabel;

						mov(eax, ptr[Radius]);
						jmp(ptr[rip + retnLabel]);

						L(retnLabel);
						dq(a_target + 0x6);

					}
				};
				Patch patch(addr, radius);
				patch.ready();

				auto& trampoline = SKSE::GetTrampoline();
				trampoline.write_branch<6>(addr, trampoline.allocate(patch));
			}
			else {
				stl::report_and_fail("Failed to set correct iGrassCellRadius");
			}
			*/
		}
		
		// Unload old grass, load new grass. But we have replaced how the grid works now.
		uintptr_t addr;
		
		if (addr = (RELOCATION_ID(13148, 13288).address() + (0xA06 - 0x220)); REL::make_pattern<"8B 3D">().match(RELOCATION_ID(13148, 13288).address() + (0xA06 - 0x220))) {
			//Memory::WriteHook(new HookParameters() { Address = addr, IncludeLength = 0, ReplaceLength = 6, Before = [&] (std::any ctx)

			struct Patch : Xbyak::CodeGenerator
			{
				Patch(std::uintptr_t a_func, uintptr_t a_target)
				{
					Xbyak::Label ifNot;
					Xbyak::Label funcLabel;
					Xbyak::Label retnLabel;

					mov(al, _canUpdateGridNormal);
					movzx(eax, al);
					test(eax, eax); 
					jne(ifNot);
					jmp(ptr[rip + retnLabel]);

				    L(ifNot);
					push(rdx);
					push(rcx);
					mov(r9d, 0);
					mov(r8d, r14); // movedY
					mov(edx, rbp); // movedX
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
					dq(a_target + 0x6 + (0xB5F - 0xA0C));
				}
			};
			Patch patch(reinterpret_cast<uintptr_t>(UpdateGrassGridNow), addr);
			patch.ready();

			auto& trampoline = SKSE::GetTrampoline();
			trampoline.write_branch<6>(addr, trampoline.allocate(patch));
		}
		else {
			stl::report_and_fail("Failed to Unload Old Grass");
		}
		
		if (addr = RELOCATION_ID(13190, 13335).address(); REL::make_pattern <"48 89 74 24 10">().match(RELOCATION_ID(13190, 13335).address())) {
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

					_canUpdateGridNormal = true;
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
		}
		else {
			stl::report_and_fail("Failed to load New Grass");
		}
		
		Memory::Internal::write<uint8_t>(addr + 5, 0xC3, true);

		if (addr = RELOCATION_ID(13191, 13336).address(); REL::make_pattern <"48 89 74 24 10">().match(RELOCATION_ID(13191, 13336).address())) {
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

					_canUpdateGridNormal = false;
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
		}
		else {
			stl::report_and_fail("Failed to load New Grass");
		}
		
		Memory::Internal::write<uint8_t>(addr + 5, 0xC3, true);
		
		if (addr = RELOCATION_ID(13138, 13278).address(); REL::make_pattern <"48 8B 51 38">().match(RELOCATION_ID(13138, 13278).address())) {
			//Memory::WriteHook(new HookParameters() { Address = addr, IncludeLength = 0, ReplaceLength = 8, Before = [&] (std::any ctx)
			struct Patch : Xbyak::CodeGenerator
			{
				Patch(std::uintptr_t a_func, uintptr_t b_func, uintptr_t a_target)
				{
					Xbyak::Label funcLabel;
					Xbyak::Label funcLabel2;
					Xbyak::Label retnLabel;
					Xbyak::Label NotIf;
					Xbyak::Label Else;

					mov(al, load_only);
					movzx(eax, al);
					test(eax, eax);
					je(Else);
					push(rcx);
					mov(rcx, ptr[rcx + 0x38]); // cell
					cmp(rcx, 0);
					je(NotIf);

					mov(r8, ptr[rcx+0x60]); // ext
					mov(rdx, ptr[r8]);  // x
					mov(r8, ptr[r8 + 0x4]); // y
					mov(rcx, ptr[rcx + 0x120]);
					sub(rsp, 0x20);
					call(ptr[rip + funcLabel2]);  // call our function
					add(rsp, 0x20);

					/*
					auto ext = Memory::Internal::read<intptr_t>(cell + 0x60);
					int x = Memory::Internal::read<int>(ext);
					int y = Memory::Internal::read<int>(ext + 4);
					auto ws = Memory::Internal::read<intptr_t>(cell + 0x120);
					LO2Map->_DoLoad((RE::TESWorldSpace*)(ws), x, y);
					*/

					pop(rcx);
					jmp(ptr[rip + retnLabel]);

					L(Else);
					push(rcx);
					add(rcx, 0x48);
					mov(rdx, rcx);
					sub(rcx, 0x48);
					mov(rcx, ptr[rcx + 0x38]);
					sub(rsp, 0x20);
					call(ptr[rip + funcLabel]);  // call our function
					add(rsp, 0x20);

					pop(rcx);
					jmp(ptr[rip + retnLabel]);
					//Call_AddGrassNow(Memory.ReadPointer(ctx.CX + 0x38), ctx.CX + 0x48);
					
					L(NotIf);
					pop(rcx);
					jmp(ptr[rip + retnLabel]);

					L(funcLabel);
					dq(a_func);

					L(funcLabel2);
					dq(b_func);

					L(retnLabel);
					dq(a_target + 0x8);
				}
			};
			Patch patch(reinterpret_cast<uintptr_t>(Call_AddGrassNow), reinterpret_cast<uintptr_t>(GrassLoadHook), addr);
			patch.ready();

			auto& trampoline = SKSE::GetTrampoline();
			Utility::Memory::SafeWrite(addr + 5, Utility::Assembly::NoOperation3);
			trampoline.write_branch<5>(addr, trampoline.allocate(patch));
		}
		else {
			stl::report_and_fail("Failed to load New Grass");
		}
		Memory::Internal::write<uint8_t>(addr + 8, 0xC3, true);
	
		// cell dtor
		if (!load_only)
		{
			if (addr = (RELOCATION_ID(18446, 18877).address() + (0xC4 - 0x50)); REL::make_pattern <"48 85 C0">().match(RELOCATION_ID(18446, 18877).address() + (0xC4 - 0x50))) {
				//Memory::WriteHook(new HookParameters() { Address = addr, IncludeLength = 0, ReplaceLength = (0xE9 - 0xC9), Before = [&] (std::any ctx)
				struct Patch : Xbyak::CodeGenerator
				{
					Patch(uintptr_t a_func, uintptr_t a_target)
					{
						Xbyak::Label funcLabel;
						Xbyak::Label retnLabel;
						Xbyak::Label NotIf;
						Xbyak::Label addr_ClearGrassHandles;

						mov(rcx, addr_GrassManager);
						mov(rdx, rdi);

						sub(rsp, 0x20);
						call(ptr[rip + funcLabel]);  // call our function
						add(rsp, 0x20);

						test(rax, rax);
						je(NotIf);
						add(rdi, 0x48);
						mov(rcx, rdi);
						sub(rdi, 0x48);

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
						dq(a_target + (0xE9 - 0xC9));
					}
				};
				Patch patch(reinterpret_cast<uintptr_t>(Handle_RemoveGrassFromCell_Call), addr);
				patch.ready();

				auto& trampoline = SKSE::GetTrampoline();
				DWORD flOldProtect = 0;
				BOOL change_protection = VirtualProtect((LPVOID)addr, 0x13, PAGE_EXECUTE_READWRITE, &flOldProtect);
				if (change_protection) {
					memset((void*)(addr), 0x90, (0xE9 - 0xC9));
				}
				trampoline.write_branch<5>(addr, trampoline.allocate(patch));
			}
			else {
				stl::report_and_fail("Failed to load New Grass");
			}
			
		}
		
		// unloading cell
		if (!load_only)
		{
			if (addr = RELOCATION_ID(13623, 13721).address() + (0xC0F8 - 0xBF90); REL::make_pattern<"E8">().match(RELOCATION_ID(13623, 13721).address() + (0xC0F8 - 0xBF90))) {
				//Memory::WriteHook(new HookParameters() { Address = addr, IncludeLength = 0, ReplaceLength = 5, Before = [&] (std::any ctx)
				Utility::Memory::SafeWrite(addr, Utility::Assembly::NoOperation5);
				// This is not necessary because we want to keep grass on cell unload.
				//Handle_RemoveGrassFromCell_Call(ctx.CX, ctx.DX, "unload");
			}
			else {
				stl::report_and_fail("Failed to unload cell");
			}
		}
		
		// remove just before read grass so there's not so noticeable fade-in / fade-out, there is still somewhat noticeable effect though
		if (!load_only)
		{
			if (addr = RELOCATION_ID(15204, 15372).address() + 0x85; REL::make_pattern<"E8">().match(RELOCATION_ID(15204, 15372).address() + (0x8F - 0x10))) {
				//Memory::WriteHook(new HookParameters() { Address = addr, IncludeLength = 5, ReplaceLength = 5, After = [&] (std::any ctx)
				struct Patch : Xbyak::CodeGenerator
				{
					Patch(std::uintptr_t a_func, uintptr_t b_func, uintptr_t a_target)
					{
						Xbyak::Label funcLabel;
						Xbyak::Label funcLabel2;
						Xbyak::Label retnLabel;

						Xbyak::Label NotIf;

						movzx(eax, ptr[rbx]);
						xor_(r15d, r15d);
						cmp(rsi, 0);
						je(NotIf);

						mov(rcx, rsi);
						mov(rdx, r13);

						sub(rsp, 0x20);
						call(ptr[rip + funcLabel]);  // call our function
						add(rsp, 0x20);

						jmp(ptr[rip + retnLabel]);
						
						L(NotIf);
						sub(rsp, 0x20);
						call(ptr[rip + funcLabel2]);  // call our function
						add(rsp, 0x20);

						jmp(ptr[rip + retnLabel]);

						L(funcLabel);
						dq(a_func);

						L(funcLabel2);
						dq(b_func);

						L(retnLabel);
						dq(a_target + 0x6);
					}
				};
				Patch patch(reinterpret_cast<uintptr_t>(RemoveGrassHook), reinterpret_cast<uintptr_t>(CallDebugLog), addr);
				patch.ready();

				auto& trampoline = SKSE::GetTrampoline();
				trampoline.write_branch<6>(addr, trampoline.allocate(patch));
			}
			else {
				stl::report_and_fail("Failed to Remove Grass");
			}
			
		}
	
	    // Fix weird shape selection.
	    // Vanilla game groups shape selection by 12 x 12 cells, we want a shape per cell.
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
			Utility::Memory::SafeWrite(addr + 6, Utility::Assembly::NoOperation3);
		    trampoline.write_branch<6>(addr, trampoline.allocate(patch));
	    }
	    else {
		    stl::report_and_fail("Failed to fix shape selection");
	    }
			    
	    
	    if (addr = RELOCATION_ID(15205, 15373).address() + (0x5F6B - 0x5954);	REL::make_pattern<"E8">().match(RELOCATION_ID(15205, 15373).address() + (0x5F6B - 0x5940))) {
		    //Memory::WriteHook(new HookParameters() { Address = addr, IncludeLength = 5, ReplaceLength = 5, Before = [&] (std::any ctx)
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
	    }
	    else {
		    stl::report_and_fail("Failed to fix shape selection");
	    }
	    
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
	    }
	    else {
		    stl::report_and_fail("Failed to fix shape selection");
	    }
		
	    if (addr = RELOCATION_ID(15214, 15383).address() + (0x78B7 - 0x7830); REL::make_pattern<"66 44 89 7C 24 4C">().match(RELOCATION_ID(15214, 15383).address() + (0x78B7 - 0x7830))) {
		    //Memory::WriteHook(new HookParameters() { Address = addr, IncludeLength = 0, ReplaceLength = 0xC2 - 0xB7, Before = [&] (std::any ctx)

		    struct Patch : Xbyak::CodeGenerator
		    {
			    explicit Patch(uintptr_t a_target)
			    {
				    Xbyak::Label retnLabel;

				    mov(ptr[rsp + 0x48 + 4], r15w);
				    mov(ptr[rsp + 0x48 + 6], bx);

				    movsx(eax, r15w); // x
					mov(r15, rcx);
					cdq();
				    mov(ecx, 12);// x/12
				    idiv(ecx);
					mov(rcx, r15);
					mov(r15d, eax);

				    movsx(eax, bx); // y
					mov(rbx, rcx);
					cdq();
				    mov(ecx, 12); // y/12
				    idiv(ecx);
					mov(rcx, rbx);
					mov(ebx, eax);

					mov(ptr[rsp + 0x258 + 0x58], r15d);  // x
					mov(ptr[rsp + 0x258 + 0x60], ebx);  // y

				    jmp(ptr[rip + retnLabel]);

				    L(retnLabel);
					dq(a_target + (0xC2 - 0xB7));
			    }
		    };
		    Patch patch(addr);
		    patch.ready();
			
		    auto& trampoline = SKSE::GetTrampoline();
		    DWORD flOldProtect = 0;
			BOOL change_protection = VirtualProtect((LPVOID)addr, (0xC2 - 0xB7), PAGE_EXECUTE_READWRITE, &flOldProtect);
		    // Pass address of the DWORD variable ^
		    if (change_protection) {
			    memset((void*)(addr), 0x90, (0xC2 - 0xB7));
		    }
		    trampoline.write_branch<5>(addr, trampoline.allocate(patch));
	    }
	    else {
		    stl::report_and_fail("Failed to fix shape selection");
	    }
		
	    // Exterior cell buffer must be extended if grass radius is outside of ugrids.
	    // Reason: cell may get deleted while it still has grass and we can not keep grass there then.
	    if (!load_only)
	    {
		    if (addr = (RELOCATION_ID(13233, 13384).address() + (0xB2 - 0x60));  REL::make_pattern<"8B 05">().match(RELOCATION_ID(13233, 13384).address() + (0xB2 - 0x60))) {
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
		    }
		    else {
			    stl::report_and_fail("Failed to extend cell buffer");
		    }
	    }
		
	    // Allow grass distance to extend beyond uGrids * 4096 units (20480).
	    if (addr = (RELOCATION_ID(15202, 15370).address() + (0xB06 - 0x890)); REL::make_pattern<"C1 E0 0C">().match(RELOCATION_ID(15202, 15370).address() + (0xB06 - 0x890))) {
		    Memory::Internal::write<uint8_t>(addr + 2, 16, true);
	    }

	    if (addr = (RELOCATION_ID(528751, 15379).address() + (0xFE - 0xE0)); REL::make_pattern<"C1 E0 0C">().match(RELOCATION_ID(528751, 15379).address() + (0xFE - 0xE0))) {
		    Memory::Internal::write<uint8_t>(addr + 2, 16, true);
	    }
		
	    // Allow create grass without a land mesh. This is still necessary!
	    if (!load_only)
	    {
		    if (addr = (RELOCATION_ID(15204, 15372).address() + (0xED2 - 0xD10)); REL::make_pattern<"E8">().match(RELOCATION_ID(15204, 15372).address() + (0xED2 - 0xD10))) {
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
		    }
		    else {
			    stl::report_and_fail("Failed to Create Grass");
		    }
	    }
	    
	    // Cell unload should clear queued task. Otherwise it will crash or not allow creating grass again later.
	    if (!load_only)
	    {
		    if (addr = (RELOCATION_ID(18655, 19130).address() + (0xC888 - 0xC7C0)); REL::make_pattern<"E8">().match(RELOCATION_ID(18655, 19130).address() + (0xC888 - 0xC7C0))) {
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
		    }
		    else {
			    stl::report_and_fail("Failed to unload cell");
		    }
	    }
		
	    // Create custom way to load cell.
	    if (!load_only)
	    {
		    if (addr = RELOCATION_ID(18137, 18527).address() + 0x17; REL::make_pattern<"48 8B 4B 18 48 8D 53 20">().match(RELOCATION_ID(18137, 18527).address() + 0x17)) {
			    //Memory::WriteHook(new HookParameters() { Address = addr, IncludeLength = 8, ReplaceLength = 8, Before = [&] (std::any ctx)

			    struct Patch : Xbyak::CodeGenerator
			    {
				    Patch(std::uintptr_t a_func, std::uintptr_t b_func, uintptr_t a_target)
				    {
					    Xbyak::Label funcLabel;
					    Xbyak::Label funcLabel2;
					    Xbyak::Label retnLabel;
						Xbyak::Label skip;

					    Xbyak::Label j_else;
					    Xbyak::Label notIf;
					    Xbyak::Label include;

						push(rcx);
						push(rdx);
					    mov(al, ptr[rbx + 0x3E]);
					    cmp(al, 1);
					    jne(j_else);

					    //Skip();

					    mov(rcx, ptr[rbx + 0x20]); // ws
					    mov(edx, ptr[rbx + 0x30]);  // x
					    mov(r8d, ptr[rbx + 0x34]);  // y

					    sub(rsp, 0x20);
					    call(ptr[rip + funcLabel]);  // call our function
					    add(rsp, 0x20);

						pop(rcx);
						pop(rdx);
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
						pop(rcx);
						pop(rdx);
					    mov(rcx, ptr[rbx + 0x18]);
					    lea(rdx, ptr[rbx + 0x20]);
					    jmp(ptr[rip + retnLabel]);

					    L(funcLabel);
					    dq(a_func);

					    L(funcLabel2);
					    dq(b_func);

					    L(retnLabel);
						dq(a_target + 0x8);

						L(skip);
						dq(a_target + 0x8 + (0xA9 - 0x8F));
				    }
			    };
			    Patch patch(reinterpret_cast<uintptr_t>(CellLoadNow_Our), reinterpret_cast<uintptr_t>(ThrowOurMethodException), addr);
			    patch.ready();

			    auto& trampoline = SKSE::GetTrampoline();
				Utility::Memory::SafeWrite(addr + 5, Utility::Assembly::NoOperation3);
			    trampoline.write_branch<5>(addr, trampoline.allocate(patch));
		    }
		    else {
			    stl::report_and_fail("Failed to load cell");
		    }
			
		    if (addr = RELOCATION_ID(18150, 18541).address() + (0xB094 - 0xAF20); REL::make_pattern<"88 43 3D 8B 74 24 30">().match(RELOCATION_ID(18150, 18541).address() + (0xB094 - 0xAF20))) {
			    //Memory::WriteHook(new HookParameters() { Address = addr, IncludeLength = 7, ReplaceLength = 7, Before = [&] (std::any ctx)
			    struct Patch : Xbyak::CodeGenerator
			    {
				    Patch(uintptr_t a_func, uintptr_t a_target)
				    {
					    Xbyak::Label funcLabel;
					    Xbyak::Label retnLabel;
					
						push(rax);
						push(rcx);

					    mov(rcx, ptr[rbx+0x30]); // x
					    mov(rdx, ptr[rbx+0x34]); // y

				        sub(rsp, 0x20);
					    call(ptr[rip + funcLabel]); 
				        add(rsp, 0x20);

					    mov(byte[rbx + 0x3E], al);
						pop(rax);
						pop(rcx);
						mov(ptr[rbx + 0x3D], al);

				        mov(esi, ptr[rsp + 0x30]);

					    jmp(ptr[rip + retnLabel]);

					    L(funcLabel);
					    dq(a_func);

					    L(retnLabel);
					    dq(a_target + 0x7);
				    }
			    };
			    Patch patch(reinterpret_cast<uintptr_t>(CellLoadHook), addr);
			    patch.ready();

			    auto& trampoline = SKSE::GetTrampoline();
			    Utility::Memory::SafeWrite(addr + 5, Utility::Assembly::NoOperation2);
			    trampoline.write_branch<5>(addr, trampoline.allocate(patch));
		    }
		    else {
			    stl::report_and_fail("Failed to load cell");
		    }
			 
		    if (addr = RELOCATION_ID(18149, 18540).address() + (0xE1B - 0xCC0); REL::make_pattern<"C6 43 3C 01 44 88 7B 3D">().match(RELOCATION_ID(18149, 18540).address() + (0xE1B - 0xCC0))) {
			    //Memory::WriteHook(new HookParameters() { Address = addr, IncludeLength = 8, ReplaceLength = 8, Before = [&] (std::any ctx)
			    struct Patch : Xbyak::CodeGenerator
			    {
				    explicit Patch(uintptr_t a_target)
				    {
					    Xbyak::Label retnLabel;

					    mov(byte[rbx + 0x3E], 0);

					    mov(byte[rbx + 0x3C], 1);
					    mov(ptr[rbx + 0x3D], r15b);

					    jmp(ptr[rip + retnLabel]);

					    L(retnLabel);
					    dq(a_target + 0x8);
				    }
			    };
			    Patch patch(addr);
			    patch.ready();

			    auto& trampoline = SKSE::GetTrampoline();
			    Utility::Memory::SafeWrite(addr + 6, Utility::Assembly::NoOperation2);
			    trampoline.write_branch<5>(addr, trampoline.allocate(patch));
		    }
		    else {
			    stl::report_and_fail("Failed to load cell");
		    }
			
		    if (addr = RELOCATION_ID(13148, 18539).address() + (0x2630 - 0x2220); REL::make_pattern<"66 83 BC 24 90 00 00 00 00">().match(RELOCATION_ID(13148, 18539).address() + (0x2630 - 0x2220))) {
			    //Memory::WriteHook(new HookParameters() { Address = addr, IncludeLength = 9, ReplaceLength = 9, Before = [&] (std::any ctx)

			    struct Patch : Xbyak::CodeGenerator
			    {
				    Patch(std::uintptr_t a_func, uintptr_t a_target)
				    {
					    Xbyak::Label funcLabel;
					    Xbyak::Label retnLabel;
					    Xbyak::Label notIf;

                        push(r9);
						push(rdx);
						push(rax);
					    mov(r8d, ptr[rsp + 0x90]); //movedX
					    mov(r9d, r13w);  //movedY

					    mov(rax, ptr[rbx + 0x140]); // ws
					    test(rax, rax);
					    jne(notIf);

                        pop(r9);
						pop(rdx);
						pop(rax);
				        cmp(word[rsp + 0x90], 0);
					    jmp(ptr[rip + retnLabel]);

					    L(notIf);

					    mov(ecx, ptr[rbx + 0xB8]); // prevX
						mov(edx, ptr[rbx + 0xBC]);  // prevY

				        sub(rsp, 0x20);
					    call(ptr[rip + funcLabel]);
				        add(rsp, 0x20);

                        pop(r9);
						pop(rdx);
						pop(rax);
					    cmp(word[rsp + 0x90], 0);

					    jmp(ptr[rip + retnLabel]);

					    L(funcLabel);
					    dq(a_func);

					    L(retnLabel);
					    dq(a_target + 0x9);
				    }
			    };
			    Patch patch(reinterpret_cast<uintptr_t>(UpdateGrassGridQueue), addr);
			    patch.ready();

			    auto& trampoline = SKSE::GetTrampoline();
			    Utility::Memory::SafeWrite(addr + 5, Utility::Assembly::NoOperation4);
			    trampoline.write_branch<5>(addr, trampoline.allocate(patch));
		    }
		    else {
			    stl::report_and_fail("Failed to load cell");
		    }
		    
		    if (addr = (RELOCATION_ID(13148, 18539).address() + (0x29AF - 0x2220)); REL::make_pattern<"E8">().match(RELOCATION_ID(13148, 18539).address() + (0x29AF - 0x2220))) {
			    //Memory::WriteHook(new HookParameters(){ Address = addr, IncludeLength = 5, ReplaceLength = 5, After = [&](std::any ctx)
			    struct Patch : Xbyak::CodeGenerator
			    {
				    Patch(uintptr_t a_func, uintptr_t a_target)
				    {
						Xbyak::Label TES_Sub;
					    Xbyak::Label funcLabel;
					    Xbyak::Label retnLabel;

						call(ptr[rip + TES_Sub]);

					    push(rcx);
					    push(rdx);
					    mov(edx, ptr[rbx + 0xB0]);  // nowX
					    mov(r8d, ptr[rbx + 0xB4]); // nowY
					    mov(rcx, ptr[rbx + 0x140]); // ws

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
			    Patch patch(reinterpret_cast<uintptr_t>(UpdateGrassGridEnsureLoad), addr);
			    patch.ready();

			    auto& trampoline = SKSE::GetTrampoline();
			    trampoline.write_branch<5>(addr, trampoline.allocate(patch));
		    }
		    else {
			    stl::report_and_fail("Failed to load cell");
		    }
	    }

    }

	bool DistantGrass::_canUpdateGridNormal = false;
	bool DistantGrass::load_only = false;

	float DistantGrass::getChosenGrassFadeRange()
	{
		float r = *Config::OverwriteGrassFadeRange;
		if (r < 0.0f)
		{
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
		if (r < 0)
		{
			float dist = *Config::OverwriteGrassDistance;
			if (dist < 0.0f)
			{
				auto addr = RELOCATION_ID(501108, 359413).address();
				dist = Memory::Internal::read<float>(addr + 8);
			}

			float range = getChosenGrassFadeRange();

			float total = std::max(0.0f, dist) + std::max(0.0f, range);
			float cells = total / 4096.0f;

			int icells = static_cast<int>(std::ceil(cells));

			switch (*Config::DynDOLODGrassMode)
			{
				case 1:
					icells = Memory::Internal::read<int>(addr_uGrids + 8) / 2;
					break;

				case 2:
					icells = Memory::Internal::read<int>(addr_uLargeRef + 8) / 2;
					break;
			    default:
					break;
			}

			if (icells < 2)
			{
				r = 2;
			}
			else if (icells > 32)
			{
				SKSE::log::info("GrassControl: calculated iGrassCellRadius is {}! Using 32 instead. This probably means fGrassFadeRange or fGrassFadeStartDistance is very strange value.", icells);
				r = 32;
			}
			else
			{
				r = icells;
			}

			int imin = 0;
			if (*Config::OverwriteMinGrassSize >= 0)
			{
				imin = *Config::OverwriteMinGrassSize;
			}
			else
			{
				try
				{
					imin = Memory::Internal::read<int>(RELOCATION_ID(501113, 359421).address() + 8);
				}
				catch (...)
				{

				}
				//auto setting = RE::INISettingCollection::GetSingleton()->GetSetting("iMinGrassSize:Grass");
				//imin = setting->data.i;
			}

			SKSE::log::debug("ChosenGrassGridRadius(null) chose: {}, dist: {}, range: {}, iMinGrassSize: {}", r, dist, range,imin);
		
		//TODO InvariantCulture
			SKSE::log::info("ChosenGrassSettings: grid: {}, dist: {}, range: {}, iMinGrassSize: {}", r, dist, range, imin);
			
			_chosenGrassGridRadius = r;
		}

		return r;
	}

	int DistantGrass::_chosenGrassGridRadius = -1;
	bool DistantGrass::DidApply = false;

	void DistantGrass::CellLoadNow_Our(uintptr_t ws, int x, int y)
	{
		auto c = Map->GetFromGrid(x, y);
		if (c == nullptr || InterlockedCompareExchange64(&c->furtherLoad, 0, 2) != 2) {
			Map->GetFromGrid(x, y) = c;
			return;
		}
		Map->GetFromGrid(x, y) = c;

		if (ws == 0)
			return;

		REL::Relocation<uintptr_t (*)(uintptr_t, int, int)> func{ RELOCATION_ID(20026, 20460) };

		auto cell =  func(ws, x, y);
		if (IsValidLoadedCell(cell, false)) {
			auto obj = reinterpret_cast<RE::TESObjectCELL*>(cell);
			if(*obj->GetName()) {
			    SKSE::log::debug("FurtherLoadSuccessFirst({}, {}) Name: " + std::string(obj->GetName()) + ", FormId: {:x}", x, y, obj->formID);
			} else {
			     SKSE::log::debug("FurtherLoadSuccessFirst({}, {}) Name: Wilderness, FormId: {:x}", x, y, obj->formID);
			}
		}
		else if (cell != 0)
		{
			REL::Relocation<uintptr_t(*)(uintptr_t)> Func{ RELOCATION_ID(18513, 18970) };

			auto landPtr = Func(cell);
			if (landPtr != 0 && (Memory::Internal::read<uint8_t>(landPtr + 0x28) & 8) == 0)
			{
				REL::Relocation<void (*)(uintptr_t, int, int)> fnc{ RELOCATION_ID(18331, 18747) };

				fnc(landPtr, 0, 1);

				if (IsValidLoadedCell(cell, false)) {
					SKSE::log::debug("FurtherLoadSuccessSecond({}, {}) {}", c->x, c->y, cell);
				}
			}
		}
	}

	bool DistantGrass::IsLodCell(uintptr_t cell)
	{
		if (!DidApply || load_only)
			return false;

		if (cell == 0)
			return false;

		auto c = Map->FindByCell(cell);
		if (c == nullptr)
			return false;

		int d = c->self_data;
		Map->FindByCell(cell) = c;
		if ((d >> 8 & 0xFF) == static_cast<int>(GrassStates::Lod))
			return true;
		return false;
	}

	bool DistantGrass::ClearCellAddGrassTask(const uintptr_t cell)
	{
		if (cell == 0)
			return false;

	    uintptr_t task;
		{
			auto alloc = new char[0x10];
			Memory::Internal::write<uintptr_t>(reinterpret_cast<uintptr_t>(alloc), 0);

			REL::Relocation <void (*)(uintptr_t, uintptr_t)> func{ RELOCATION_ID(11933, 12072) };
			func(cell + 0x48, reinterpret_cast<uintptr_t>(alloc));

			task = Memory::Internal::read<uintptr_t>(alloc);
			delete[] alloc;
		}

		if (task == 0)
			return false;

		Memory::Internal::write<uint8_t>(task + 0x48, 1);
		volatile uint64_t* TaskAdr = &task + 8;
		if (InterlockedDecrement(TaskAdr) == 0)
		{
			auto vtable = Memory::Internal::read<intptr_t>(task);
			uintptr_t dtor = Memory::Internal::read<intptr_t>(vtable);
			REL::Relocation<void (*)(intptr_t, int)> func{ dtor };

			func(task, 1);
		}

		REL::Relocation<void (*)(intptr_t, int)> func{ RELOCATION_ID(11932, 12071) };

		func(cell + 0x48, 0);

		return true;
	}

	bool DistantGrass::IsValidLoadedCell(uintptr_t cell, bool quickLoad)
	{
		if (cell == 0)
			return false;

		if (quickLoad)
			return true;

		auto land = Memory::Internal::read<intptr_t>(cell + 0x68);
		if (land == 0)
			return false;

		auto data = Memory::Internal::read<intptr_t>(land + 0x40);
		if (data == 0)
			return false;

		return true;
	}

	uintptr_t DistantGrass::GetCurrentWorldspaceCell(uintptr_t tes, RE::TESWorldSpace* ws, int x, int y, bool quickLoad, bool allowLoadNow)
	{
		auto data = Memory::Internal::read<uintptr_t>(addr_DataHandler);
		if (data == 0 || ws == nullptr)
			return 0;

		using func_t = uintptr_t(*)(uintptr_t, int, int);
		REL::Relocation<func_t> func{ RELOCATION_ID(13174, 13319) };

		uintptr_t cell = func(tes, x, y);
		if (IsValidLoadedCell(cell, quickLoad))
		{
			auto obj = reinterpret_cast<RE::TESObjectCELL*>(cell);
			if(*obj->GetName()) {
			    SKSE::log::debug("add_GetCell({}, {}) (0): Name: " + std::string(obj->GetName()) + ", FormId: {:x}", x, y, obj->formID);
			} else {
			     SKSE::log::debug("add_GetCell({}, {}) (0): Name: Wilderness, FormId: {:x}", x, y, obj->formID);
			}
			
			return cell;
		}

		if (allowLoadNow) {
			auto c = Map->GetFromGrid(x, y);
			if (c != nullptr) {
				InterlockedCompareExchange64(&c->furtherLoad, 0, 2);
				Map->GetFromGrid(x, y) = c;
			}
			{
				REL::Relocation<void (*)()> Func{ RELOCATION_ID(13188, 13333) };
			    Func();

			    try {
					REL::Relocation<uintptr_t (*)(RE::TESWorldSpace*, int16_t, int16_t)> Func{ RELOCATION_ID(20026, 20460) };
				    cell = Func(ws, x, y);
					auto obj = (RE::TESObjectCELL*)cell;
				    if (IsValidLoadedCell(cell, quickLoad)) {
						if(*obj->GetName()) {
			                SKSE::log::debug("add_GetCell({}, {}) (0): Name: " + std::string(obj->GetName()) + ", FormId: {:x}", x, y,obj->formID);
			            } else {
			                 SKSE::log::debug("add_GetCell({}, {}) (0): Name: Wilderness, FormId: {:x}", x, y,obj->formID);
			            }
					    return cell;
				    }
			    }
			    catch (...) {
					REL::Relocation <void (*)()> Fnc{ RELOCATION_ID(13189, 13334) };
					Fnc();

					logger::debug("Error Loading Cell({}, {})", x, y);
			    }
				REL::Relocation<void (*)()> Fnc{ RELOCATION_ID(13189, 13334) };
			    Fnc();
			   
		    }
			if (cell != 0)
			{
				REL::Relocation <uintptr_t (*)(uintptr_t)> func{ RELOCATION_ID(18513, 18970) };

				auto landPtr = func(cell);
				if (landPtr != 0 && (Memory::Internal::read<uint8_t>(landPtr + 0x28) & 8) == 0)
				{
					REL::Relocation<void (*)(uintptr_t, int, int)> Func{ RELOCATION_ID(18331, 18747) };

					Func(landPtr, 0, 1);

					if (IsValidLoadedCell(cell, false))
					{
						auto obj = reinterpret_cast<RE::TESObjectCELL*>(cell);
						if(*obj->GetName()) {
			                SKSE::log::debug("add_GetCell({}, {}) (0): Name: " + std::string(obj->GetName()) + ", FormId: {:x}", x, y, obj->formID);
			            } else {
			                 SKSE::log::debug("add_GetCell({}, {}) (0): Name: Wilderness, FormId: {:x}", x, y,obj->formID);
			            }
						
						return cell;
					}
				}
			}
		}

		std::string failData = "null";
		if (cell != 0)
		{
			auto obj = reinterpret_cast<RE::TESObjectCELL*>(cell);
			
			auto cellStates = obj->cellState.get();
			failData = "loadState: " + std::to_string(static_cast<int>(cellStates)) + " hasTemp: " + std::to_string((Memory::Internal::read<uint8_t>(cell + 0x40) & 0x10) != 0 ? 1 : 0);
			
		}
		SKSE::log::debug("add_GetCell({}, {}) FAIL " + failData, x, y);

		return 0;
	}

	void DistantGrass::Call_AddGrassNow(uintptr_t cell, uintptr_t customArg)
	{
		if (Memory::Internal::read<uint8_t>(customArg) != 0)
		    return;

		{
			std::scoped_lock lock(locker());
			std::shared_ptr<cell_info> c = Map->FindByCell(cell);
			if (c == nullptr)
			{
			    SKSE::log::debug("AddedGrass(null): warning: c == null");
			    return;
			}

			int d = c->self_data;
			int tg = d >> 8 & 0xFF;

			bool quickLoad = (d >> 16 & 0xFF) != 0;
			if (IsValidLoadedCell(cell, quickLoad))
			{
				auto grassMgr = Memory::Internal::read<uintptr_t>(addr_GrassManager);

				auto obj = reinterpret_cast<RE::TESObjectCELL*>(cell);
				if(*obj->GetName()) {
				    SKSE::log::debug("AddingGrass({}, {}) ca: {}, cell: Name: " + std::string(obj->GetName()) + ", FormId: {:x}", c->x, c->y, Memory::Internal::read<uint8_t>(customArg), obj->formID);
				} else {
				    SKSE::log::debug("AddingGrass({}, {}) ca: {}, cell: Name: Wilderness, FormId: {:x}", c->x, c->y, Memory::Internal::read<uint8_t>(customArg),obj->formID);
			    }

				using func_t = void(*)(uintptr_t, uintptr_t, uintptr_t);
				REL::Relocation<func_t> func{ RELOCATION_ID(15204, 15372) };

				func(grassMgr, cell, customArg);

				SKSE::log::debug("AddedGrass({}, {})", c->x, c->y);
			} else {
				SKSE::log::debug("AddedGrassFAIL({}, {}) <NotValidLoadedCell>", c->x, c->y);
			}

			// Set this anyway otherwise we get stuck.
			c->self_data = tg;
			Map->FindByCell(cell) = c;
		}
	}
	GrassStates DistantGrass::GetWantState(const std::shared_ptr<cell_info>& c, const int curX, const int curY, const int uGrid, const int grassRadius, const bool canLoadFromFile, const std::string& wsName)
	{
		int diffX = std::abs(curX - c->x);
		int diffY = std::abs(curY - c->y);

		if (diffX > grassRadius || diffY > grassRadius)
		{
			return GrassStates::None;
		}

		if (load_only)
		{
			return GrassStates::Active;
		}

		int uHalf = uGrid / 2;
		if (diffX > uHalf || diffY > uHalf)
		{
			// Special case: if we are loading and not generating anyway and already have active file we can take active instead of lod
			if (canLoadFromFile && c->checkHasFile(wsName, false))
			{
				return GrassStates::Active;
			}

			return GrassStates::Lod;
		}

		return GrassStates::Active;
	}

	void DistantGrass::Handle_RemoveGrassFromCell_Call(uintptr_t grassMgr, uintptr_t cell)
	{
		grassMgr = Memory::Internal::read<uintptr_t>(grassMgr);
		std::string why = "celldtor";
		if (cell == 0)
		{
			SKSE::log::debug("RemoveGrassOther(null): <- " + why);
			return;
		}
		{
			std::scoped_lock lock(locker());
			std::shared_ptr<cell_info> c = Map->FindByCell(cell);
			if (c != nullptr)
			{
				if (c->self_data >> 24 != 0)
				{
					SKSE::log::debug("RemoveGrassOther({}, {}) <- " + why + " warning: busy", c->x, c->y);
				}
				else
				{
					SKSE::log::debug("RemoveGrassOther({}, {}) <- " + why, c->x, c->y);
				}

				c->self_data = 0;
				c->cell = 0;
				Map->map.erase(cell);
				ClearCellAddGrassTask(cell);
			} else {
				auto cellObj = reinterpret_cast<RE::TESObjectCELL*>(cell);
				if(!cellObj->IsInteriorCell()) {
					SKSE::log::debug("cell_dtor({}, {})", cellObj->GetCoordinates()->cellX, cellObj->GetCoordinates()->cellY);
				}
			}
			Map->FindByCell(cell) = c;
		}
		REL::Relocation<void (*)(intptr_t, intptr_t)> func{ RELOCATION_ID(15207, 15375) };

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

	void DistantGrass::UpdateGrassGridEnsureLoad(uintptr_t ws, const int nowX, const int nowY)
	{
		if (ws == 0)
			return;

		int grassRadius = getChosenGrassGridRadius();
		int uGrids = Memory::Internal::read<int>(addr_uGrids + 8);
		int uHalf = uGrids / 2;
		int bigSide = std::max(grassRadius, uHalf);
		bool canLoadGrass = Memory::Internal::read<uint8_t>(addr_AllowLoadFile + 8) != 0;

		std::string wsName;
		try
		{
			auto wsObj = reinterpret_cast<RE::TESWorldSpace*>(ws);
			wsName = wsObj->editorID.c_str();
		}
		catch (...)
		{

		}

		for (int x = nowX - bigSide; x <= nowX + bigSide; x++)
		{
			for (int y = nowY - bigSide; y <= nowY + bigSide; y++)
			{
				unsigned char wantState = CalculateLoadState(nowX, nowY, x, y, uHalf, grassRadius);
				if (wantState != 1)
					continue;

				auto c = Map->GetFromGrid(x, y);
				if (c == nullptr)
					continue;

				auto xs = static_cast<short>(x);
				auto ys = static_cast<short>(y);

				auto xsu = static_cast<unsigned short>(xs);

				auto ysu = static_cast<unsigned short>(ys);

				unsigned int mask = static_cast<unsigned int>(xsu) << 16;
				mask |= ysu;

				REL::Relocation <uintptr_t (*)(uintptr_t, uintptr_t)> func{ RELOCATION_ID(13549, 13641) };

				auto cell = func(ws, mask);
				if (IsValidLoadedCell(cell, false))
					continue;

				bool quickLoad = false;
				if (canLoadGrass && (c->checkHasFile(wsName, false) || c->checkHasFile(wsName, true)))
				{
					quickLoad = true;
				}

				if (quickLoad && IsValidLoadedCell(cell, true))
					continue;

				InterlockedCompareExchange64(&c->furtherLoad, 0, 2);
				Map->GetFromGrid(x, y) = c;

				REL::Relocation<uintptr_t(*)(uintptr_t, int, int)> func2{ RELOCATION_ID(20026, 20460) };

				cell = func2(ws, x, y);
				if (IsValidLoadedCell(cell, false))
				{
				    SKSE::log::debug("InstantLoadSuccessFirst({}, {}) {}", x, y, std::to_string(cell));
				}
				else if (cell != 0)
				{
					REL::Relocation<uintptr_t(*)(uintptr_t)> Func{ RELOCATION_ID(18513, 18970) };

					auto landPtr = Func(cell);
					if (landPtr != 0 && (Memory::Internal::read<uint8_t>(landPtr + 0x28) & 8) == 0)
					{
						REL::Relocation<void (*)(intptr_t, int, int)> fnc{ RELOCATION_ID(18331, 18747) };

						fnc(landPtr, 0, 1);

						if (IsValidLoadedCell(cell, false))
						{
						    SKSE::log::debug("InstantLoadSuccessSecond({}, {}) {}", c->x, c->y,cell);
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

		std::string wsName;
		try
		{
			//auto wsObj = (RE::TESWorldSpace*)ws;
			wsName = wsObj->editorID.c_str();
		}
		catch (...)
		{

		}

		int nowX = prevX + movedX;
		int nowY = prevY + movedY;

		for (int x = nowX - bigSide; x <= nowX + bigSide; x++)
		{
			for (int y = nowY - bigSide; y <= nowY + bigSide; y++)
			{
				unsigned char wantState = CalculateLoadState(nowX, nowY, x, y, uHalf, grassRadius);
				if (wantState != 1)
					continue;

				auto c = Map->GetFromGrid(x, y);
				if (c == nullptr)
					continue;

				auto xs = static_cast<short>(x);
				auto ys = static_cast<short>(y);

				auto xsu = static_cast<unsigned short>(xs);

				auto ysu = static_cast<unsigned short>(ys);

				unsigned int mask = static_cast<unsigned int>(xsu) << 16;
				mask |= ysu;

				REL::Relocation<intptr_t (*)(RE::TESWorldSpace*, intptr_t)> func{ RELOCATION_ID(13549, 13641) };

				auto cell = func(wsObj, mask);
				if (IsValidLoadedCell(cell, false))
					continue;

				bool quickLoad = false;
				if (canLoadGrass && (c->checkHasFile(wsName, false) || c->checkHasFile(wsName, true)))
					quickLoad = true;

				if (quickLoad && IsValidLoadedCell(cell, true))
					continue;

 				if (InterlockedCompareExchange64(&c->furtherLoad, 1, 0) == 0)
				{
					REL::Relocation<void (*)(uintptr_t, RE::TESWorldSpace*, int, int, int)> Func{ RELOCATION_ID(18150, 18541) };

					Func(Memory::Internal::read<uintptr_t>(addr_QueueLoadCellUnkGlobal), wsObj, x, y, 0);
				}
				Map->GetFromGrid(x, y) = c;
			}
		}
	}

	void DistantGrass::UpdateGrassGridNow(const uintptr_t tes, int movedX, int movedY, int addType)
	{
		if (load_only)
		{
			UpdateGrassGridNow_LoadOnly(tes, movedX, movedY, addType);
			return;
		}
		
		SKSE::log::debug("UpdateGrassGridNowBegin({}, {}) type: {}", movedX, movedY, addType);

		int grassRadius = getChosenGrassGridRadius();
		int uGrids = Memory::Internal::read<int>(addr_uGrids + 8);
		int uHalf = uGrids / 2;
		int bigSide = std::max(grassRadius, uHalf);
		auto ws = Memory::Internal::read<uintptr_t>(tes + 0x140);
		auto grassMgr = Memory::Internal::read<uintptr_t>(addr_GrassManager);
		bool canLoadGrass = Memory::Internal::read<uint8_t>(addr_AllowLoadFile + 8) != 0;
		RE::TESWorldSpace *wsObj = nullptr;
		std::string wsName;
		if (ws != 0)
		{
			wsObj = (RE::TESWorldSpace*)ws; 
		    wsName = wsObj->editorID.c_str();
		}

		auto nowX = Memory::Internal::read<int32_t>(tes + 0xB0);
		auto nowY = Memory::Internal::read<int32_t>(tes + 0xB4);

		auto invokeList = std::vector<uintptr_t>();
		if (addType <= 0)
		{
			{
				std::scoped_lock lock(locker());
				Map->unsafe_ForeachWithState([&](const std::shared_ptr<cell_info>& c)
				{
					auto want = addType < 0 ? GrassStates::None : GetWantState(c, nowX, nowY, uGrids, grassRadius, false, "");
					if (want == GrassStates::None)
					{
						auto cell = c->cell;
						c->cell = 0;
						c->self_data = 0;
						ClearCellAddGrassTask(cell);

					    SKSE::log::debug("RemoveGrassGrid({}, {})", c->x, c->y);

						invokeList.push_back(cell);
						return false;
					}

					return true;
				});
			}

			if (!invokeList.empty())
			{
				for (auto cell : invokeList)
				{
					REL::Relocation<void (*)(intptr_t, intptr_t)> func{ RELOCATION_ID(15207, 15375) };

					func(grassMgr, cell);
				}

				invokeList.clear();
			}
		}

		if (addType >= 0)
		{
			{
				std::scoped_lock lock(locker());
				int minX = nowX - bigSide;
				int maxX = nowX + bigSide;
				int minY = nowY - bigSide;
				int maxY = nowY + bigSide;

                SKSE::log::debug("X: ({}, {})", minX, maxX);
                SKSE::log::debug("NowX: ({}, {})", nowX, 0);
                SKSE::log::debug("Bigside: ({}, {})", bigSide, 0);
                SKSE::log::debug("X: ({}, {})", minX, maxX);
                SKSE::log::debug("NowX: ({}, {})", nowX, 0);
                SKSE::log::debug("Y: ({}, {})", minY, maxY);
                SKSE::log::debug("NowY: ({}, {})", nowY, 0);
                SKSE::log::debug("GrassRadius: ({}, {})", grassRadius, 0);
                SKSE::log::debug("uHalf: ({}, {})", uHalf, 0);
            

				// Add grass after.
				for (int x = minX; x <= maxX; x++)
				{
					for (int y = minY; y <= maxY; y++)
					{
						auto c = Map->GetFromGrid(x, y);
						if (c == nullptr)
						{
							continue;
						}

						int d = c->self_data;
						bool busy = (d >> 24) != 0;

						auto want = GetWantState(c, nowX, nowY, uGrids, grassRadius, canLoadGrass, wsName);
						if (want > static_cast<GrassStates>(d & 0xFF)) // this check is using > because there's no need to set lod grass if we already have active grass
						{
							bool canQuickLoad = want == GrassStates::Active || (canLoadGrass && c->checkHasFile(wsName, true));
							auto cellPtr = GetCurrentWorldspaceCell(tes, wsObj, x, y, canQuickLoad, addType > 0);

							if (cellPtr != 0)
							{
								if (c->cell != cellPtr)
								{
									if (c->cell != 0)
									{
										SKSE::log::debug("c.cell({}, {}) warning: already had cell!", c->x, c->y);
									}
									c->cell = cellPtr;
									Map->map.insert_or_assign(cellPtr, c);
								}

								// set busy and target
								c->self_data = 0x01000000 | (canQuickLoad ? 0x00010000 : 0) | (static_cast<int>(want) << 8) | (d & 0xFF);

								SKSE::log::debug("QueueGrass({}, {})", x, y);

								if (!busy)
									invokeList.push_back(cellPtr);
							}
						}
						Map->GetFromGrid(x, y) = c;
					}
				}
			}

			if (!invokeList.empty())
			{
				for (auto cell : invokeList)
				{
					// The way game does it is do it directly if generate grass files is enabled, But we don't want that because it causes a lot of stuttering.
					REL::Relocation<void (*)(intptr_t)> func{ RELOCATION_ID(13137, 13277) };

					func(cell);
				}
			}
		}

		SKSE::log::debug("UpdateGrassGridNowEnd({}, {}) type: {}", movedX, movedY,addType);
	}

	void DistantGrass::UpdateGrassGridNow_LoadOnly(const uintptr_t tes, int movedX, int movedY, int addType)
	{
		SKSE::log::debug("UpdateGrassGridNowBegin_LoadOnly({}, {}) type: {}", movedX, movedY, addType);

		int grassRadius = getChosenGrassGridRadius();
		auto ws = Memory::Internal::read<uintptr_t>(tes + 0x140);
		auto grassMgr = Memory::Internal::read<uintptr_t>(addr_GrassManager);
		RE::TESWorldSpace* wsObj = nullptr;
		if (ws != 0)
		{
			wsObj = reinterpret_cast<RE::TESWorldSpace*>(ws);
		}

		auto nowX = Memory::Internal::read<int32_t>(tes + 0xB0);
		auto nowY = Memory::Internal::read<int32_t>(tes + 0xB4);

		if (addType <= 0)
		{
			LO2Map->UpdatePositionWithRemove(wsObj, addType, nowX, nowY, grassRadius);
		}

		if (addType >= 0)
		{
			auto minX = static_cast<short>(nowX - grassRadius);
			auto maxX = static_cast<short>(nowX + grassRadius);
			auto minY = static_cast<short>(nowY - grassRadius);
			auto maxY = static_cast<short>(nowY + grassRadius);

			// Add grass after.
			for (short x = minX; x <= maxX; x++)
			{
				for (short y = minY; y <= maxY; y++)
				{
					LO2Map->QueueLoad(wsObj, x, y);
				}
			}
		}

		SKSE::log::debug("UpdateGrassGridNowEnd_LoadOnly({}, {}) type: {}; c_count: {}; shapeCount: {}", movedX, movedY, addType, LO2Map->GetCount(), Memory::Internal::read<int>(grassMgr + 0x58));
	}
}




