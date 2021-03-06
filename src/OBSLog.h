#ifndef _OBS_LOG_H_
#define _OBS_LOG_H_

#include "OBSCommon.h"
#include <fstream>

using namespace std;

namespace OBS {

	class OBSLog
	{

	public:
		OBSLog();
		~OBSLog();

		void		create_log_file();

	private:
		void		get_last_log(bool has_prefix, const char *subdir_to_use, std::string &last);
		string		generateTimeDateFilename(const char *extension, bool noSpace);
		uint64_t	convert_log_name(bool has_prefix, const char *name);
		bool		get_token(lexer *lex, string &str, base_token_type type);
		void		delete_oldest_file(bool has_prefix, const char *location);

		static void	do_log(int log_level, const char *msg, va_list args, void *param);

		void		LogStringChunk(fstream &logFile, char *str);
		void		LogString(fstream &logFile, const char *timeString, char *str);
		bool		too_many_repeated_entries(fstream &logFile, const char *msg, const char *output_str);
		int			sum_chars(const char *str);
		string		currentTimeString();
	private:
		fstream		_logFile;
		string		_currentLogFile;
		string		_lastLogFile;
		string		_lastCrashLogFile;
		bool		_unfiltered_log;
	};
}
#endif
