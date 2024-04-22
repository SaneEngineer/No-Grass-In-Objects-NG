#pragma once

#include "CasualLibrary.hpp"
#include "GrassControl/GidFileCache.h"
#include "GrassControl/Main.h"

namespace GrassControl
{
	enum class GrassStates : unsigned char
	{
		None,

		Lod,

		Active
	};

	static std::mutex& NRlocker()
	{
		static std::mutex NRlocker;
		return NRlocker;
	}

	static std::recursive_mutex& locker()
	{
		static std::recursive_mutex locker;
		return locker;
	}

	class DistantGrass final
	{
		class LoadOnlyCellInfoContainer2;

	public:
		static void RemoveGrassHook(RE::TESObjectCELL* cell, uintptr_t arg_1);

		static void CellUnloadHook(bool did, RE::TESObjectCELL* cellObj);

		static void ReplaceGrassGrid(bool _loadOnly);

	private:
		static bool _canUpdateGridNormal;

		static bool load_only;

		static float getChosenGrassFadeRange();

		static int getChosenGrassGridRadius();
		static int _chosenGrassGridRadius;

		static bool DidApply;
		static void CellLoadNow_Our(uintptr_t ws, int x, int y);

		static uintptr_t addr_uGrids;
		static uintptr_t addr_AllowLoadFile;
		static uintptr_t addr_DataHandler;
		static uintptr_t addr_uLargeRef;
		static uintptr_t addr_QueueLoadCellUnkGlobal;

		// This is only ever called from when we are adding grass, calling from outside is not valid.
	public:
		static bool IsLodCell(RE::TESObjectCELL* cell);

		static void InstallHooks()
		{
			Hooks::Install();
		}

	private:
		static bool ClearCellAddGrassTask(const RE::TESObjectCELL* cell);

		class cell_info final
		{
		public:
			cell_info(int _x, int _y);
			cell_info();

			int x;
			int y;

			RE::TESObjectCELL* cell = nullptr;
			int self_data = 0;
			volatile long long furtherLoad = 0;

			[[nodiscard]] bool checkHasFile(const std::string& wsName, bool lod) const;
		};

		class CellInfoContainer
		{
		public:
			CellInfoContainer();

			[[nodiscard]] std::optional<cell_info> GetFromGrid(int x, int y) const;

		private:
			std::vector<cell_info> grid{};

		public:
			std::unordered_map<RE::TESObjectCELL*, cell_info> map = std::unordered_map<RE::TESObjectCELL*, cell_info>(1024);

			void unsafe_ForeachWithState(const std::function<bool(cell_info)>& action);

			std::optional<cell_info> FindByCell(RE::TESObjectCELL* cell);
		};

		static unsigned char CellLoadHook(int x, int y);

		inline static std::unique_ptr<CellInfoContainer> Map;
		//private static LoadOnlyCellInfoContainer LOMap;
		inline static std::unique_ptr<LoadOnlyCellInfoContainer2> LO2Map;

		static bool IsValidLoadedCell(RE::TESObjectCELL* cell, bool quickLoad);

		static RE::TESObjectCELL* GetCurrentWorldspaceCell(const RE::TES* tes, RE::TESWorldSpace* ws, int x, int y, bool quickLoad, bool allowLoadNow);

		static uintptr_t tryCheckCell(RE::TESObjectCELL* cell, RE::TESWorldSpace* ws, int x, int y, bool quickLoad);

		static void Call_AddGrassNow(RE::BGSGrassManager* GrassMgr, RE::TESObjectCELL* cell, uintptr_t customArg);

		static GrassStates GetWantState(const cell_info& c, int curX, int curY, int uGrid, int grassRadius, bool canLoadFromFile, const std::string& wsName);

		static void Handle_RemoveGrassFromCell_Call(RE::TESObjectCELL* cell);

		static unsigned char CalculateLoadState(int nowX, int nowY, int x, int y, int ugrid, int ggrid);

		static void UpdateGrassGridEnsureLoad(RE::TESWorldSpace* ws, int nowX, int nowY);

		static void UpdateGrassGridQueue(int prevX, int prevY, int movedX, int movedY);

		static void UpdateGrassGridNow(const RE::TES* tes, int movedX, int movedY, int addType);

