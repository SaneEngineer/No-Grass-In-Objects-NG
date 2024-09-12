#pragma once

#include "GrassControl/Main.h"

namespace GrassControl
{
	class GidFileGenerationTask;
	class GidFileWorldGenerateTask;
	class GidFileCellGenerateTask;

	struct case_insensitive_unordered_set
	{
		struct comp
		{
			bool operator()(const std::string& Left, const std::string& Right) const noexcept
			{
				return Left.size() == Right.size() && std::equal(Left.begin(), Left.end(), Right.begin(),
														  [](char a, char b) {
															  return tolower(a) == tolower(b);
														  });
			}
		};
		struct hash
		{
			size_t operator()(const std::string& Keyval) const noexcept
			{
				size_t h = 0;
				std::ranges::for_each(Keyval.begin(), Keyval.end(), [&](char c) {
					h += tolower(c);
				});
				return h;
			}
		};
	};

	static std::mutex ProgressLocker;

	class GidFileCache final
	{
	public:
		static void FixFileFormat(bool only_load);

	private:
		static uintptr_t CustomGrassFileName;

		static uintptr_t CustomGrassLodFileName;
	};

	class GidFileGenerationTask final
	{
	public:
		GidFileGenerationTask();

		static uintptr_t addr_GrassMgr;
		static uintptr_t addr_uGrids;

		static int getChosenGrassGridRadius();

	private:
		static int _chosenGrassGridRadius;

	public:
		static void run_freeze_check();

		static void apply();

		static void IncrementHook();

		static void ExchangeHook();

		static int DoneWS;
		static int TotalWS;

	private:
		static std::ofstream FileStream;

	public:
		static volatile int64_t queued_grass_counter;

		static volatile int64_t queued_grass_mode;

		static int cur_state;

		static inline bool IsApplying = false;

		static std::unique_ptr<GidFileGenerationTask> cur_instance;

	private:
		bool IsResuming;

		std::unordered_set<std::string, case_insensitive_unordered_set::hash> ProgressDone;

	public:
		static const std::string KeyWS;

		static const std::string KeyCell;

		static volatile int64_t _lastDidSomething;

		static void InstallHooks()
		{
			Hooks::Install();
		}

	private:
		void Init();

		static void Free();

	public:
		[[nodiscard]] bool HasDone(const std::string& key, const std::string& wsName, int x = INT_MIN, int y = INT_MIN) const;

		[[nodiscard]] std::string GenerateProgressKey(const std::string& key, const std::string& wsName, int x = INT_MIN, int y = INT_MIN) const;

		void WriteProgressFile(const std::string& key, const std::string& wsName, int x = INT_MIN, int y = INT_MIN);

	private:
		std::vector<std::unique_ptr<GidFileWorldGenerateTask>> WorldTodo;

		void Begin();

		static void End();

		static inline int _istate = 0;

	public:
		static void Update();

	private:
		bool RunOne();

	protected:
		struct Hooks
		{
			struct MainUpdate_Nullsub
			{
				static void thunk(RE::Main* a_this, float a2)
				{
					Update();
					func(a_this, a2);
				}
				static inline REL::Relocation<decltype(thunk)> func;
			};

			struct GrassCountIncrement
			{
				static void thunk(RE::TESObjectCELL* cell)
				{
					InterlockedIncrement64(&queued_grass_counter);
					func(cell);
				}
				static inline REL::Relocation<decltype(thunk)> func;
			};

			struct WriteProgress
			{
				static void thunk(uintptr_t GrassMgr, RE::TESObjectCELL* cell, uintptr_t unk)
				{
					func(GrassMgr, cell, unk);
					if (IsApplying) {
						if (cell != nullptr) {
							auto ws = cell->worldSpace;
							if (ws != nullptr) {
								std::string wsn = ws->editorID.c_str();
								int x = cell->GetCoordinates()->cellX;
								int y = cell->GetCoordinates()->cellY;

								cur_instance->WriteProgressFile(KeyCell, wsn, x, y);
							}
						}
						InterlockedDecrement64(&queued_grass_counter);
					}
				}
				static inline REL::Relocation<decltype(thunk)> func;
			};

			static void Install()
			{
				if (Config::UseGrassCache) {
					stl::write_thunk_call<MainUpdate_Nullsub>(REL_ID(35551, 36544).address() + OFFSET(0x11F, 0x160));
					stl::write_thunk_call<GrassCountIncrement>(REL_ID(13190, 13335).address() + OFFSET(0xD40 - 0xC70, 0xD0));

					if (exists(std::filesystem::path(Util::getProgressFilePath()))) {
						stl::write_thunk_jump<WriteProgress>(REL_ID(13138, 13278).address() + OFFSET(0xF, 0xF));
					}
				}
			}
		};
	};

	class GidFileWorldGenerateTask final
	{
	public:
		GidFileWorldGenerateTask(GidFileGenerationTask* parent, RE::TESWorldSpace* ws, const std::string& wsName);

		GidFileGenerationTask* Parent;

		RE::TESWorldSpace* WorldSpace;

		std::string Name;

	private:
		std::list<GidFileCellGenerateTask*> CellTodo{};

		std::vector<GidFileCellGenerateTask*> _grid{};

	public:
		int TotalCellDo = 0;

		int DidCellDo = 0;

	private:
		static void Init();

		void Free();

		void Begin();

		void End() const;

		int _istate = 0;

		[[nodiscard]] GidFileCellGenerateTask* GetGrid(int x, int y) const;

		int uhalf = 0;
		int ugrid = 0;

		[[nodiscard]] GidFileCellGenerateTask* FindBestTodo() const;

	public:
		bool RunOne();

		void Remove(GidFileCellGenerateTask*& t);
	};

	class GidFileCellGenerateTask final
	{
	public:
		GidFileCellGenerateTask(GidFileWorldGenerateTask* parent, int x, int y);

		GidFileWorldGenerateTask* Parent;

		int X;

		int Y;

	private:
		RE::TESObjectCELL* Cell{};

		static void Init();

		static void Free();

		bool Begin();

		void End();

		bool Process();

	public:
		bool RunOne();
	};
}
