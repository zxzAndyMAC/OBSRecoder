
#include <windows.h>
#include "OBSMain.h"
#include <shlobj.h>
#include <algorithm>
#include <time.h>
#include <sstream>

namespace OBS {
	
	static const double s_scaled_vals[] =
	{
		1.0,
		1.25,
		(1.0 / 0.75),
		1.5,
		(1.0 / 0.6),
		1.75,
		2.0,
		2.25,
		2.5,
		2.75,
		3.0,
		0.0
	};

	/* some nice default output resolution vals */
	static const double vals[] =
	{
		1.0,
		1.25,
		(1.0 / 0.75),
		1.5,
		(1.0 / 0.6),
		1.75,
		2.0,
		2.25,
		2.5,
		2.75,
		3.0
	};

	static string GetDefaultVideoSavePath()
	{
		wchar_t path_utf16[MAX_PATH];
		char    path_utf8[MAX_PATH] = {};

		SHGetFolderPathW(NULL, CSIDL_MYVIDEO, NULL, SHGFP_TYPE_CURRENT,
			path_utf16);

		os_wcs_to_utf8(path_utf16, wcslen(path_utf16), path_utf8, MAX_PATH);
		return string(path_utf8);
	}

	OBSMain::OBSMain()
		:_basicConfig(nullptr),
		_simpleout(nullptr),
		_capture_frame(nullptr),
		_dshow(nullptr),
		_color_source(nullptr),
		_scene(nullptr),
		_boundsType(OBS_BOUNDS_NONE),
		_test(nullptr)
	{
		init();
	}

	OBSMain::~OBSMain()
	{
		config_close(_basicConfig);
#ifdef OBS_CPU_INFO
		os_cpu_usage_info_destroy(_cpu_info);
#endif // OBS_CPU_INFO

		ACE_MOT::releaseAceMonitor();

		if (_test != nullptr)
		{
			delete _test;
			_test = nullptr;
		}
		for (VolControl *vol : _volumes)
			delete vol;
		_volumes.clear();

		if (_dshow != nullptr)
		{
			delete _dshow;
			_dshow = nullptr;
		}

		if (_color_source != nullptr)
		{
			delete _color_source;
			_color_source = nullptr;
		}

		obs_set_output_source(0, nullptr);
		obs_set_output_source(1, nullptr);
		obs_set_output_source(2, nullptr);
		obs_set_output_source(3, nullptr);
		obs_set_output_source(4, nullptr);
		obs_set_output_source(5, nullptr);

		auto cb = [](void *unused, obs_source_t *source)
		{
			obs_source_remove(source);
			UNUSED_PARAMETER(unused);
			return true;
		};

		obs_enum_sources(cb, nullptr);

		if (_capture_frame != nullptr)
		{
			closePreview();
		}
		if (_simpleout != nullptr)
		{
			_simpleout->stopRecording();
			delete _simpleout;
			_simpleout = nullptr;
		}
	}

	void OBSMain::init()
	{
		_log.create_log_file();
		initBasicConfigDefaults();

		/* create OBS */
		blog(LOG_INFO, "===========create OBS============");
		if (!obs_startup("en-US", nullptr, nullptr))
			throw "Couldn't create OBS";

		/****************************************BrowserHWAccel********************************************/
		uint32_t winver = getWindowsVersion();
		bool browserHWAccel = winver > 0x601;
		obs_data_t *settings = obs_data_create();
		obs_data_set_bool(settings, "BrowserHWAccel", browserHWAccel);
		obs_apply_private_data(settings);
		obs_data_release(settings);

		blog(LOG_INFO, "Current Date/Time: %s", currentDateTimeString().c_str());

		blog(LOG_INFO, "Browser Hardware Acceleration: %s",
			browserHWAccel ? "true" : "false");
		/****************************************end********************************************/
		printSegmentation("initOBSCallbacks");
		initOBSCallbacks();

		//calldata_init(&_frame_data_cd);
		//calldata_set_ptr(&_frame_data_cd, "frame_addr", _pixel_info.rgba);
		//calldata_set_int(&_frame_data_cd, "frame_width", _pixel_info.width);
		//calldata_set_int(&_frame_data_cd, "frame_height", _pixel_info.height);

		printSegmentation("===========load modules============");
		/* load modules */
		const char* bin = "./obs-modules/obs-plugins/32bit";
		const char* data = "./obs-modules/data/obs-plugins/%module%";
		obs_add_module_path(bin, data);
		blog(LOG_INFO, "---------------------------------");
		obs_load_all_modules();
		blog(LOG_INFO, "---------------------------------");
		obs_log_loaded_modules();
		blog(LOG_INFO, "---------------------------------");
		obs_post_load_modules();


		////////////////////////////////////////
		
		if (!resetAudio())
		{
			blog(LOG_ERROR, "Failed to initialize audio");
		}
		
		tryresetVedio();

		printSegmentation("===========set_audio_monitoring_device============");
#if defined(_WIN32) || defined(__APPLE__) || HAVE_PULSEAUDIO
		const char *device_name = config_get_string(_basicConfig, "Audio",
			"MonitoringDeviceName");
		const char *device_id = config_get_string(_basicConfig, "Audio",
			"MonitoringDeviceId");

		obs_set_audio_monitoring_device(device_name, device_id);

		blog(LOG_INFO, "Audio monitoring device:\n\tname: %s\n\tid: %s",
			device_name, device_id);
#endif

		printSegmentation("===========init basic config============");
		initBasicConfigDefaults2();

		printSegmentation("===========check simple x264============");
		checkForSimpleModeX264Fallback();

		blog(LOG_INFO, STARTUP_SEPARATOR);

		printSegmentation("===========create simple output============");
		_simpleout = new SimpleOut(this);
		_simpleout->simpleoutputInit();

		printSegmentation("===========create default scene============");
		_scene = obs_scene_create("ACEKid scene");
		if (!_scene)
			throw "Couldn't create scene";

		printSegmentation("===========create firstrun Sources============");
		createFirstRunSources();
		loadAudioDevices();

		printSegmentation("===========create dshow Source============");

		_dshow = new DshowSource();
		_dshow->create("ace-webcam", _scene);
		setResolution(_dshow->getDefaultres());
		setOutputResolution(_dshow->getDefaultres());
		obs_set_output_source(0, obs_scene_get_source(_scene));
		obs_scene_release(_scene);
#ifdef OBS_CPU_INFO
		_cpu_info = os_cpu_usage_info_start();
#endif // OBS_CPU_INFO
	}

