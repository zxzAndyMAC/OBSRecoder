#ifndef _ACE_MONITOR_OOBS_H_
#define _ACE_MONITOR_OOBS_H_

#include <string>
#include <functional>

#if defined(IACE_MONT_EXPORTS)
#	define IACE_MONT __declspec(dllexport)
#else
#	define IACE_MONT __declspec(dllimport)
#endif

namespace ACE_MOT
{
	class IACE_MONT IAceCallData {
	public:
		virtual void set(const char* name, void* ptr) = 0;
		virtual void set(const char* name, void** ptr) = 0;
		virtual void* get(const char* name) = 0;

	};

	typedef std::function<void(IAceCallData*)> cd_cb;

	class IACE_MONT IAceMonitor {
	public:
		virtual void set_cb(const char* name, cd_cb cb_data) = 0;
		virtual bool call(const char* name, IAceCallData* data) = 0;
	};

	IACE_MONT IAceMonitor* getAceMonitor();
	IACE_MONT void releaseAceMonitor();

	IACE_MONT IAceCallData* createIAC();
	IACE_MONT void	releaseIAC(IAceCallData* ac);
}

#endif
