#include "GrassControl/Profiler.h"

namespace GrassControl
{
	Profiler::Profiler() :
		Timer(new stopwatch::Stopwatch()), Divide(1)
	{
		this->Timer->start();
	}

	void Profiler::Begin()
	{
		int who = GetCurrentThreadId();
		long long when = this->Timer->elapsed<stopwatch::ms>();

		{
			std::scoped_lock lock(this->Locker);
			for (int i = 0; i < this->Progress.size(); i++) {
				auto& [fst, snd] = this->Progress[i];
				if (fst == who) {
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
		long long when = this->Timer->elapsed<stopwatch::ms>();
		int who = GetCurrentThreadId();

		{
			std::scoped_lock lock(this->Locker);
			for (int i = 0; i < this->Progress.size(); i++) {
				auto& [fst, snd] = this->Progress[i];
				if (fst == who) {
					long long diff = when - snd;
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
				w /= this->Divide;
				all.push_back(w);
			}
		}

		std::string message;
		if (all.empty()) {
			message = "No samples have been gathered yet.";
		} else {
			if (all.size() > 1) {
				std::ranges::sort(all);
			}

			double sum = std::accumulate(all.begin(), all.end(), 0.0);
			double avg = sum / all.size();
			double med = all[all.size() / 2];
			double min = all[0];
			double max = all[all.size() - 1];

			std::vector<std::string> things;
			auto timeTemplate = fmt::runtime("{:.3}ms");
			things.emplace_back("Samples = " + std::to_string(all.size()));
			things.emplace_back("Average = " + format(timeTemplate, avg));
			things.emplace_back("Median = " + format(timeTemplate, med));
			things.emplace_back("Min = " + format(timeTemplate, min));
			things.emplace_back("Max = " + format(timeTemplate, max));

			message = Util::StringHelpers::Join(things, "; ");
		}
		RE::DebugNotification(message.c_str(), nullptr, true);
		//logger::debug(fmt::runtime(message));
	}
}