	void OBSMain::setBackgroundColorVisible(bool visible)
	{
		if (visible)
		{
			if (_color_source == nullptr)
			{
				_color_source = new ColorSource();
				_color_source->create("ace-color", _scene);

				uint32_t base_witdh = (uint32_t)config_get_uint(_basicConfig,
					"Video", "BaseCX");
				uint32_t base_height = (uint32_t)config_get_uint(_basicConfig,
					"Video", "BaseCY");
				_color_source->setSize(base_witdh, base_height);

				_color_source->orderItem(obs_order_movement::OBS_ORDER_MOVE_DOWN);
			}
		}
		if (_color_source != nullptr)
			_color_source->setVisible(visible);
	}

	void OBSMain::setBackgroundColor(int color /*0xFF00FF00*/)
	{
		if (_color_source != nullptr)
			_color_source->setBackgroundColor(color);
	}

	void OBSMain::setChromaEnable(bool enable)
	{
		if (_dshow != nullptr)
		{
			_dshow->setChromaEnable(enable);
		}
	}

	void OBSMain::setChromaParameters(ChromaKey &ck)
	{
		if (_dshow != nullptr)
		{
			_dshow->setChromaParameters(ck);
		}

	}

	PixelInfo *OBSMain::getRawData()
	{
		//proc_handler_t *ph = obs_output_get_proc_handler(_frameOutput);
		//proc_handler_call(ph, "copy_frame_data", &_frame_data_cd);
		if (nullptr == _capture_frame)
			return nullptr;
		return _capture_frame->getRawData();
	}

	bool OBSMain::openPreview()
	{
		blog(LOG_INFO, "=======openPreview======");
		if (_capture_frame)
		{
			return false;
		}
		_capture_frame = new OBSCaptureFrame();
		return _capture_frame->openPreview();
	}

	void OBSMain::closePreview()
	{
		blog(LOG_INFO, "=======closePreview======");
		if (nullptr == _capture_frame)
			return;
		_capture_frame->closePreview();
		delete _capture_frame;
		_capture_frame = nullptr;
	}

	void OBSMain::setResolution(const std::string &res)
	{
		uint32_t BaseCX;
		uint32_t BaseCY;

		convertResText(res.c_str(), BaseCX, BaseCY);
		resetDownscales(BaseCX, BaseCY);

		config_set_int(_basicConfig, "Video", "BaseCX", BaseCX);
		config_set_int(_basicConfig, "Video", "BaseCY", BaseCY);

		if (_color_source != nullptr)
		{
			_color_source->setSize(BaseCX, BaseCY);
		}

		saveConfig();
	}

	string OBSMain::resString(uint32_t cx, uint32_t cy)
	{
		stringstream res;
		res << cx << "x" << cy;
		return res.str();
	}

	void OBSMain::setOutputResolution(const Output_Resulution_Show_Name &name)
	{
		uint32_t outputCX;
		uint32_t outputCY;
		convertResText(name.c_str(), outputCX, outputCY);
		//resetDownscales(outputCX, outputCY);

		config_set_int(_basicConfig, "Video", "OutputCX", outputCX);
		config_set_int(_basicConfig, "Video", "OutputCY", outputCY);

		config_save_safe(_basicConfig, "tmp", nullptr);

		tryresetVedio();
	}

	bool OBSMain::centerAlignSelectedItems(obs_scene_t *scene, obs_sceneitem_t *item, void *param)
	{
		obs_bounds_type boundsType = *reinterpret_cast<obs_bounds_type*>(param);

		if (obs_sceneitem_is_group(item))
			obs_sceneitem_group_enum_items(item, OBSMain::centerAlignSelectedItems,
				param);
		if (!obs_sceneitem_selected(item))
			return true;

		obs_video_info ovi;
		obs_get_video_info(&ovi);

		obs_transform_info itemInfo;
		vec2_set(&itemInfo.pos, 0.0f, 0.0f);
		vec2_set(&itemInfo.scale, 1.0f, 1.0f);
		itemInfo.alignment = OBS_ALIGN_LEFT | OBS_ALIGN_TOP;
		itemInfo.rot = 0.0f;

		vec2_set(&itemInfo.bounds,
			float(ovi.base_width), float(ovi.base_height));
		itemInfo.bounds_type = boundsType;
		itemInfo.bounds_alignment = OBS_ALIGN_CENTER;

		obs_sceneitem_set_info(item, &itemInfo);

		UNUSED_PARAMETER(scene);
		return true;
	}

