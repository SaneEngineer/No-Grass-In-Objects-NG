#pragma once

#include <algorithm>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

#include <GrassControl/Stopwatch.hpp>
#include <GrassControl/Util.h>
#include <GrassControl/logging.h>

namespace GrassControl
{
	class Profiler
	{
	public:
		virtual ~Profiler()
		{
			delete Timer;
		}

		Profiler();

	private:
		std::vector<long long> Times = std::vector<long long>();

		std::vector<std::pair<int, long long>> Progress = std::vector<std::pair<int, long long>>();

		std::mutex Locker;

		stopwatch::Stopwatch* const Timer;

		long long Divide;

	public:
		void Begin();

		void End();

		void Report();
	};
}
