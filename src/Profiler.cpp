#include "GrassControl/Profiler.h"

namespace GrassControl
{
	struct KahanAccumulation
	{
		double sum;
		double correction;
	};

	KahanAccumulation KahanSum(KahanAccumulation accumulation, double value)
	{
		KahanAccumulation result;
		double y = value - accumulation.correction;
		double t = accumulation.sum + y;
		result.correction = (t - accumulation.sum) - y;
		result.sum = t;
		return result;
	}

	Profiler::Profiler() :
		Timer(new stopwatch::Stopwatch()), Divide(0)
	{
		this->Timer->start();
	}

	void Profiler::Begin()
	{
		int who = GetCurrentThreadId();
		long long when = this->Timer->elapsed<stopwatch::ct>();

		{
			std::lock_guard lock(this->Locker);
			for (int i = 0; i < this->Progress.size(); i++) {
				auto t = this->Progress[i];
				if (t.first == who) {
					{
						this->Progress[i] = std::pair{ who, when };
					}
					return;
				}
			}

			this->Progress.emplace_back(std::pair{ who, when });
		}
	}

	void Profiler::End()
	{
		long long when = this->Timer->elapsed<stopwatch::ct>();
		int who = GetCurrentThreadId();

		{
			std::lock_guard lock(this->Locker);
			for (int i = 0; i < this->Progress.size(); i++) {
				auto t = this->Progress[i];
				if (t.first == who) {
					long long diff = when - t.second;
					this->Progress.erase(this->Progress.begin() + i);

					this->Times.push_back(diff);
					if (this->Times.size() > 1000) {
						this->Times.erase(this->Times.begin());
					}
					return;
				}
			}
		}
	}

	void Profiler::Report()
	{
		std::vector<double> all;

		{
			std::lock_guard lock(this->Locker);
			for (auto diff : this->Times) {
				auto w = static_cast<double>(diff);
				w /= static_cast<double>(this->Divide);
				all.push_back(w);
			}
		}

		std::string message;
		if (all.empty()) {
			message = "No samples have been gathered yet.";
		} else {
			if (all.size() > 1) {
				std::sort(all.begin(), all.end());
			}

			KahanAccumulation init = { 0 };
			KahanAccumulation result =
				std::accumulate(all.begin(), all.end(), init, KahanSum);

			double sum = result.sum;
			double avg = sum / static_cast<double>(all.size());
			double med = all[all.size() / 2];
			double min = all[0];
			double max = all[all.size() - 1];

			std::function<std::string(double)> fmt = [&](double v) {
				return std::to_string(v);
			};

			std::vector<std::string> things;

			things.emplace_back("Samples = " + std::to_string(all.size()));
			things.emplace_back("Average = " + fmt(avg));
			things.emplace_back("Median = " + fmt(med));
			things.emplace_back("Min = " + fmt(min));
			things.emplace_back("Max = " + fmt(max));

			message = Util::StringHelpers::Join(things, "; ");
		}
		RE::DebugNotification(message.c_str(), nullptr, true);
		//logger::debug(fmt::runtime(message));
	}
}