	void OBSMain::tryresetVedio()
	{
		int ret = resetVideo();
		if (ret != OBS_VIDEO_SUCCESS)
			return;

		if (_scene != nullptr)
		{
			_boundsType = OBS_BOUNDS_SCALE_INNER;
			obs_scene_enum_items(_scene, OBSMain::centerAlignSelectedItems,
				&_boundsType);
		}

		if (_dshow != nullptr)
		{
			uint32_t base_witdh = (uint32_t)config_get_uint(_basicConfig,
				"Video", "BaseCX");
			uint32_t base_height = (uint32_t)config_get_uint(_basicConfig,
				"Video", "BaseCY");
			_dshow->setScale(base_witdh, base_height);
		}
	}

	void OBSMain::resetDownscales(uint32_t cx, uint32_t cy)
	{
		const size_t numVals = sizeof(vals) / sizeof(double);
		// (uint32_t)config_get_uint(_basicConfig, "Video", "OutputCX");
		// (uint32_t)config_get_uint(_basicConfig, "Video", "OutputCY");

		_outputresolutions.clear();
		_outputresolutions.swap(OutputResulutions());

		for (size_t idx = 0; idx < numVals; idx++) 
		{
			uint32_t downscaleCX = uint32_t(double(cx) / vals[idx]);
			uint32_t downscaleCY = uint32_t(double(cy) / vals[idx]);

			downscaleCX &= 0xFFFFFFFC;
			downscaleCY &= 0xFFFFFFFE;

			string res = resString(downscaleCX, downscaleCY);
			_outputresolutions.push_back(res);
		}
	}

	bool OBSMain::convertResText(const char *res, uint32_t &cx, uint32_t &cy)
	{
		BaseLexer lex;
		base_token token;

		lexer_start(lex, res);

		/* parse width */
		if (!lexer_getbasetoken(lex, &token, IGNORE_WHITESPACE))
			return false;
		if (token.type != BASETOKEN_DIGIT)
			return false;

		cx = std::stoul(token.text.array);

		/* parse 'x' */
		if (!lexer_getbasetoken(lex, &token, IGNORE_WHITESPACE))
			return false;
		if (strref_cmpi(&token.text, "x") != 0)
			return false;

		/* parse height */
		if (!lexer_getbasetoken(lex, &token, IGNORE_WHITESPACE))
			return false;
		if (token.type != BASETOKEN_DIGIT)
			return false;

		cy = std::stoul(token.text.array);

		/* shouldn't be any more tokens after this */
		if (lexer_getbasetoken(lex, &token, IGNORE_WHITESPACE))
			return false;

		return true;
	}

	void OBSMain::testDevice(FPSType style, std::function<void()> cb)
	{
		printSegmentation("======================begin test device======================");
		if (_capture_frame != nullptr)
		{
			_capture_frame->closePreview();
		}
		if (_test == nullptr)
		{
			_test = new RecordingTest();
		}

		auto testcb = [this, cb]() {

			//main->ResetVideo();
			//main->ResetOutputs();
			tryresetVedio();

			if (_simpleout != nullptr)
			{
				_simpleout->stopRecording();
				delete _simpleout;
				_simpleout = new SimpleOut(this);
				_simpleout->simpleoutputInit();
			}
			cb();
		};

		_test->setcallback(testcb);
		uint32_t type = config_get_uint(_basicConfig, "Video", "FPSType");
		_test->excuteTestThread(_basicConfig, style);
	}

	void OBSMain::refreshCameras()
	{
		if (nullptr != _dshow)
		{
			delete _dshow;
			_dshow = nullptr;
		}
		_dshow = new DshowSource();
		_dshow->create("acekid", _scene);
	}

	void OBSMain::loadListValues(obs_property_t *prop, int index)
	{
		size_t count = obs_property_list_item_count(prop);

		obs_source_t *source = obs_get_output_source(index);
		const char *deviceId = nullptr;
		obs_data_t *settings = nullptr;

		if (source) {
			settings = obs_source_get_settings(source);
			if (settings)
				deviceId = obs_data_get_string(settings, "device_id");
		}

		for (size_t i = 0; i < count; i++) {
			const char *name = obs_property_list_item_name(prop, i);
			const char *id = obs_property_list_item_string(prop, i);
			if (index == AUX_AUDIO_CHANNEL)
			{
				_auxaudios.push_back(std::make_pair(id, name));
			}
			else
			{
				_dektopaudios.push_back(std::make_pair(id, name));
			}
		}

		if (settings)
			obs_data_release(settings);
		if (source)
			obs_source_release(source);
	}

	void OBSMain::loadAudioDevices()
	{
		obs_properties_t *input_props = obs_get_source_properties(INPUT_AUDIO_SOURCE);
		obs_properties_t *output_props = obs_get_source_properties(OUTPUT_AUDIO_SOURCE);

		if (input_props)
		{
			obs_property_t *inputs = obs_properties_get(input_props,
				"device_id");
			loadListValues(inputs, AUX_AUDIO_CHANNEL);
			obs_properties_destroy(input_props);
		}

		if (output_props)
		{
			obs_property_t *outputs = obs_properties_get(output_props,
				"device_id");
			loadListValues(outputs, DESKTOP_AUDIO_CHANNEL);
			obs_properties_destroy(output_props);
		}
	}