		static void UpdateGrassGridNow_LoadOnly(const RE::TES* tes, int movedX, int movedY, int addType);

		class LoadOnlyCellInfoContainer2 final
		{
			class _cell_data final
			{
			public:
				RE::TESObjectCELL* DummyCell_Ptr = nullptr;
				unsigned int WsId = 0;
				int X = 0;
				int Y = 0;
				enum class _cell_states : int
				{
					None = 0,
					Queued = 1,
					Loading = 2,
					Loaded = 3,
					Abort = 4
				};
				_cell_states State = _cell_states::None;
			};

			struct case_insensitive_unordered_map
			{
				struct comp
				{
					bool operator()(const std::string& lhs, const std::string& rhs) const
					{
						// On non Windows OS, use the function "strcasecmp" in #include <strings.h>
						return _stricmp(lhs.c_str(), rhs.c_str()) == 0;
					}
				};
				struct hash
				{
					std::size_t operator()(std::string str) const
					{
						for (std::size_t index = 0; index < str.size(); ++index) {
							auto ch = static_cast<unsigned char>(str[index]);
							str[index] = static_cast<unsigned char>(std::tolower(ch));
						}
						return std::hash<std::string>{}(str);
					}
				};
			};

			std::unordered_map<std::string, std::shared_ptr<_cell_data>, case_insensitive_unordered_map::hash, case_insensitive_unordered_map::comp> map = std::unordered_map<std::string, std::shared_ptr<_cell_data>, case_insensitive_unordered_map::hash, case_insensitive_unordered_map::comp>(256);

			// Only used for debug message so it's fine to be slow.
		public:
			[[nodiscard]] int GetCount() const;

		private:
			static std::string MakeKey(const std::string& ws, int x, int y);

		public:
			void UpdatePositionWithRemove(RE::TESWorldSpace* ws, int addType, int nowX, int nowY, int grassRadius);

			void QueueLoad(RE::TESWorldSpace* ws, int x, int y);

		private:
			void _DoUnload(const std::shared_ptr<_cell_data>& d);

		public:
			void Unload(const RE::TESWorldSpace* ws, int x, int y);

			void _DoLoad(const RE::TESWorldSpace* ws, int x, int y) const;
		};

	protected:
		struct Hooks
		{
			struct WriteProgress
			{
				static void thunk(RE::BGSGrassManager* GrassMgr, RE::TESObjectCELL* cell, uintptr_t unk)
				{
					if (exists(std::filesystem::path(Util::getProgressFilePath())))
						return;

					if (Config::OnlyLoadFromCache) {
						if (cell != nullptr) {
							auto ext = cell->GetCoordinates();
							auto x = ext->cellX;
							auto y = ext->cellY;
							LO2Map->_DoLoad(cell->worldSpace, x, y);
						}
					} else {
						Call_AddGrassNow(GrassMgr, cell, unk);
					}
				}
				static inline REL::Relocation<decltype(thunk)> func;
			};

			struct CellSelection
			{
				static void thunk(uintptr_t arg_1, uintptr_t arg_2, int arg_3, int arg_4, uintptr_t arg_5, uintptr_t arg_6)
				{
					func(arg_1, arg_2, arg_3 * 12, arg_4 * 12, arg_5, arg_6);
				}
				static inline REL::Relocation<decltype(thunk)> func;
			};

			struct CellSelection2
			{
				static void thunk(uintptr_t arg_1, uintptr_t arg_2, int arg_3, int arg_4, uintptr_t arg_5, uintptr_t arg_6)
				{
					func(arg_1, arg_2, arg_3 * 12, arg_4 * 12, arg_5, arg_6);
				}
				static inline REL::Relocation<decltype(thunk)> func;
			};

			static void Install()
			{
				if(Config::ExtendGrassDistance) {
					stl::write_thunk_jump<WriteProgress>(REL_ID(13138, 13278).address() + OFFSET(0xF, 0xF));
#ifdef SKYRIM_AE
					stl::write_thunk_call<CellSelection>(REL_ID(15206, 15374).address() + OFFSET(0x645C - 0x6200, 0x645C - 0x6200));
					stl::write_thunk_call<CellSelection>(REL_ID(15204, 15372).address() + OFFSET(0x2F5, 0x2F5));
#endif
				}
			}
		};
	};
}
