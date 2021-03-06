
#include "OBSSimpleOut.h"
#include <algorithm>

namespace OBS {

	static std::mutex s_stateMutex;

	RecState SimpleOut::s_state = RecState::Idle;

	SimpleOut::SimpleOut(OBSMain* m)
		:_main(m),
		_flag("")
	{
		std::unique_lock<std::mutex> lk(s_stateMutex, std::defer_lock);
		lk.try_lock();
		s_state = RecState::Idle;
	}

	SimpleOut::~SimpleOut()
	{
		//obs_output_release(_fileOutput);//停止录制后需不需要release fileputput
	}

	RecState SimpleOut::getState()
	{
		std::unique_lock<std::mutex> lk(s_stateMutex, std::defer_lock);
		lk.try_lock();
		return SimpleOut::s_state;
	}

	void SimpleOut::OBSStartRecording(void *data, calldata_t *params)
	{
		blog(LOG_INFO, "=====================starting recording=========================");
		SimpleOut* so = static_cast<SimpleOut*>(data);
		std::unique_lock<std::mutex> lk(s_stateMutex, std::defer_lock);
		lk.try_lock();
		SimpleOut::s_state = RecState::Recording;
	}

	void SimpleOut::OBSStopRecording(void *data, calldata_t *params)
	{
		blog(LOG_INFO, "=====================stop recording=========================");
		SimpleOut* so = static_cast<SimpleOut*>(data);
		std::unique_lock<std::mutex> lk(s_stateMutex, std::defer_lock);
		lk.try_lock();
		SimpleOut::s_state = RecState::Stopped;
	}

	void SimpleOut::OBSRecordStopping(void *data, calldata_t *params)
	{
		blog(LOG_INFO, "=====================stopping recording=========================");
		SimpleOut* so = static_cast<SimpleOut*>(data);
		std::unique_lock<std::mutex> lk(s_stateMutex, std::defer_lock);
		lk.try_lock();
		SimpleOut::s_state = RecState::Stopping;
	}

	void SimpleOut::recordingInit()
	{
		_startRecording.Connect(obs_output_get_signal_handler(_fileOutput),
			"start", SimpleOut::OBSStartRecording, this/*this*/);
		_stopRecording.Connect(obs_output_get_signal_handler(_fileOutput),
			"stop", SimpleOut::OBSStopRecording, this/*this*/);
		_recordStopping.Connect(obs_output_get_signal_handler(_fileOutput),
			"stopping", SimpleOut::OBSRecordStopping, this/*this*/);
	}

	bool SimpleOut::createAACEncoder(OBSEncoder &res, string &id, int bitrate, const char *name, size_t idx)
	{
		const char *id_ = GetAACEncoderForBitrate(bitrate);
		if (!id_) {
			id.clear();
			res = nullptr;
			return false;
		}

		if (id == id_)
			return true;

		id = id_;
		res = obs_audio_encoder_create(id_, name, nullptr, idx, nullptr);

		if (res) {
			obs_encoder_release(res);
			return true;
		}

		return false;
	}

	void SimpleOut::loadStreamingPreset_h264(const char *encoderId)
	{
		_h264Streaming = obs_video_encoder_create(encoderId,
			"simple_h264_stream", nullptr, nullptr);
		if (!_h264Streaming)
			throw "Failed to create h264 streaming encoder (simple output)";
		obs_encoder_release(_h264Streaming);
	}

	int SimpleOut::getAudioBitrate()
	{
		int bitrate = (int)config_get_uint(_main->getBaseConfig(), "SimpleOutput",
			"ABitrate");

		return FindClosestAvailableAACBitrate(bitrate);
	}

	void SimpleOut::loadRecordingPreset_Lossless()
	{
		_fileOutput = obs_output_create("ffmpeg_output",
			"simple_ffmpeg_output", nullptr, nullptr);
		if (!_fileOutput)
			throw "Failed to create recording FFmpeg output "
			"(simple output)";
		obs_output_release(_fileOutput);

		obs_data_t *settings = obs_data_create();
		obs_data_set_string(settings, "format_name", "avi");
		obs_data_set_string(settings, "video_encoder", "utvideo");
		obs_data_set_string(settings, "audio_encoder", "pcm_s16le");

		int aMixes = 1;
		obs_output_set_mixers(_fileOutput, aMixes);
		obs_output_update(_fileOutput, settings);
		obs_data_release(settings);
	}

