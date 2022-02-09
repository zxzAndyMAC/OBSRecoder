#ifndef _OBS_COLOR_SOURCE_H
#define _OBS_COLOR_SOURCE_H

#include "OBSCommon.h"
#include "IOBSStudio.h"
#include "OBSBaseSource.h"
#include <string>

using namespace std;

namespace OBS {

	class ColorSource : public BaseSource
	{
	public:
		ColorSource();
		~ColorSource();

		void setBackgroundColor(int color /*0xFF00FF00*/);

		void setSize(uint32_t width, uint32_t height);
	};

}

#endif