	bool OBSMain::hasAudioDevices(const char *source_id)
	{
		const char *output_id = source_id;
		obs_properties_t *props = obs_get_source_properties(output_id);
		size_t count = 0;

		if (!props)
			return false;

		obs_property_t *devices = obs_properties_get(props, "device_id");
		if (devices)
			count = obs_property_list_item_count(devices);

		obs_properties_destroy(props);

		return count != 0;
	}

	void OBSMain::setDesktopAudioDevice(const Desktop_Audio_ID &id)
	{
		for (auto vl : _dektopaudios)
		{
			if (strcmp(vl.first.c_str(), id.c_str()) == 0)
			{
				resetAudioDevice(OUTPUT_AUDIO_SOURCE, id.c_str(), vl.second.c_str(), DESKTOP_AUDIO_CHANNEL);
			}
		}
	}

	void OBSMain::setAuxAudioDevice(const Aux_Audio_ID &id)
	{
		for (auto vl : _auxaudios)
		{
			if (strcmp(vl.first.c_str(), id.c_str()) == 0)
			{
				resetAudioDevice(INPUT_AUDIO_SOURCE, id.c_str(), vl.second.c_str(), AUX_AUDIO_CHANNEL);
			}
		}
	}

	void OBSMain::resetAudioDevice(const char *sourceId, const char *deviceId, const char *deviceDesc, int channel)
	{
		bool disable = deviceId && strcmp(deviceId, "disabled") == 0;
		obs_source_t *source;
		obs_data_t *settings;

		source = obs_get_output_source(channel);
		if (source) {
			if (disable) {
				obs_set_output_source(channel, nullptr);
			}
			else {
				settings = obs_source_get_settings(source);
				const char *oldId = obs_data_get_string(settings,
					"device_id");
				if (strcmp(oldId, deviceId) != 0) {
					obs_data_set_string(settings, "device_id",
						deviceId);
					obs_source_update(source, settings);
				}
				obs_data_release(settings);
			}

			obs_source_release(source);

		}
		else if (!disable) {
			settings = obs_data_create();
			obs_data_set_string(settings, "device_id", deviceId);
			source = obs_source_create(sourceId, deviceDesc, settings,
				nullptr);
			obs_data_release(settings);

			obs_set_output_source(channel, source);
			obs_source_release(source);
		}
	}

	void OBSMain::createFirstRunSources()
	{
		bool hasDesktopAudio = hasAudioDevices(OUTPUT_AUDIO_SOURCE);
		bool hasInputAudio = hasAudioDevices(INPUT_AUDIO_SOURCE);

		if (hasDesktopAudio)
			resetAudioDevice(OUTPUT_AUDIO_SOURCE, "default",
				"Desktop Audio", DESKTOP_AUDIO_CHANNEL);
		if (hasInputAudio)
			resetAudioDevice(INPUT_AUDIO_SOURCE, "default",
				"Mic/Aux", AUX_AUDIO_CHANNEL);
	}

	void OBSMain::checkForSimpleModeX264Fallback()
	{
		const char *curStreamEncoder = config_get_string(_basicConfig,
			"SimpleOutput", "StreamEncoder");
		const char *curRecEncoder = config_get_string(_basicConfig,
			"SimpleOutput", "RecEncoder");
		bool qsv_supported = false;
		bool amd_supported = false;
		bool nve_supported = false;
		bool changed = false;
		size_t idx = 0;
		const char *id;

		while (obs_enum_encoder_types(idx++, &id)) {
			if (strcmp(id, "amd_amf_h264") == 0)
				amd_supported = true;
			else if (strcmp(id, "obs_qsv11") == 0)
				qsv_supported = true;
			else if (strcmp(id, "ffmpeg_nvenc") == 0)
				nve_supported = true;
		}

		auto CheckEncoder = [&](const char *&name)
		{
			if (strcmp(name, SIMPLE_ENCODER_QSV) == 0) {
				if (!qsv_supported) {
					changed = true;
					name = SIMPLE_ENCODER_X264;
					return false;
				}
			}
			else if (strcmp(name, SIMPLE_ENCODER_NVENC) == 0) {
				if (!nve_supported) {
					changed = true;
					name = SIMPLE_ENCODER_X264;
					return false;
				}
			}
			else if (strcmp(name, SIMPLE_ENCODER_AMD) == 0) {
				if (!amd_supported) {
					changed = true;
					name = SIMPLE_ENCODER_X264;
					return false;
				}
			}

			return true;
		};

		if (!CheckEncoder(curStreamEncoder))
			config_set_string(_basicConfig,
				"SimpleOutput", "StreamEncoder",
				curStreamEncoder);
		if (!CheckEncoder(curRecEncoder))
			config_set_string(_basicConfig,
				"SimpleOutput", "RecEncoder",
				curRecEncoder);
		if (changed)
			config_save_safe(_basicConfig, "tmp", nullptr);
	}

