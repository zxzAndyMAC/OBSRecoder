
#include "OBSCommon.h"
#include <map>

namespace OBS {

	const std::map<int, const char*> &GetAACEncoderBitrateMap();
	const char *GetAACEncoderForBitrate(int bitrate);
	int FindClosestAvailableAACBitrate(int bitrate);

}
