#pragma once


namespace Util
{
	
	// Function object for case insensitive comparison
	struct case_insensitive_compare
	{
		case_insensitive_compare() {}

		// Function objects overloader operator()
		// When used as a comparer, it should function as operator<(a,b)
		bool operator()(const std::wstring& a, const std::wstring& b) const
		{
			return to_lower(a) < to_lower(b);
		}

		std::wstring to_lower(const std::wstring& a) const
		{
			std::wstring s(a);
			for (auto& c : s)
			{
				c = tolower(c);
			}
		    //	transform(a.begin(), a.end(), a.begin(), ::tolower);
			return s;
		}

		void char_to_lower(char& c) const
		{
			if (c >= 'A' && c <= 'Z')
				c += ('a' - 'A');
		}
	};
	struct StringHelpers
	{
		template <typename Range, typename Value = typename Range::value_type>
		static std::string Join(Range const& elements, const char* const delimiter)
		{
			std::ostringstream os;
			auto b = begin(elements), e = end(elements);

			if (b != e) {
				std::copy(b, prev(e), std::ostream_iterator<Value>(os, delimiter));
				b = prev(e);
			}
			if (b != e) {
				os << *b;
			}

			return os.str();
		}

		/*! note: imput is assumed to not contain NUL characters
 */
		template <typename Input, typename Output, typename Value = typename Output::value_type>
		static void Split(char delimiter, Output& output, Input const& input)
		{
			using namespace std;
			for (auto cur = begin(input), beg = cur;; ++cur) {
				if (cur == end(input) || *cur == delimiter || !*cur) {
					output.insert(output.end(), Value(beg, cur));
					if (cur == end(input) || !*cur)
						break;
					else
						beg = next(cur);
				}
			}
		}

		static std::vector<std::string> split(const std::string& str, const std::string& regex_str, bool RemoveEmpty)
		{
			std::regex regexz(regex_str);
			std::vector<std::string> list(std::sregex_token_iterator(str.begin(), str.end(), regexz, -1),
				std::sregex_token_iterator());
			if (RemoveEmpty) {
				for (int i = 0; i < list.size();) {
					if (list[i].size() == 0) {
						list.erase(list.begin() + i);
					} else
						++i;
				}
			}
			return list;
		}

		static std::vector<std::string> Split_at_any(const std::string& str, const std::vector<char> delims, bool RemoveEmpty)
		{
			const std::string fixedDelim = ",";
			std::vector<std::string> split;
			size_t t = 0;

			// Make a copy of the string
			std::string tmp = str;

			// First, replace all delimeters with a common delimeter
			for (const auto& delim : delims) {
				t = 0;
				while ((t = tmp.find(delim, t)) != std::string::npos) {
					tmp = tmp.replace(t, delim, fixedDelim);
					t += delim;
				}
			}
			t = 0;
			int start = 0;
			// Extract all delimited values
			while ((t = tmp.find(fixedDelim, t)) != std::string::npos) {
				split.emplace_back(tmp.substr(start, t - start));
				start = t + 1;
				t += fixedDelim.size() + 1;
			}
			if (RemoveEmpty) {
				for (int i = 0; i < split.size();) {
					if (split[i].size() == 0) {
						split.erase(split.begin() + i);
					} else
						++i;
				}
			}

			return split;
		}

		static std::string trim(std::string_view s)
		{
			s.remove_prefix(std::min(s.find_first_not_of(" \t\r\v\n"), s.size()));
			s.remove_suffix(std::min(s.size() - s.find_last_not_of(" \t\r\v\n") - 1, s.size()));

			return std::string{ s };
		}
	};
}