	void OBSMain::initBasicConfigDefaults2()
	{
		bool useOldDefaults = LIBOBS_API_VER < MAKE_SEMANTIC_VERSION(23, 0, 0);

		bool useNV = encoderAvailable("ffmpeg_nvenc") && !useOldDefaults;

		config_set_default_string(_basicConfig, "SimpleOutput", "StreamEncoder",
			useNV ? SIMPLE_ENCODER_NVENC : SIMPLE_ENCODER_X264);
		config_set_default_string(_basicConfig, "SimpleOutput", "RecEncoder",
			useNV ? SIMPLE_ENCODER_NVENC : SIMPLE_ENCODER_X264);
	}

	void OBSMain::printSegmentation(const char* name)
	{
		blog(LOG_INFO, "=============================================");
		blog(LOG_INFO, "%s", name);
	}


	void OBSMain::getFPSCommon(uint32_t &num, uint32_t &den)
	{
		const char *val = config_get_string(_basicConfig, "Video", "FPSCommon");

		if (strcmp(val, "10") == 0) {
			num = 10;
			den = 1;
		}
		else if (strcmp(val, "20") == 0) {
			num = 20;
			den = 1;
		}
		else if (strcmp(val, "24 NTSC") == 0) {
			num = 24000;
			den = 1001;
		}
		else if (strcmp(val, "25 PAL") == 0) {
			num = 25;
			den = 1;
		}
		else if (strcmp(val, "29.97") == 0) {
			num = 30000;
			den = 1001;
		}
		else if (strcmp(val, "48") == 0) {
			num = 48;
			den = 1;
		}
		else if (strcmp(val, "50 PAL") == 0) {
			num = 50;
			den = 1;
		}
		else if (strcmp(val, "59.94") == 0) {
			num = 60000;
			den = 1001;
		}
		else if (strcmp(val, "60") == 0) {
			num = 60;
			den = 1;
		}
		else {
			num = 30;
			den = 1;
		}
	}

	void OBSMain::getFPSInteger(uint32_t &num, uint32_t &den)
	{
		num = (uint32_t)config_get_uint(_basicConfig, "Video", "FPSInt");
		den = 1;
	}

	void OBSMain::getFPSFraction(uint32_t &num, uint32_t &den)
	{
		num = (uint32_t)config_get_uint(_basicConfig, "Video", "FPSNum");
		den = (uint32_t)config_get_uint(_basicConfig, "Video", "FPSDen");
	}

	void OBSMain::getFPSNanoseconds(uint32_t &num, uint32_t &den)
	{
		num = 1000000000;
		den = (uint32_t)config_get_uint(_basicConfig, "Video", "FPSNS");
	}

	void OBSMain::getConfigFPS(uint32_t &num, uint32_t &den)
	{
		uint32_t type = config_get_uint(_basicConfig, "Video", "FPSType");

		if (type == 1) //"Integer"
			getFPSInteger(num, den);
		else if (type == 2) //"Fraction"
			getFPSFraction(num, den);
		else if (false) //"Nanoseconds", currently not implemented
			getFPSNanoseconds(num, den);
		else
			getFPSCommon(num, den);
	}


	enum video_format OBSMain::getVideoFormatFromName(const char *name)
	{
		if (astrcmpi(name, "I420") == 0)
			return VIDEO_FORMAT_I420;
		else if (astrcmpi(name, "NV12") == 0)
			return VIDEO_FORMAT_NV12;
		else if (astrcmpi(name, "I444") == 0)
			return VIDEO_FORMAT_I444;
#if 0 //currently unsupported
		else if (astrcmpi(name, "YVYU") == 0)
			return VIDEO_FORMAT_YVYU;
		else if (astrcmpi(name, "YUY2") == 0)
			return VIDEO_FORMAT_YUY2;
		else if (astrcmpi(name, "UYVY") == 0)
			return VIDEO_FORMAT_UYVY;
#endif
		else
			return VIDEO_FORMAT_RGBA;
	}

	enum obs_scale_type OBSMain::getScaleType(config_t *basicConfig)
	{
		const char *scaleTypeStr = config_get_string(basicConfig,
			"Video", "ScaleType");

		if (astrcmpi(scaleTypeStr, "bilinear") == 0)
			return OBS_SCALE_BILINEAR;
		else if (astrcmpi(scaleTypeStr, "lanczos") == 0)
			return OBS_SCALE_LANCZOS;
		else
			return OBS_SCALE_BICUBIC;
	}

	int OBSMain::attemptToResetVideo(struct obs_video_info *ovi)
	{
		return obs_reset_video(ovi);
	}

