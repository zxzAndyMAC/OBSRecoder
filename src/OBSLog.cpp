
#include <windows.h>
#include "OBSLog.h"
#include <string>
#include <sstream>
#include <time.h>
#include <mutex>

namespace OBS {
	OBSLog::OBSLog() :_unfiltered_log(false)
	{
	}

	OBSLog::~OBSLog()
	{
		base_set_log_handler(NULL, NULL);
	}


	bool OBSLog::get_token(lexer *lex, string &str, base_token_type type)
	{
		base_token token;
		if (!lexer_getbasetoken(lex, &token, IGNORE_WHITESPACE))
			return false;
		if (token.type != type)
			return false;

		str.assign(token.text.array, token.text.len);
		return true;
	}

	static bool expect_token(lexer *lex, const char *str, base_token_type type)
	{
		base_token token;
		if (!lexer_getbasetoken(lex, &token, IGNORE_WHITESPACE))
			return false;
		if (token.type != type)
			return false;

		return strref_cmp(&token.text, str) == 0;
	}

	uint64_t OBSLog::convert_log_name(bool has_prefix, const char *name)
	{
		BaseLexer  lex;
		string     year, month, day, hour, minute, second;

		lexer_start(lex, name);

		if (has_prefix) {
			string temp;
			if (!get_token(lex, temp, BASETOKEN_ALPHA)) return 0;
		}

		if (!get_token(lex, year, BASETOKEN_DIGIT)) return 0;
		if (!expect_token(lex, "-", BASETOKEN_OTHER)) return 0;
		if (!get_token(lex, month, BASETOKEN_DIGIT)) return 0;
		if (!expect_token(lex, "-", BASETOKEN_OTHER)) return 0;
		if (!get_token(lex, day, BASETOKEN_DIGIT)) return 0;
		if (!get_token(lex, hour, BASETOKEN_DIGIT)) return 0;
		if (!expect_token(lex, "-", BASETOKEN_OTHER)) return 0;
		if (!get_token(lex, minute, BASETOKEN_DIGIT)) return 0;
		if (!expect_token(lex, "-", BASETOKEN_OTHER)) return 0;
		if (!get_token(lex, second, BASETOKEN_DIGIT)) return 0;

		stringstream timestring;
		timestring << year << month << day << hour << minute << second;
		return std::stoull(timestring.str());
	}

	void OBSLog::get_last_log(bool has_prefix, const char *subdir_to_use, std::string &last)
	{
#if OBS_USE_EXEDIR == 1
		const char* logDir = subdir_to_use;
#else
		BPtr<char>       logDir(GetConfigPathPtr(subdir_to_use));
#endif
		struct os_dirent *entry;
		os_dir_t         *dir = os_opendir(logDir);
		uint64_t         highest_ts = 0;

		if (dir) {
			while ((entry = os_readdir(dir)) != NULL) {
				if (entry->directory || *entry->d_name == '.')
					continue;

				uint64_t ts = convert_log_name(has_prefix,
					entry->d_name);

				if (ts > highest_ts) {
					last = entry->d_name;
					highest_ts = ts;
				}
			}

			os_closedir(dir);
		}
	}


	string OBSLog::generateTimeDateFilename(const char *extension, bool noSpace)
	{
		time_t    now = time(0);
		char      file[256] = {};
		struct tm *cur_time;

		cur_time = localtime(&now);
		snprintf(file, sizeof(file), "%d-%02d-%02d%c%02d-%02d-%02d.%s",
			cur_time->tm_year + 1900,
			cur_time->tm_mon + 1,
			cur_time->tm_mday,
			noSpace ? '_' : ' ',
			cur_time->tm_hour,
			cur_time->tm_min,
			cur_time->tm_sec,
			extension);

		return string(file);
	}


	void OBSLog::delete_oldest_file(bool has_prefix, const char *location)
	{
#if OBS_USE_EXEDIR == 1
		const char* logDir = location;
#else
		BPtr<char>       logDir(GetConfigPathPtr(location));
#endif
		string           oldestLog;
		uint64_t         oldest_ts = (uint64_t)-1;
		struct os_dirent *entry;

		unsigned int maxLogs = 10;

		os_dir_t *dir = os_opendir(logDir);
		if (dir) {
			unsigned int count = 0;

			while ((entry = os_readdir(dir)) != NULL) {
				if (entry->directory || *entry->d_name == '.')
					continue;

				uint64_t ts = convert_log_name(has_prefix,
					entry->d_name);

				if (ts) {
					if (ts < oldest_ts) {
						oldestLog = entry->d_name;
						oldest_ts = ts;
					}

					count++;
				}
			}

			os_closedir(dir);

			if (count > maxLogs) {
				stringstream delPath;

				delPath << logDir << "/" << oldestLog;
				os_unlink(delPath.str().c_str());
			}
		}
	}

