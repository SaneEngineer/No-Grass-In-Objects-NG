#pragma once

#include "CasualLibrary/CasualLibrary.hpp"
#include "GrassControl/Main.h"
#include "GrassControl/Util.h"


namespace GrassControl
{
	template<typename K, typename V>
	std::vector<std::pair<K, V>> mapToVector(const std::unordered_map<K, V>& map) {
		return std::vector<std::pair<K, V>>(map.begin(), map.end());
	}

	enum class GrassStates : unsigned char
	{
		None,

		Lod,

		Active
	};

	static std::mutex& locker()
	{
		static std::mutex locker;
		return locker;
	}

	class DistantGrass final
	{
		class LoadOnlyCellInfoContainer2;

	public:
		static void GrassLoadHook(uintptr_t ws, int x, int y);

		static void RemoveGrassHook(uintptr_t cell, uintptr_t arg_1);

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

		static uintptr_t addr_GrassManager;
		static uintptr_t addr_uGrids;
		static uintptr_t addr_AllowLoadFile;
		static uintptr_t addr_DataHandler;
	    static uintptr_t addr_uLargeRef;
		static uintptr_t addr_QueueLoadCellUnkGlobal;

		// This is only ever called from when we are adding grass, calling from outside is not valid.
	public:
		static bool IsLodCell(uintptr_t cell);

	private:
		static bool ClearCellAddGrassTask(uintptr_t cell);

		class cell_info final
		{
		public:
			cell_info(int _x, int _y);

			const int x;
			const int y;

			uintptr_t cell;
			int self_data = 0;
			volatile long long furtherLoad = 0;

			bool checkHasFile(const std::string &wsName, bool lod) const;
		};

		class CellInfoContainer
		{
		public:
			CellInfoContainer();

			std::shared_ptr<cell_info> GetFromGrid(int x, int y) const;

		private:
		    std::vector<std::shared_ptr<cell_info>> grid;

		public:
			std::unordered_map<uintptr_t, std::shared_ptr<cell_info>> map = std::unordered_map<uintptr_t, std::shared_ptr<cell_info>>(1024);

			void unsafe_ForeachWithState(const std::function<bool(std::shared_ptr<cell_info>)> &action);

			std::shared_ptr<cell_info> FindByCell(uintptr_t cell);
		};
	
		static unsigned char CellLoadHook(int x, int y);

		inline static std::unique_ptr<CellInfoContainer> Map;
		//private static LoadOnlyCellInfoContainer LOMap;
		inline static std::unique_ptr <LoadOnlyCellInfoContainer2> LO2Map;

		static bool IsValidLoadedCell(uintptr_t cell, bool quickLoad);

		static uintptr_t GetCurrentWorldspaceCell(uintptr_t tes, RE::TESWorldSpace* ws, int x, int y, bool quickLoad, bool allowLoadNow);

		static uintptr_t tryCheckCell(intptr_t cell, uintptr_t ws, int x, int y, bool quickLoad);

		static void Call_AddGrassNow(uintptr_t cell, uintptr_t customArg);

		static GrassStates GetWantState(const std::shared_ptr<cell_info>& c, int curX, int curY, int uGrid, int grassRadius, bool canLoadFromFile, const std::string& wsName);

		static void Handle_RemoveGrassFromCell_Call(uintptr_t grassMgr, uintptr_t cell);

		static unsigned char CalculateLoadState(int nowX, int nowY, int x, int y, int ugrid, int ggrid);

		static void UpdateGrassGridEnsureLoad(uintptr_t ws, int nowX, int nowY);

		static void UpdateGrassGridQueue(int prevX, int prevY, int movedX, int movedY);

		static void UpdateGrassGridNow(uintptr_t tes, int movedX, int movedY, int addType);

		static void UpdateGrassGridNow_LoadOnly(uintptr_t tes, int movedX, int movedY, int addType);

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
				_cell_states State = _cell_data::_cell_states::None;
			};

			struct case_insensitive_unordered_map {
				struct comp {
					bool operator() (const std::string& lhs, const std::string& rhs) const {
						// On non Windows OS, use the function "strcasecmp" in #include <strings.h>
						return _stricmp(lhs.c_str(), rhs.c_str()) == 0;
					}
					
				};
				struct hash {

					std::size_t operator() (std::string str) const {
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
			int GetCount() const;

		private:
			static std::string MakeKey(const std::string &ws, int x, int y);

		public:
			void UpdatePositionWithRemove(RE::TESWorldSpace *ws, int addType, int nowX, int nowY, int grassRadius) const;

			void QueueLoad(RE::TESWorldSpace *ws, int x, int y);

		private:
			void _DoUnload(const std::shared_ptr<_cell_data>& d) const;

		public:
			void Unload(const RE::TESWorldSpace *ws, int x, int y) const;

			void _DoLoad(const RE::TESWorldSpace *ws, int x, int y) const;
		};
	};
}