	bool OBSMain::resetVideo()
	{
		printSegmentation("===========resetVideo============");
		struct obs_video_info ovi;
		int ret;

		getConfigFPS(ovi.fps_num, ovi.fps_den);

		const char *colorFormat = config_get_string(_basicConfig, "Video",
			"ColorFormat");
		const char *colorSpace = config_get_string(_basicConfig, "Video",
			"ColorSpace");
		const char *colorRange = config_get_string(_basicConfig, "Video",
			"ColorRange");

		ovi.graphics_module = DL_D3D11;// DL_OPENGL;
		ovi.base_width = (uint32_t)config_get_uint(_basicConfig,
			"Video", "BaseCX");
		ovi.base_height = (uint32_t)config_get_uint(_basicConfig,
			"Video", "BaseCY");
		ovi.output_width = (uint32_t)config_get_uint(_basicConfig,
			"Video", "OutputCX");
		ovi.output_height = (uint32_t)config_get_uint(_basicConfig,
			"Video", "OutputCY");
		ovi.output_format = getVideoFormatFromName(colorFormat);
		ovi.colorspace = astrcmpi(colorSpace, "601") == 0 ?
			VIDEO_CS_601 : VIDEO_CS_709;
		ovi.range = astrcmpi(colorRange, "Full") == 0 ?
			VIDEO_RANGE_FULL : VIDEO_RANGE_PARTIAL;
		ovi.adapter = 0;
		ovi.gpu_conversion = true;
		ovi.scale_type = getScaleType(_basicConfig);

		if (ovi.base_width == 0 || ovi.base_height == 0) {
			ovi.base_width = 1280;
			ovi.base_height = 720;
			config_set_uint(_basicConfig, "Video", "BaseCX", 1280);
			config_set_uint(_basicConfig, "Video", "BaseCY", 720);
		}

		if (ovi.output_width == 0 || ovi.output_height == 0) {
			ovi.output_width = ovi.base_width;
			ovi.output_height = ovi.base_height;
			config_set_uint(_basicConfig, "Video", "OutputCX",
				ovi.base_width);
			config_set_uint(_basicConfig, "Video", "OutputCY",
				ovi.base_height);
		}

		ret = attemptToResetVideo(&ovi);
		if (_WIN32 && ret != OBS_VIDEO_SUCCESS) {
			if (ret == OBS_VIDEO_CURRENTLY_ACTIVE) {
				blog(LOG_WARNING, "Tried to reset when "
					"already active");
				return ret;
			}

			/* Try OpenGL if DirectX fails on windows */
			if (astrcmpi(ovi.graphics_module, DL_OPENGL) != 0) {
				blog(LOG_WARNING, "Failed to initialize obs video (%d) "
					"with graphics_module='%s', retrying "
					"with graphics_module='%s'",
					ret, ovi.graphics_module,
					DL_OPENGL);
				ovi.graphics_module = DL_OPENGL;
				ret = attemptToResetVideo(&ovi);
			}
		}
		else if (ret == OBS_VIDEO_SUCCESS) {
		}

		return ret;
	}

	bool OBSMain::resetAudio()
	{
		printSegmentation("===========resetAudio============");
		struct obs_audio_info ai;
		ai.samples_per_sec = config_get_uint(_basicConfig, "Audio",
			"SampleRate");

		const char *channelSetupStr = config_get_string(_basicConfig,
			"Audio", "ChannelSetup");

		if (strcmp(channelSetupStr, "Mono") == 0)
			ai.speakers = SPEAKERS_MONO;
		else if (strcmp(channelSetupStr, "2.1") == 0)
			ai.speakers = SPEAKERS_2POINT1;
		else if (strcmp(channelSetupStr, "4.0") == 0)
			ai.speakers = SPEAKERS_4POINT0;
		else if (strcmp(channelSetupStr, "4.1") == 0)
			ai.speakers = SPEAKERS_4POINT1;
		else if (strcmp(channelSetupStr, "5.1") == 0)
			ai.speakers = SPEAKERS_5POINT1;
		else if (strcmp(channelSetupStr, "7.1") == 0)
			ai.speakers = SPEAKERS_7POINT1;
		else
			ai.speakers = SPEAKERS_STEREO;

		return obs_reset_audio(&ai);
	}


	void OBSMain::initOBSCallbacks()
	{
		signal_handler_connect(obs_get_signal_handler(), "source_activate",
			OBSMain::sourceActivated, this);//最后传this

		signal_handler_connect(obs_get_signal_handler(), "source_deactivate",
			OBSMain::sourceDeactivated, this);//最后传this
	}

	void OBSMain::deactivateAudioSource(obs_source_t *source)
	{
		for (size_t i = 0; i < _volumes.size(); i++) {
			if (_volumes[i]->getSource() == source) {
				delete _volumes[i];
				_volumes.erase(_volumes.begin() + i);
				break;
			}
		} //管理 volctrol
	}

	void OBSMain::sourceActivated(void *data, calldata_t *params)
	{
		OBSMain *obsmain = static_cast<OBSMain*>(data);
		obs_source_t *source = (obs_source_t*)calldata_ptr(params, "source");
		uint32_t     flags = obs_source_get_output_flags(source);
		if (flags & OBS_SOURCE_AUDIO)
		{
			VolControl *vol = new VolControl(source); //管理这个类
			obsmain->_volumes.push_back(vol);
		}

	}

	void OBSMain::setDesktopAudioMute(bool mute)
	{
		setmute(mute, OUTPUT_AUDIO_SOURCE);
	}

	void OBSMain::setAuxAudioMute(bool mute)
	{
		setmute(mute, INPUT_AUDIO_SOURCE);
	}

	const int OBSMain::getVolumeLevel(AudioType atype, float data[], unsigned size)
	{
		const char* id = atype == AudioType::AUX ? INPUT_AUDIO_SOURCE : OUTPUT_AUDIO_SOURCE;

		for (size_t i = 0; i < _volumes.size(); i++)
		{
			if (strcmp(_volumes[i]->getID(), id) == 0)
			{
				return _volumes[i]->getVolumeLevel(data, size);
			}
		}
		return 0;
	}

