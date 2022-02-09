
#include "OBSColorSource.h"

namespace OBS {

	ColorSource::ColorSource()
		:BaseSource(COLOR_SOURCE_CHANNEL, COLOR_SOURCE)
	{

	}

	ColorSource::~ColorSource()
	{

	}

	void ColorSource::setBackgroundColor(int color /*0xFF00FF00*/)
	{
		obs_data_t *settings = obs_source_get_settings(_source);

		obs_data_set_int(settings, "color", color);

		obs_source_update(_source, settings);

		obs_data_release(settings);
	}

	void ColorSource::setSize(uint32_t width, uint32_t height)
	{
		obs_data_t *settings = obs_source_get_settings(_source);

		obs_data_set_int(settings, "width", width);
		obs_data_set_int(settings, "height", height);

		obs_source_update(_source, settings);

		obs_data_release(settings);
	}

}