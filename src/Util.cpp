#include "GrassControl/Util.h"

namespace Util
{
	void report_and_fail_timed(const std::string& a_message)
	{
		logger::error(fmt::runtime(a_message));
		MessageBoxTimeoutA(nullptr, a_message.c_str(), a_message.c_str(), MB_SYSTEMMODAL, 0, 7000);
		TerminateProcess(GetCurrentProcess(), 1);
	}

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

	CachedFormList* CachedFormList::TryParse(const std::string& input, std::string settingNameForLog, bool warnOnMissingForm, bool dontWriteAnythingToLog)
	{
		if (settingNameForLog.empty()) {
			settingNameForLog = "unknown form list setting";
		}

		auto ls = new CachedFormList();
		char Char = ';';
		auto spl = StringHelpers::split(input, Char, true);
		for (auto& x : spl) {
			std::string idstr;
			std::string fileName;

			auto ix = x.find(L':');
			if (ix <= 0) {
				if (!dontWriteAnythingToLog) {
					logger::warn(fmt::runtime("Failed to parse form for " + settingNameForLog + "! Invalid input: `" + x + "`."));
				}

				delete ls;
				return nullptr;
			}

			idstr = x.substr(0, ix);
			fileName = x.substr(ix + 1);

			if (!std::ranges::all_of(idstr.begin(), idstr.end(), [](wchar_t q) { return (q >= L'0' && q <= L'9') || (q >= L'a' && q <= L'f') || (q >= L'A' && q <= L'F'); })) {
				if (!dontWriteAnythingToLog) {
					logger::warn(fmt::runtime("Failed to parse form for " + settingNameForLog + "! Invalid form ID: `" + idstr + "`."));
				}

				delete ls;
				return nullptr;
			}

			if (fileName.empty()) {
				if (!dontWriteAnythingToLog) {
					logger::warn(fmt::runtime("Failed to parse form for " + settingNameForLog + "! Missing file name."));
				}

				delete ls;
				return nullptr;
			}

			RE::FormID id = 0;
			bool sucess;
			try {
				id = stoi(idstr, nullptr, 16);
				sucess = true;
			} catch (std::exception&) {
				sucess = false;
			}
			if (!sucess) {
				if (!dontWriteAnythingToLog) {
					logger::warn(fmt::runtime("Failed to parse form for " + settingNameForLog + "! Invalid form ID: `" + idstr + "`."));
				}

				delete ls;
				return nullptr;
			}

			id &= 0x00FFFFFF;
			if (auto file = RE::TESDataHandler::GetSingleton()->LookupLoadedModByName(fileName); file) {
				if (!RE::TESDataHandler::GetSingleton()->LookupFormID(id, fileName)) {
					if (!dontWriteAnythingToLog && warnOnMissingForm) {
						logger::warn(fmt::runtime("Failed to find form for " + settingNameForLog + "! Form ID was 0x{:x} and file was " + fileName + "."), id);
					}
					continue;
				}
			}
			auto form = RE::TESDataHandler::GetSingleton()->LookupForm(id, fileName);
			auto formID = RE::TESDataHandler::GetSingleton()->LookupFormID(id, fileName);
			if (!form || !formID) {
				if (!dontWriteAnythingToLog) {
					logger::warn(fmt::runtime("Invalid form detected while adding form to " + settingNameForLog + "! Possible invalid form ID: `0x{:x}`."), formID);
				}
				continue;
			}
			if (ls->Ids.insert(formID).second) {
				logger::info(fmt::runtime("Form 0x{:x} was successfully added to " + settingNameForLog), formID);
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