	void SimpleOut::loadRecordingPreset_h264(const char *encoderId)
	{
		_h264Recording = obs_video_encoder_create(encoderId,
			"simple_h264_recording", nullptr, nullptr);
		if (!_h264Recording)
			throw "Failed to create h264 recording encoder (simple output)";
		obs_encoder_release(_h264Recording);
	}

	void SimpleOut::loadRecordingPreset()
	{
		const char *quality = config_get_string(_main->getBaseConfig(), "SimpleOutput",
			"RecQuality");
		const char *encoder = config_get_string(_main->getBaseConfig(), "SimpleOutput",
			"RecEncoder");

		_videoEncoder = encoder;
		_videoQuality = quality;
		_ffmpegOutput = false;

		if (strcmp(quality, "Stream") == 0) {
			_h264Recording = _h264Streaming;
			_aacRecording = _aacStreaming;
			_usingRecordingPreset = false;
			return;

		}
		else if (strcmp(quality, "Lossless") == 0) {
			loadRecordingPreset_Lossless();
			_usingRecordingPreset = true;
			_ffmpegOutput = true;
			return;

		}
		else {
			_lowCPUx264 = false;

			if (strcmp(encoder, SIMPLE_ENCODER_X264) == 0) {
				loadRecordingPreset_h264("obs_x264");
			}
			else if (strcmp(encoder, SIMPLE_ENCODER_X264_LOWCPU) == 0) {
				loadRecordingPreset_h264("obs_x264");
				_lowCPUx264 = true;
			}
			else if (strcmp(encoder, SIMPLE_ENCODER_QSV) == 0) {
				loadRecordingPreset_h264("obs_qsv11");
			}
			else if (strcmp(encoder, SIMPLE_ENCODER_AMD) == 0) {
				loadRecordingPreset_h264("amd_amf_h264");
			}
			else if (strcmp(encoder, SIMPLE_ENCODER_NVENC) == 0) {
				const char *id = _main->encoderAvailable("jim_nvenc")
					? "jim_nvenc"
					: "ffmpeg_nvenc";
				loadRecordingPreset_h264(id);
			}
			_usingRecordingPreset = true;

			if (!createAACEncoder(_aacRecording, _aacRecEncID, 192,
				"simple_aac_recording", 0))
				throw "Failed to create aac recording encoder "
				"(simple output)";
		}
	}

	void SimpleOut::simpleoutputInit()
	{
		const char *encoder = config_get_string(_main->getBaseConfig(), "SimpleOutput",
			"StreamEncoder");

		if (strcmp(encoder, SIMPLE_ENCODER_QSV) == 0) {
			loadStreamingPreset_h264("obs_qsv11");

		}
		else if (strcmp(encoder, SIMPLE_ENCODER_AMD) == 0) {
			loadStreamingPreset_h264("amd_amf_h264");

		}
		else if (strcmp(encoder, SIMPLE_ENCODER_NVENC) == 0) {
			const char *id = _main->encoderAvailable("jim_nvenc")
				? "jim_nvenc"
				: "ffmpeg_nvenc";
			loadStreamingPreset_h264(id);

		}
		else {
			loadStreamingPreset_h264("obs_x264");
		}

		if (!createAACEncoder(_aacStreaming, _aacStreamEncID, getAudioBitrate(),
			"simple_aac", 0))
			blog(LOG_ERROR, "Failed to create aac streaming encoder (simple output)");

		loadRecordingPreset();

		if (!_ffmpegOutput) {
			bool useReplayBuffer = config_get_bool(_main->getBaseConfig(),
				"SimpleOutput", "RecRB");
			if (useReplayBuffer) {
				const char *str = config_get_string(_main->getBaseConfig(),
					"Hotkeys", "ReplayBuffer");
				obs_data_t *hotkey = obs_data_create_from_json(str);
				_replayBuffer = obs_output_create("replay_buffer",
					"Replay Buffer", nullptr, hotkey);

				obs_data_release(hotkey);
				if (!_replayBuffer)
					throw "Failed to create replay buffer output "
					"(simple output)";
				obs_output_release(_replayBuffer);

				signal_handler_t *signal =
					obs_output_get_signal_handler(_replayBuffer);
			}

			_fileOutput = obs_output_create("ffmpeg_muxer",
				"simple_file_output", nullptr, nullptr);
			if (!_fileOutput)
				throw "Failed to create recording output "
				"(simple output)";
			obs_output_release(_fileOutput);
		}

		recordingInit();
	}

