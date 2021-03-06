
#include "OBSDshowSource.h"
#include "IOBSStudio.h"

namespace OBS {

	struct FPSFormat {
		const char *text;
		long long  interval;
	};

	static const FPSFormat validFPSFormats[] = {
		{ "60",         MAKE_DSHOW_FPS(60) },
		{ "59.94 NTSC", MAKE_DSHOW_FRACTIONAL_FPS(60000, 1001) },
		{ "50",         MAKE_DSHOW_FPS(50) },
		{ "48 film",    MAKE_DSHOW_FRACTIONAL_FPS(48000, 1001) },
		{ "40",         MAKE_DSHOW_FPS(40) },
		{ "30",         MAKE_DSHOW_FPS(30) },
		{ "29.97 NTSC", MAKE_DSHOW_FRACTIONAL_FPS(30000, 1001) },
		{ "25",         MAKE_DSHOW_FPS(25) },
		{ "24 film",    MAKE_DSHOW_FRACTIONAL_FPS(24000, 1001) },
		{ "20",         MAKE_DSHOW_FPS(20) },
		{ "15",         MAKE_DSHOW_FPS(15) },
		{ "10",         MAKE_DSHOW_FPS(10) },
		{ "5",          MAKE_DSHOW_FPS(5) },
		{ "4",          MAKE_DSHOW_FPS(4) },
		{ "3",          MAKE_DSHOW_FPS(3) },
		{ "2",          MAKE_DSHOW_FPS(2) },
		{ "1",          MAKE_DSHOW_FPS(1) },
	};

	DshowSource::DshowSource()
		:BaseSource(DSHOW_SOURCE_CHANNEL, DSHOW_SOURCE),
		_defaultresl(""),
		_chroma_key_filter(nullptr)
	{

	}

	DshowSource::~DshowSource()
	{
		if (_chroma_key_filter != nullptr)
		{
			obs_source_filter_remove(_source, _chroma_key_filter);
		}
	}

	void DshowSource::setScale(const float &x, const float &y)
	{
		/*放弃此方法*/
		{
			return;
		}
		if (nullptr == _source)
		{
			return;
		}

		float base_width = (float)obs_source_get_base_width(_source);
		float base_height = (float)obs_source_get_base_height(_source);

		if (base_width < 1.0f)
		{
			return;
		}

		struct vec2 scale;
		scale.x = x;
		scale.y = y;
		//obs_sceneitem_set_scale(_sceneitem, &scale);
	}

	void DshowSource::init_cams_info()
	{
		/*get properties*/
		//对应 摄像头 name,id
		_properties = obs_source_properties(_source);
		obs_property_t *property = obs_properties_first(_properties);
		bool hasNoProperties = !property;

		while (property) {
			const char        *name = obs_property_name(property);
			obs_property_type type = obs_property_get_type(property);

			if (strcmp(name, "video_device_id") == 0)
			{
				size_t  count = obs_property_list_item_count(property);

				for (size_t i = 0; i < count; i++)
				{
					const char * strId = obs_property_list_item_string(property, i);
					const char * strName = obs_property_list_item_name(property, i);
					_vecCam.push_back(std::make_pair(strName, strId));
				}
			}

			obs_property_next(&property);
		}
	}

	void DshowSource::set_cam_device(std::string device_id)
	{
		obs_data_t *obsData = obs_source_get_settings(_source);

		obs_data_set_string(obsData, "video_device_id", device_id.c_str());
		obs_source_update(_source, obsData);
		obs_data_release(obsData);
		get_cam_info(device_id);
	}

	long long DshowSource::getOBSFPS(int fps)
	{
		obs_video_info ovi;
		if (!obs_get_video_info(&ovi))
			return 0;

		return MAKE_DSHOW_FRACTIONAL_FPS(ovi.fps_num, ovi.fps_den);
	}

	const char* DshowSource::getFPSName(long long interval)
	{
		if (interval == FPS_MATCHING) {
			return "Match Output FPS";
		}

		if (interval == FPS_HIGHEST) {
			return "Highest FPS";
		}

		for (const FPSFormat &format : validFPSFormats) {
			if (format.interval != interval)
				continue;
			return format.text;
		}
		return to_string(10000000. / interval).c_str();
	}

	void DshowSource::get_cam_info(std::string device_id)
	{
		obs_property_t *property = obs_properties_first(_properties);
		
		/*设置摄像头*/
		blog(LOG_INFO, "==============set webcam=================");
		while (property) {

			const char        *name = obs_property_name(property);
			obs_property_type type = obs_property_get_type(property);

			if (strcmp(name, "video_device_id") == 0)
			{
				obs_data_t *settings = obs_source_get_settings(_source);
				obs_data_set_string(settings, "video_device_id", device_id.c_str());
				obs_property_modified(property, settings);
				obs_source_update(_source, settings);
				obs_data_release(settings);
				break;
			}

			obs_property_next(&property);
		}

		/*获取摄像头支持到分辨率*/
		blog(LOG_INFO, "==============get resolutions that webcam support=================");
		_resolutions.clear();
		_resolutions.swap(CameraResolutionLists());
		property = obs_properties_first(_properties);
		while (property) {
			const char        *name = obs_property_name(property);
			obs_property_type type = obs_property_get_type(property);

			if (strcmp(name, "resolution") == 0)
			{
				size_t  count = obs_property_list_item_count(property);

				for (size_t i = 0; i < count; i++)
				{
					const char * str = obs_property_list_item_string(property, i);
					_resolutions.push_back(std::make_pair(i, str));
				}
			}

			obs_property_next(&property);
		}

		_defaultresl = set_cam_resolution(0);
	}