	void OBSMain::setmute(bool mute, const char *id)
	{
		for (size_t i = 0; i < _volumes.size(); i++)
		{
			if (strcmp(_volumes[i]->getID(), id) == 0)
			{
				_volumes[i]->setMuted(mute);
			}
		}
	}

	void OBSMain::sourceDeactivated(void *data, calldata_t *params)
	{
		OBSMain *obsmain = static_cast<OBSMain*>(data);
		obs_source_t *source = (obs_source_t*)calldata_ptr(params, "source");
		uint32_t     flags = obs_source_get_output_flags(source);

		if (flags & OBS_SOURCE_AUDIO)
			obsmain->deactivateAudioSource(source);
	}

	string OBSMain::currentDateTimeString()
	{
		time_t     now = time(0);
		struct tm  tstruct;
		char       buf[80];
		tstruct = *localtime(&now);
		strftime(buf, sizeof(buf), "%Y-%m-%d, %X", &tstruct);
		return buf;
	}

	uint32_t OBSMain::getWindowsVersion()
	{
		static uint32_t ver = 0;

		if (ver == 0) {
			struct win_version_info ver_info;

			get_win_ver(&ver_info);
			ver = (ver_info.major << 8) | ver_info.minor;
		}

		return ver;
	}

	bool OBSMain::encoderAvailable(const char *encoder)
	{
		const char *val;
		int i = 0;

		while (obs_enum_encoder_types(i++, &val))
			if (strcmp(val, encoder) == 0)
				return true;

		return false;
	}

	bool OBSMain::initBasicConfigDefaults()
	{
		os_mkdirs(OBS_CONFIG_DIR("/obsConfig"));
		string vedioSafePath = GetDefaultVideoSavePath();
		string conf(OBS_CONFIG_DIR("/obsConfig"));
		conf.append("/obsbasic.ini");
		const char *configfile = conf.c_str();

		int code = config_open(&_basicConfig, configfile, CONFIG_OPEN_ALWAYS);
		if (code != CONFIG_SUCCESS) {
			blog(LOG_ERROR, "Failed to open obsbasic.ini: %d", code);
			return false;
		}

		uint32_t cx = 1920;
		uint32_t cy = 1080;

		/* use 1920x1080 for new default base res if main monitor is above
		* 1920x1080, but don't apply for people from older builds -- only to
		* new users */
		if ((cx * cy) > (1920 * 1080)) {
			cx = 1920;
			cy = 1080;
		}

		/* ----------------------------------------------------- */

		config_set_default_string(_basicConfig, "Output", "Mode", "Simple");

		//config_set_default_string(_basicConfig, "SimpleOutput", "FilePath",
		//	GetDefaultVideoSavePath().c_str());

		config_set_default_string(_basicConfig, "SimpleOutput", "FilePath",
			vedioSafePath.c_str());

		config_set_default_string(_basicConfig, "SimpleOutput", "RecFormat",
			"flv");
		config_set_default_uint(_basicConfig, "SimpleOutput", "VBitrate",
			2500);
		config_set_default_uint(_basicConfig, "SimpleOutput", "ABitrate", 160);
		config_set_default_bool(_basicConfig, "SimpleOutput", "UseAdvanced",
			false);
		config_set_default_bool(_basicConfig, "SimpleOutput", "EnforceBitrate",
			true);
		config_set_default_string(_basicConfig, "SimpleOutput", "Preset",
			"veryfast");
		config_set_default_string(_basicConfig, "SimpleOutput", "NVENCPreset",
			"hq");
		config_set_default_string(_basicConfig, "SimpleOutput", "RecQuality",
			"Stream");
		config_set_default_bool(_basicConfig, "SimpleOutput", "RecRB", false);
		config_set_default_int(_basicConfig, "SimpleOutput", "RecRBTime", 20);
		config_set_default_int(_basicConfig, "SimpleOutput", "RecRBSize", 512);
		config_set_default_string(_basicConfig, "SimpleOutput", "RecRBPrefix",
			"Replay");

		config_set_default_uint(_basicConfig, "Video", "BaseCX", cx);
		config_set_default_uint(_basicConfig, "Video", "BaseCY", cy);

		/* don't allow BaseCX/BaseCY to be susceptible to defaults changing */
		if (!config_has_user_value(_basicConfig, "Video", "BaseCX") ||
			!config_has_user_value(_basicConfig, "Video", "BaseCY")) {
			config_set_uint(_basicConfig, "Video", "BaseCX", cx);
			config_set_uint(_basicConfig, "Video", "BaseCY", cy);
			config_save_safe(_basicConfig, "tmp", nullptr);
		}

		config_set_default_string(_basicConfig, "Output", "FilenameFormatting",
			"%CCYY-%MM-%DD %hh-%mm-%ss");

		config_set_default_bool(_basicConfig, "Output", "DelayEnable", false);
		config_set_default_uint(_basicConfig, "Output", "DelaySec", 20);
		config_set_default_bool(_basicConfig, "Output", "DelayPreserve", true);

		config_set_default_bool(_basicConfig, "Output", "Reconnect", true);
		config_set_default_uint(_basicConfig, "Output", "RetryDelay", 10);
		config_set_default_uint(_basicConfig, "Output", "MaxRetries", 20);

		config_set_default_string(_basicConfig, "Output", "BindIP", "default");
		config_set_default_bool(_basicConfig, "Output", "NewSocketLoopEnable",
			false);
		config_set_default_bool(_basicConfig, "Output", "LowLatencyEnable",
			false);

		int i = 0;
		uint32_t scale_cx = cx;
		uint32_t scale_cy = cy;

		/* use a default scaled resolution that has a pixel count no higher
		* than 1280x720 */
		while (((scale_cx * scale_cy) > (1280 * 720)) && s_scaled_vals[i] > 0.0) {
			double scale = s_scaled_vals[i++];
			scale_cx = uint32_t(double(cx) / scale);
			scale_cy = uint32_t(double(cy) / scale);
		}

		config_set_default_uint(_basicConfig, "Video", "OutputCX", scale_cx);
		config_set_default_uint(_basicConfig, "Video", "OutputCY", scale_cy);

		/* don't allow OutputCX/OutputCY to be susceptible to defaults
		* changing */
		if (!config_has_user_value(_basicConfig, "Video", "OutputCX") ||
			!config_has_user_value(_basicConfig, "Video", "OutputCY")) {
			config_set_uint(_basicConfig, "Video", "OutputCX", scale_cx);
			config_set_uint(_basicConfig, "Video", "OutputCY", scale_cy);
			config_save_safe(_basicConfig, "tmp", nullptr);
		}

		config_set_default_uint(_basicConfig, "Video", "FPSType", 0);
		config_set_default_string(_basicConfig, "Video", "FPSCommon", "30");
		config_set_default_uint(_basicConfig, "Video", "FPSInt", 30);
		config_set_default_uint(_basicConfig, "Video", "FPSNum", 30);
		config_set_default_uint(_basicConfig, "Video", "FPSDen", 1);
		config_set_default_string(_basicConfig, "Video", "ScaleType", "bicubic");
		config_set_default_string(_basicConfig, "Video", "ColorFormat", "I420");
		config_set_default_string(_basicConfig, "Video", "ColorSpace", "601");
		config_set_default_string(_basicConfig, "Video", "ColorRange",
			"Partial");

		config_set_default_string(_basicConfig, "Audio", "MonitoringDeviceId",
			"default");
		config_set_default_string(_basicConfig, "Audio", "MonitoringDeviceName",
			"default");
		config_set_default_uint(_basicConfig, "Audio", "SampleRate", 44100);
		config_set_default_string(_basicConfig, "Audio", "ChannelSetup",
			"Stereo");
		config_set_default_double(_basicConfig, "Audio", "MeterDecayRate",
			VOLUME_METER_DECAY_FAST);
		config_set_default_uint(_basicConfig, "Audio", "PeakMeterType", 0);

		return true;
	}