	int SimpleOut::calcCRF(int crf)
	{
		int cx = config_get_uint(_main->getBaseConfig(), "Video", "OutputCX");
		int cy = config_get_uint(_main->getBaseConfig(), "Video", "OutputCY");
		double fCX = double(cx);
		double fCY = double(cy);

		if (_lowCPUx264)
			crf -= 2;

		double crossDist = sqrt(fCX * fCX + fCY * fCY);
		double crfResReduction =
			fmin(CROSS_DIST_CUTOFF, crossDist) / CROSS_DIST_CUTOFF;
		crfResReduction = (1.0 - crfResReduction) * 10.0;

		return crf - int(crfResReduction);
	}

	bool SimpleOut::icq_available(obs_encoder_t *encoder)
	{
		obs_properties_t *props = obs_encoder_properties(encoder);
		obs_property_t *p = obs_properties_get(props, "rate_control");
		bool icq_found = false;

		size_t num = obs_property_list_item_count(p);
		for (size_t i = 0; i < num; i++) {
			const char *val = obs_property_list_item_string(p, i);
			if (strcmp(val, "ICQ") == 0) {
				icq_found = true;
				break;
			}
		}

		obs_properties_destroy(props);
		return icq_found;
	}

	void SimpleOut::updateRecordingSettings_qsv11(int crf)
	{
		bool icq = icq_available(_h264Recording);

		obs_data_t *settings = obs_data_create();
		obs_data_set_string(settings, "profile", "high");

		if (icq) {
			obs_data_set_string(settings, "rate_control", "ICQ");
			obs_data_set_int(settings, "icq_quality", crf);
		}
		else {
			obs_data_set_string(settings, "rate_control", "CQP");
			obs_data_set_int(settings, "qpi", crf);
			obs_data_set_int(settings, "qpp", crf);
			obs_data_set_int(settings, "qpb", crf);
		}

		obs_encoder_update(_h264Recording, settings);

		obs_data_release(settings);
	}

	void SimpleOut::updateRecordingSettings_x264_crf(int crf)
	{
		obs_data_t *settings = obs_data_create();
		obs_data_set_int(settings, "crf", crf);
		obs_data_set_bool(settings, "use_bufsize", true);
		obs_data_set_string(settings, "rate_control", "CRF");
		obs_data_set_string(settings, "profile", "high");
		obs_data_set_string(settings, "preset",
			_lowCPUx264 ? "ultrafast" : "veryfast");

		obs_encoder_update(_h264Recording, settings);

		obs_data_release(settings);
	}

	void SimpleOut::updateRecordingSettings_amd_cqp(int cqp)
	{
		obs_data_t *settings = obs_data_create();

		// Static Properties
		obs_data_set_int(settings, "Usage", 0);
		obs_data_set_int(settings, "Profile", 100); // High

													// Rate Control Properties
		obs_data_set_int(settings, "RateControlMethod", 0);
		obs_data_set_int(settings, "QP.IFrame", cqp);
		obs_data_set_int(settings, "QP.PFrame", cqp);
		obs_data_set_int(settings, "QP.BFrame", cqp);
		obs_data_set_int(settings, "VBVBuffer", 1);
		obs_data_set_int(settings, "VBVBuffer.Size", 100000);

		// Picture Control Properties
		obs_data_set_double(settings, "KeyframeInterval", 2.0);
		obs_data_set_int(settings, "BFrame.Pattern", 0);

		// Update and release
		obs_encoder_update(_h264Recording, settings);
		obs_data_release(settings);
	}

