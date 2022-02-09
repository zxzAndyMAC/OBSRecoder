#ifndef _ACE_MONITOR_OOBS_HPP_
#define _ACE_MONITOR_OOBS_HPP_

#include "iace-monitor.h"
#include <unordered_map>
#include <mutex>

namespace ACE_MOT {

	typedef std::unordered_map<std::string, cd_cb> cd_mont;
	typedef std::unordered_map<std::string, void*> cd_map;

	class AceCallData : public IAceCallData
	{
	public:
		AceCallData();
		~AceCallData();

		void set(const char* name, void* ptr) override;
		void set(const char* name, void** ptr) override;

		void* get(const char* name) override;

	private:
		cd_map _map;
	};

	class AceMonitor : public IAceMonitor
	{
	public:
		AceMonitor();
		~AceMonitor();

		static AceMonitor* getInstance();

		static void release();

		void set_cb(const char* name, cd_cb cb_data) override;

		cd_cb get_cb(const char* name);

		bool call(const char* name, IAceCallData* data) override;

	private:
		cd_mont _mont;
	};
}

#endif