	void OBSMain::setVideoSafeDir(const char* dir)
	{
		os_mkdirs(dir);
		string _dir = dir;
		char lastChar = _dir.back();
		if (lastChar != '/' && lastChar != '\\')
			_dir += "/";

		replace(_dir.begin(), _dir.end(), '\\', '/');

		config_set_string(_basicConfig, "SimpleOutput", "FilePath",
			_dir.c_str());
		saveConfig();
	}

	void OBSMain::setRecFormat(const char* format)
	{
		config_set_string(_basicConfig, "SimpleOutput", "RecFormat",
			format);
		saveConfig();
	}

	void OBSMain::saveConfig()
	{
		config_save_safe(_basicConfig, "tmp", nullptr);
	}

	void OBSMain::set_fps(const Frame_Interval &interval)
	{
		RecState rs = _simpleout->getState();
		if (rs == RecState::Recording || rs == RecState::Stopping)
		{
			return;
		}
		_dshow->set_fps(interval);
	}

	void OBSMain::set_cam_resolution(const Resolution_Index &index)
	{
		RecState rs = _simpleout->getState();
		if (rs == RecState::Recording || rs == RecState::Stopping)
		{
			return;
		}
		std::string resl = _dshow->set_cam_resolution(index);
		setResolution(resl);
		setOutputResolution(resl);
	}

	void OBSMain::set_cam_device(const Camera_ID &device_id)
	{
		RecState rs = _simpleout->getState();
		if (rs == RecState::Recording || rs == RecState::Stopping)
		{
			return;
		}
		_dshow->set_cam_device(device_id);
	}

	bool OBSMain::startRecording(const char* flag)
	{ 
		if (_simpleout) 
		{ 
			return _simpleout->startRecording(flag);
		} 
		return false;
	}

	void OBSMain::stopRecording()
	{
		if (_simpleout)
			_simpleout->stopRecording();
	}

	const RecState OBSMain::getRecordState() const
	{
		if (_simpleout)
			return _simpleout->getState(); 
		return RecState::Idle;
	}

	const RecState OBSMain::getCamFrameState() const
	{
		if (_capture_frame)
		{
			return _capture_frame->getState();
		}
		return RecState::Idle;
	}

	void OBSMain::setNoiseParameters(NoiseParameters &np)
	{
		for (VolControl *vol : _volumes)
		{
			if (strcmp(vol->getID(), INPUT_AUDIO_SOURCE) == 0)
			{
				vol->setNoiseParameters(np);
			}
		}
	}

	double OBSMain::getCpuInfo()
	{
#ifdef OBS_CPU_INFO
		return  os_cpu_usage_info_query(_cpu_info);
#endif // OBS_CPU_INFO

		return 0.0f;
	}
}