	void SimpleOut::updateRecordingSettings_nvenc(int cqp)
	{
		obs_data_t *settings = obs_data_create();
		obs_data_set_string(settings, "rate_control", "CQP");
		obs_data_set_string(settings, "profile", "high");
		obs_data_set_string(settings, "preset", "hq");
		obs_data_set_int(settings, "cqp", cqp);

		obs_encoder_update(_h264Recording, settings);

		obs_data_release(settings);
	}

	void SimpleOut::updateRecordingSettings()
	{
		bool ultra_hq = (_videoQuality == "HQ");
		int crf = calcCRF(ultra_hq ? 16 : 23);

		if (astrcmp_n(_videoEncoder.c_str(), "x264", 4) == 0) {
			updateRecordingSettings_x264_crf(crf);

		}
		else if (_videoEncoder == SIMPLE_ENCODER_QSV) {
			updateRecordingSettings_qsv11(crf);

		}
		else if (_videoEncoder == SIMPLE_ENCODER_AMD) {
			updateRecordingSettings_amd_cqp(crf);

		}
		else if (_videoEncoder == SIMPLE_ENCODER_NVENC) {
			updateRecordingSettings_nvenc(crf);
		}
	}

	void SimpleOut::updateStreamingSettings_amd(obs_data_t *settings, int bitrate)
	{
		// Static Properties
		obs_data_set_int(settings, "Usage", 0);
		obs_data_set_int(settings, "Profile", 100); // High

													// Rate Control Properties
		obs_data_set_int(settings, "RateControlMethod", 3);
		obs_data_set_int(settings, "Bitrate.Target", bitrate);
		obs_data_set_int(settings, "FillerData", 1);
		obs_data_set_int(settings, "VBVBuffer", 1);
		obs_data_set_int(settings, "VBVBuffer.Size", bitrate);

		// Picture Control Properties
		obs_data_set_double(settings, "KeyframeInterval", 2.0);
		obs_data_set_int(settings, "BFrame.Pattern", 0);
	}

	void SimpleOut::update()
	{
		obs_data_t *h264Settings = obs_data_create();
		obs_data_t *aacSettings = obs_data_create();

		int videoBitrate = config_get_uint(_main->getBaseConfig(), "SimpleOutput",
			"VBitrate");
		int audioBitrate = getAudioBitrate();
		bool advanced = config_get_bool(_main->getBaseConfig(), "SimpleOutput",
			"UseAdvanced");
		bool enforceBitrate = config_get_bool(_main->getBaseConfig(), "SimpleOutput",
			"EnforceBitrate");
		const char *custom = config_get_string(_main->getBaseConfig(),
			"SimpleOutput", "x264Settings");
		const char *encoder = config_get_string(_main->getBaseConfig(), "SimpleOutput",
			"StreamEncoder");
		const char *presetType;
		const char *preset;

		if (strcmp(encoder, SIMPLE_ENCODER_QSV) == 0) {
			presetType = "QSVPreset";

		}
		else if (strcmp(encoder, SIMPLE_ENCODER_AMD) == 0) {
			presetType = "AMDPreset";
			updateStreamingSettings_amd(h264Settings, videoBitrate);

		}
		else if (strcmp(encoder, SIMPLE_ENCODER_NVENC) == 0) {
			presetType = "NVENCPreset";

		}
		else {
			presetType = "Preset";
		}

		preset = config_get_string(_main->getBaseConfig(), "SimpleOutput", presetType);

		obs_data_set_string(h264Settings, "rate_control", "CBR");
		obs_data_set_int(h264Settings, "bitrate", videoBitrate);

		if (advanced) {
			obs_data_set_string(h264Settings, "preset", preset);
			obs_data_set_string(h264Settings, "x264opts", custom);
		}

		obs_data_set_string(aacSettings, "rate_control", "CBR");
		obs_data_set_int(aacSettings, "bitrate", audioBitrate);

		/*obs_service_apply_encoder_settings(GetService(),
		h264Settings, aacSettings);*/

		if (advanced && !enforceBitrate) {
			obs_data_set_int(h264Settings, "bitrate", videoBitrate);
			obs_data_set_int(aacSettings, "bitrate", audioBitrate);
		}

		video_t *video = obs_get_video();
		enum video_format format = video_output_get_format(video);

		if (format != VIDEO_FORMAT_NV12 && format != VIDEO_FORMAT_I420)
			obs_encoder_set_preferred_video_format(_h264Streaming,
				VIDEO_FORMAT_NV12);

		obs_encoder_update(_h264Streaming, h264Settings);
		obs_encoder_update(_aacStreaming, aacSettings);

		obs_data_release(h264Settings);
		obs_data_release(aacSettings);
	}

