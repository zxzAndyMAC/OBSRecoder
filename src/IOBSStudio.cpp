#include "IOBSStudio.h"
#include "OBSMain.h"

namespace OBS {

	static OBSMain* s_obs = nullptr;

	IOBS* OBSStudio::createInstance()
	{
		if (s_obs == nullptr)
		{
			s_obs = new OBSMain();
		}
		return s_obs;
	}

	IOBS* OBSStudio::getInstance()
	{
		return s_obs;
	}

	void  OBSStudio::release()
	{
		if (s_obs != nullptr)
		{
			delete s_obs;
			s_obs = nullptr;
		}
	}
}