	const char* DshowSource::set_cam_resolution(size_t index)
	{
		obs_property_t *property = obs_properties_first(_properties);
		const char* resl = NULL;
		while (property) {
			const char        *name = obs_property_name(property);
			obs_property_type type = obs_property_get_type(property);

			if (strcmp(name, "resolution") == 0)
			{
				blog(LOG_INFO, "==============set webcam resolution=================");
				obs_data_t *settings = obs_source_get_settings(_source);
				obs_data_set_int(settings, "res_type", 1);
				obs_data_set_string(settings, "resolution", _resolutions[index].second.c_str());
				resl = _resolutions[index].second.c_str();
				obs_property_modified(property, settings);
				obs_source_update(_source, settings);
				obs_data_release(settings);
				break;
			}

			obs_property_next(&property);
		}
		get_cam_fps();
		blog(LOG_INFO, "==============resolution: %s=================", resl);
		return resl;
	}

	void DshowSource::get_cam_fps()
	{
		_fps.clear();
		_fps.swap(CamerasFPS());

		obs_property_t *property = obs_properties_first(_properties);
		bool hassetfps = false;
		while (property) {
			const char        *name = obs_property_name(property);
			obs_property_type type = obs_property_get_type(property);

			if (strcmp(name, "frame_interval") == 0)
			{
				size_t  count = obs_property_list_item_count(property);
				string strFps;
				for (size_t i = 0; i < count; i++)
				{
					long long frame_interval = obs_property_list_item_int(property, i);
					const char* fname = getFPSName(frame_interval);
					_fps.push_back(std::make_pair(frame_interval, fname));
					if (!hassetfps)
					{
						hassetfps = true;
						obs_data_t *settings = obs_source_get_settings(_source);
						obs_data_set_int(settings, "frame_interval", frame_interval);
						obs_property_modified(property, settings);
						obs_source_update(_source, settings);
						obs_data_release(settings);
					}
				}
				break;
			}

			obs_property_next(&property);
		}
	}

	void DshowSource::set_fps(long long interval)
	{
		obs_property_t *property = obs_properties_first(_properties);
		while (property) {
			const char        *name = obs_property_name(property);
			obs_property_type type = obs_property_get_type(property);

			if (strcmp(name, "frame_interval") == 0)
			{
				blog(LOG_INFO, "==============set webcam fps=================");
				obs_data_t *settings = obs_source_get_settings(_source);
				obs_data_set_int(settings, "frame_interval", interval);
				obs_property_modified(property, settings);
				obs_source_update(_source, settings);
				obs_data_release(settings);
				break;
			}

			obs_property_next(&property);
		}
	}

	void DshowSource::create(const char* name, obs_scene_t *scene)
	{
		
		BaseSource::create(name, scene);

		init_cams_info();
		if(_vecCam.size() > 0)
			set_cam_device(_vecCam[0].second.c_str());
	}

	void DshowSource::setChromaParameters(ChromaKey &ck)
	{
		if (_chroma_key_filter != nullptr)
		{
			obs_data_t *settings = obs_source_get_settings(_chroma_key_filter);

			obs_data_set_int(settings, CK_SETTING_OPACITY, ck.opacity);
			obs_data_set_double(settings, CK_SETTING_CONTRAST, ck.contrast);
			obs_data_set_double(settings, CK_SETTING_BRIGHTNESS, ck.brightness);
			obs_data_set_double(settings, CK_SETTING_GAMMA, ck.gamma);
			obs_data_set_int(settings, CK_SETTING_KEY_COLOR, ck.key_color);
			obs_data_set_string(settings, CK_SETTING_COLOR_TYPE, ck.key_color_type.c_str());
			obs_data_set_int(settings, CK_SETTING_SIMILARITY, ck.similarity);
			obs_data_set_int(settings, CK_SETTING_SMOOTHNESS, ck.smoothness);
			obs_data_set_int(settings, CK_SETTING_SPILL, ck.spill);

			obs_source_update(_chroma_key_filter, settings);

			obs_data_release(settings);
		}
	}

	void DshowSource::setChromaEnable(bool enable)
	{
		if (enable)
		{
			if (_chroma_key_filter == nullptr)
			{
				const char *sourceName = obs_source_get_name(_source);
				_chroma_key_filter = obs_source_create("chroma_key_filter", "ace_chroma",
					nullptr, nullptr);
				if (_chroma_key_filter) 
				{
					blog(LOG_INFO, "User added filter '%s' (%s) "
						"to source '%s'",
						"ace_chroma", "chroma_key_filter", sourceName);

					obs_source_filter_add(_source, _chroma_key_filter);
					obs_source_release(_chroma_key_filter);
				}
				else
				{
					_chroma_key_filter = nullptr;
					blog(LOG_ERROR, "Failed to added filter '%s' (%s) "
						"to source '%s'",
						"ace_chroma", "chroma_key_filter", sourceName);
				}
			}
		}

		if (_chroma_key_filter != nullptr)
		{
			obs_source_set_enabled(_chroma_key_filter, enable);
		}
	}
}