
#include "ace-monitor.h"


namespace ACE_MOT {
	static AceMonitor* s_ace_monitor = nullptr;
	static std::mutex	_dataMutex;
	AceCallData::AceCallData() {}
	AceCallData::~AceCallData() { _map.clear(); }

	void AceCallData::set(const char* name, void* ptr)
	{
		//_map.insert(std::pair<std::string, void*>(name, ptr));
		_map[name] = ptr;
	}

	void AceCallData::set(const char* name, void** ptr)
	{
		_map[name] = ptr;
	}

	void* AceCallData::get(const char* name)
	{
		std::unordered_map<std::string, void*>::iterator got = _map.find(name);
		if (got == _map.end())
			return nullptr;
		return got->second;
	}

	
	AceMonitor::AceMonitor() { }
	AceMonitor::~AceMonitor() { _mont.clear(); }

	AceMonitor* AceMonitor::getInstance()
	{
		if (nullptr == s_ace_monitor)
		{
			s_ace_monitor = new AceMonitor();
		}

		return s_ace_monitor;
	}

	void AceMonitor::release()
	{
		if (nullptr != s_ace_monitor)
		{
			delete s_ace_monitor;
			s_ace_monitor = nullptr;
		}
	}

	void AceMonitor::set_cb(const char* name, cd_cb cb_data)
	{
		//_mont.insert(std::pair<std::string, cd_cb>(name, cb_data));
		std::unique_lock<std::mutex> lk(_dataMutex, std::defer_lock);
		lk.try_lock();
		_mont[name] = cb_data;
	}

	cd_cb AceMonitor::get_cb(const char* name)
	{
		std::unique_lock<std::mutex> lk(_dataMutex, std::defer_lock);
		lk.try_lock();
		std::unordered_map<std::string, cd_cb>::iterator got = _mont.find(name);
		if (got == _mont.end())
			return nullptr;
		return got->second;
	}

	bool AceMonitor::call(const char* name, IAceCallData* data)
	{
		std::unique_lock<std::mutex> lk(_dataMutex, std::defer_lock);
		lk.try_lock();
		cd_cb cb = get_cb(name);

		if (cb == nullptr)
		{
			return false;
		}

		cb(data);

		return true;
	}

	IAceMonitor* getAceMonitor()
	{
		return AceMonitor::getInstance();
	}

	void releaseAceMonitor()
	{
		AceMonitor::release();
	}

	IAceCallData* createIAC()
	{
		return new AceCallData();
	}

	void releaseIAC(IAceCallData* ac)
	{
		delete (AceCallData*)ac;
	}

}