	void SimpleOut::setupOutputs()
	{
		update();
		obs_encoder_set_video(_h264Streaming, obs_get_video());
		obs_encoder_set_audio(_aacStreaming, obs_get_audio());

		if (_usingRecordingPreset) {
			if (_ffmpegOutput) {
				obs_output_set_media(_fileOutput, obs_get_video(),
					obs_get_audio());
			}
			else {
				obs_encoder_set_video(_h264Recording, obs_get_video());
				obs_encoder_set_audio(_aacRecording, obs_get_audio());
			}
		}
	}

	void SimpleOut::updateRecording()
	{
		updateRecordingSettings();
		setupOutputs();
		if (!_ffmpegOutput)
		{
			obs_output_set_video_encoder(_fileOutput, _h264Recording);
			obs_output_set_audio_encoder(_fileOutput, _aacRecording, 0);
		}
		if (_replayBuffer) {
			obs_output_set_video_encoder(_replayBuffer, _h264Recording);
			obs_output_set_audio_encoder(_replayBuffer, _aacRecording, 0);
		}
	}

	string SimpleOut::generateSpecifiedFilename(const char *extension, bool noSpace, const char *format)
	{
		bool autoRemux = config_get_bool(_main->getBaseConfig(), "Video", "AutoRemux");

		if ((strcmp(extension, "mp4") == 0) && autoRemux)
			extension = "mkv";

		if (!_flag.empty())
		{
			string name(_flag);
			name.append(".").append(extension);
			if (noSpace)
			{
				replace(name.begin(), name.end(), ' ', '_');
			}
			return string(name);
		}
		BPtr<char> filename = os_generate_formatted_filename(extension,
			!noSpace, format);

		return string(filename);
	}



	static void ensure_directory_exists(string &path)
	{
		replace(path.begin(), path.end(), '\\', '/');

		size_t last = path.rfind('/');
		if (last == string::npos)
			return;

		string directory = path.substr(0, last);
		os_mkdirs(directory.c_str());
	}

	void SimpleOut::findBestFilename(string &strPath, bool noSpace)
	{
		int num = 2;

		if (!os_file_exists(strPath.c_str()))
			return;

		const char *ext = strrchr(strPath.c_str(), '.');
		if (!ext)
			return;

		int extStart = int(ext - strPath.c_str());
		for (;;) {
			string testPath = strPath;
			string numStr;

			numStr = noSpace ? "_" : " (";
			numStr += to_string(num++);
			if (!noSpace)
				numStr += ")";

			testPath.insert(extStart, numStr);

			if (!os_file_exists(testPath.c_str())) {
				strPath = testPath;
				break;
			}
		}
	}

	void SimpleOut::remove_reserved_file_characters(string &s)
	{
		replace(s.begin(), s.end(), '/', '_');
		replace(s.begin(), s.end(), '\\', '_');
		replace(s.begin(), s.end(), '*', '_');
		replace(s.begin(), s.end(), '?', '_');
		replace(s.begin(), s.end(), '"', '_');
		replace(s.begin(), s.end(), '|', '_');
		replace(s.begin(), s.end(), ':', '_');
		replace(s.begin(), s.end(), '>', '_');
		replace(s.begin(), s.end(), '<', '_');
	}