	void OBSLog::do_log(int log_level, const char *msg, va_list args, void *param)
	{
		OBSLog* obslog = static_cast<OBSLog*>(param);

		char str[4096];
		vsnprintf(str, 4095, msg, args);

		OutputDebugStringA(str);
		OutputDebugStringA("\n");

		if (log_level == LOG_WARNING)
		{
			printf("WARNING: %s\n", str);
		}
		else if (log_level == LOG_ERROR)
		{
			printf("ERROR: %s\n", str);
		}
		else if (log_level == LOG_INFO)
		{
			printf("INFO: %s\n", str);
		}
		else if (log_level == LOG_DEBUG)
		{
			printf("DEBUG: %s\n", str);
		}

		//if (log_level < LOG_WARNING)
		//	__debugbreak();

		if (IsDebuggerPresent()) {
			int wNum = MultiByteToWideChar(CP_UTF8, 0, str, -1, NULL, 0);
			if (wNum > 1) {
				static wstring wide_buf;
				static mutex wide_mutex;

				lock_guard<mutex> lock(wide_mutex);
				wide_buf.reserve(wNum + 1);
				wide_buf.resize(wNum - 1);
				MultiByteToWideChar(CP_UTF8, 0, str, -1, &wide_buf[0],
					wNum);
				wide_buf.push_back('\n');

				OutputDebugStringW(wide_buf.c_str());
			}
		}

		bool log_verbose = true;
		if (log_level <= LOG_INFO || log_verbose) {
			if (obslog->too_many_repeated_entries(obslog->_logFile, msg, str))
				return;
			obslog->LogStringChunk(obslog->_logFile, str);
		}

#if defined(OBS_DEBUGBREAK_ON_ERROR)
		if (log_level <= LOG_ERROR && IsDebuggerPresent())
			__debugbreak();
#endif
	}


	string OBSLog::currentTimeString()
	{
		using namespace std::chrono;

		struct tm  tstruct;
		char       buf[80];

		auto tp = system_clock::now();
		auto now = system_clock::to_time_t(tp);
		tstruct = *localtime(&now);

		size_t written = strftime(buf, sizeof(buf), "%X", &tstruct);
		if (ratio_less<system_clock::period, seconds::period>::value &&
			written && (sizeof(buf) - written) > 5) {
			auto tp_secs =
				time_point_cast<seconds>(tp);
			auto millis =
				duration_cast<milliseconds>(tp - tp_secs).count();

			snprintf(buf + written, sizeof(buf) - written, ".%03u",
				static_cast<unsigned>(millis));
		}

		return buf;
	}

	int OBSLog::sum_chars(const char *str)
	{
		int val = 0;
		for (; *str != 0; str++)
			val += *str;

		return val;
	}

	bool OBSLog::too_many_repeated_entries(fstream &logFile, const char *msg, const char *output_str)
	{
		static mutex log_mutex;
		static const char *last_msg_ptr = nullptr;
		static int last_char_sum = 0;
		static char cmp_str[4096];
		static int rep_count = 0;

		int new_sum = sum_chars(output_str);

		lock_guard<mutex> guard(log_mutex);

		if (_unfiltered_log) {
			return false;
		}

		if (last_msg_ptr == msg) {
			int diff = std::abs(new_sum - last_char_sum);
			if (diff < MAX_CHAR_VARIATION) {
				return (rep_count++ >= MAX_REPEATED_LINES);
			}
		}

		if (rep_count > MAX_REPEATED_LINES) {
			logFile << currentTimeString() <<
				": Last log entry repeated for " <<
				to_string(rep_count - MAX_REPEATED_LINES) <<
				" more lines" << endl;
		}

		last_msg_ptr = msg;
		strcpy(cmp_str, output_str);
		last_char_sum = new_sum;
		rep_count = 0;

		return false;
	}

	void OBSLog::LogString(fstream &logFile, const char *timeString, char *str)
	{
		logFile << timeString << str << endl;
	}

	void OBSLog::LogStringChunk(fstream &logFile, char *str)
	{
		char *nextLine = str;
		string timeString = currentTimeString();
		timeString += ": ";

		while (*nextLine) {
			char *nextLine = strchr(str, '\n');
			if (!nextLine)
				break;

			if (nextLine != str && nextLine[-1] == '\r') {
				nextLine[-1] = 0;
			}
			else {
				nextLine[0] = 0;
			}

			LogString(logFile, timeString.c_str(), str);
			nextLine++;
			str = nextLine;
		}

		LogString(logFile, timeString.c_str(), str);
	}

	void OBSLog::create_log_file()
	{
#ifdef OBS_OPEN_LOG
		os_mkdirs(OBS_LOGS_DIR);
		os_mkdirs(OBS_LOG("/logs"));
		os_mkdirs(OBS_CRASH("/crashes"));

		stringstream dst;

		get_last_log(false, OBS_LOG("/logs"), _lastLogFile);
		get_last_log(true, OBS_CRASH("/crashes"), _lastCrashLogFile);


		_currentLogFile = generateTimeDateFilename("txt", false);
		dst << OBS_LOG("/logs/") << _currentLogFile.c_str();

#if OBS_USE_EXEDIR == 1
		string ss(dst.str());
		//ss.append(dst.str());
		const char* path = ss.c_str();
		//const char* path = dst.str().c_str();
#else
		BPtr<char> path(GetConfigPathPtr(dst.str().c_str()));
#endif

		BPtr<wchar_t> wpath;
		os_utf8_to_wcs_ptr(path, 0, &wpath);
		_logFile.open(wpath,
			ios_base::in | ios_base::out | ios_base::trunc);

		if (_logFile.is_open()) {
			delete_oldest_file(false, OBS_LOG("/logs"));
			base_set_log_handler(do_log, this);
		}
		else {
			blog(LOG_ERROR, "Failed to open log file");
		}
#endif
	}
}