#include "GrassControl/Util.h"

namespace Util
{
	std::string Util::getProgressFilePath()
	{
		std::string n = _ovFilePath;
		if (!n.empty()) {
			return n;
		}

		// Dumb user mode for .txt.txt file name.
		try {
			auto fi = std::filesystem::path(_progressFilePath);
			if (exists(fi)) {
				_ovFilePath = _progressFilePath;
			} else {
				std::string fpath = _progressFilePath + R"(.txt)";
				fi = std::filesystem::path(fpath);
				if (exists(fi)) {
					_ovFilePath = fpath;
				}
			}
		} catch (...) {
		}

		if (_ovFilePath.empty()) {
			_ovFilePath = _progressFilePath;
		}

		return _ovFilePath;
	}

	CachedFormList::CachedFormList() = default;

	CachedFormList* CachedFormList::TryParse(const std::string& input, std::string pluginForLog, std::string settingNameForLog, bool warnOnMissingForm, bool dontWriteAnythingToLog)
	{
		if (settingNameForLog.empty()) {
			settingNameForLog = "unknown form list setting";
		}
		if (pluginForLog.empty()) {
			pluginForLog = "unknown plugin";
		}

		auto ls = new CachedFormList();
		char Char = ';';
		auto spl = StringHelpers::split(input, Char, true);
		for (auto& x : spl) {
			std::string idstr;
			std::string fileName;

			int ix = x.find(L':');
			if (ix <= 0) {
				if (!dontWriteAnythingToLog) {
					logger::warn(fmt::runtime("Failed to parse " + settingNameForLog + " for " + pluginForLog + "! Invalid input: `" + x + "`."));
				}

				delete ls;
				return nullptr;
			}

			idstr = x.substr(0, ix);
			fileName = x.substr(ix + 1);

			if (!std::ranges::all_of(idstr.begin(), idstr.end(), [](wchar_t q) { return (q >= L'0' && q <= L'9') || (q >= L'a' && q <= L'f') || (q >= L'A' && q <= L'F'); })) {
				if (!dontWriteAnythingToLog) {
					logger::warn(fmt::runtime("Failed to parse " + settingNameForLog + " for " + pluginForLog + "! Invalid form ID: `" + idstr + "`."));
				}

				delete ls;
				return nullptr;
			}

			if (fileName.empty()) {
				if (!dontWriteAnythingToLog) {
					logger::warn(fmt::runtime("Failed to parse " + settingNameForLog + " for " + pluginForLog + "! Missing file name."));
				}

				delete ls;
				return nullptr;
			}

			RE::FormID id = 0;
			bool sucess;
			try {
				id = stoi(idstr, nullptr, 16);
				sucess = true;
			} catch (std::exception& err) {
				sucess = false;
			}
			if (!sucess) {
				if (!dontWriteAnythingToLog) {
					logger::warn(fmt::runtime("Failed to parse " + settingNameForLog + " for " + pluginForLog + "! Invalid form ID: `" + idstr + "`."));
				}

				delete ls;
				return nullptr;
			}

			id &= 0x00FFFFFF;
			if (auto file = RE::TESDataHandler::GetSingleton()->LookupLoadedModByName(fileName); file) {
				if (!file->IsFormInMod(id)) {
					if (!dontWriteAnythingToLog && warnOnMissingForm) {
						logger::warn(fmt::runtime("Failed to find form " + settingNameForLog + " for " + pluginForLog + "! Form ID was " + std::to_string(id) + " and file was " + fileName + "."));
					}
					continue;
				}
			}
			auto form = RE::TESForm::LookupByID(id);
			if (ls->Ids.insert(id).second) {
				ls->Forms.push_back(form);
			}
		}

		return ls;
	}

	bool CachedFormList::Contains(RE::TESForm* form)
	{
		if (form == nullptr)
			return false;

		return Contains(form->formID);
	}

	bool CachedFormList::Contains(unsigned int formId)
	{
		return std::ranges::find(this->Ids.begin(), this->Ids.end(), formId) != this->Ids.end();
	}

	std::vector<RE::TESForm*> CachedFormList::getAll() const
	{
		return this->Forms;
	}
}