	bool SimpleOut::configureRecording(bool updateReplayBuffer)
	{
		const char *path = config_get_string(_main->getBaseConfig(),
			"SimpleOutput", "FilePath");
		const char *format = config_get_string(_main->getBaseConfig(),
			"SimpleOutput", "RecFormat");
		const char *mux = config_get_string(_main->getBaseConfig(), "SimpleOutput",
			"MuxerCustom");
		bool noSpace = config_get_bool(_main->getBaseConfig(), "SimpleOutput",
			"FileNameWithoutSpace");
		const char *filenameFormat = config_get_string(_main->getBaseConfig(), "Output",
			"FilenameFormatting");
		bool overwriteIfExists = config_get_bool(_main->getBaseConfig(), "Output",
			"OverwriteIfExists");
		const char *rbPrefix = config_get_string(_main->getBaseConfig(), "SimpleOutput",
			"RecRBPrefix");
		const char *rbSuffix = config_get_string(_main->getBaseConfig(), "SimpleOutput",
			"RecRBSuffix");
		int rbTime = config_get_int(_main->getBaseConfig(), "SimpleOutput",
			"RecRBTime");
		int rbSize = config_get_int(_main->getBaseConfig(), "SimpleOutput",
			"RecRBSize");

		os_dir_t *dir = path && path[0] ? os_opendir(path) : nullptr;

		if (!dir) {
			throw "recording dir is not exsit";
			blog(LOG_ERROR, "recording dir is not exsit: %s", path);
			return false;
		}

		os_closedir(dir);

		string strPath;
		strPath += path;

		char lastChar = strPath.back();
		if (lastChar != '/' && lastChar != '\\')
			strPath += "/";

		strPath += generateSpecifiedFilename(_ffmpegOutput ? "avi" : format,
			noSpace, filenameFormat);
		ensure_directory_exists(strPath);
		if (!overwriteIfExists)
			findBestFilename(strPath, noSpace);

		obs_data_t *settings = obs_data_create();
		if (updateReplayBuffer) {
			string f;

			if (rbPrefix && *rbPrefix) {
				f += rbPrefix;
				if (f.back() != ' ')
					f += " ";
			}

			f += filenameFormat;

			if (rbSuffix && *rbSuffix) {
				if (*rbSuffix != ' ')
					f += " ";
				f += rbSuffix;
			}

			remove_reserved_file_characters(f);

			obs_data_set_string(settings, "directory", path);
			obs_data_set_string(settings, "format", f.c_str());
			obs_data_set_string(settings, "extension", format);
			obs_data_set_bool(settings, "allow_spaces", !noSpace);
			obs_data_set_int(settings, "max_time_sec", rbTime);
			obs_data_set_int(settings, "max_size_mb",
				_usingRecordingPreset ? rbSize : 0);
		}
		else {
			obs_data_set_string(settings, _ffmpegOutput ? "url" : "path",
				strPath.c_str());
		}

		obs_data_set_string(settings, "muxer_settings", mux);

		if (updateReplayBuffer)
			obs_output_update(_replayBuffer, settings);
		else
			obs_output_update(_fileOutput, settings);

		obs_data_release(settings);
		return true;
	}

	bool SimpleOut::startRecording(const char* flag)
	{
		if (obs_output_active(_fileOutput))
			return false;
		std::unique_lock<std::mutex> lk(s_stateMutex, std::defer_lock);
		lk.try_lock();
		if (SimpleOut::s_state == RecState::Recording || SimpleOut::s_state == RecState::Stopping)
		{
			return false;
		}
		lk.unlock();

		_flag = "";

		if (flag)
		{
			_flag = flag;
		}
		updateRecording();
		if (!configureRecording(false))
			return false;
		if (!obs_output_start(_fileOutput)) {
			blog(LOG_ERROR, "recording obs_output_start failed");
			return false;
		}
		return true;
	}

	void SimpleOut::stopRecording()
	{
		std::unique_lock<std::mutex> lk(s_stateMutex, std::defer_lock);
		lk.try_lock();
		if (SimpleOut::s_state != RecState::Recording)
		{
			return;
		}
		lk.unlock();
		if (!obs_output_active(_fileOutput))
		{
			return;
		}
		obs_output_stop(_fileOutput);
	}
}