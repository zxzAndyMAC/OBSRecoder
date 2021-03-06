#include <windows.h>

#include <thread>
#include <mutex>
#include <condition_variable>
#include <string>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <functional>
#include <obs.hpp>
#include <graphics/math-extra.h>

#include "OBSRecordingTest.h"
using namespace std;

namespace OBS {

	struct Result {
		int cx;
		int cy;
		int fps_num;
		int fps_den;

		inline Result(int cx_, int cy_, int fps_num_, int fps_den_)
			: cx(cx_), cy(cy_), fps_num(fps_num_), fps_den(fps_den_)
		{
		}
	};

	class TestMode {
		obs_video_info ovi;
		OBSSource source[6];

		static void render_rand(void *, uint32_t cx, uint32_t cy)
		{
			gs_effect_t *solid = obs_get_base_effect(OBS_EFFECT_SOLID);
			gs_eparam_t *randomvals[3] = {
				gs_effect_get_param_by_name(solid, "randomvals1"),
				gs_effect_get_param_by_name(solid, "randomvals2"),
				gs_effect_get_param_by_name(solid, "randomvals3")
			};

			struct vec4 r;

			for (int i = 0; i < 3; i++) {
				vec4_set(&r,
					rand_float(true) * 100.0f,
					rand_float(true) * 100.0f,
					rand_float(true) * 50000.0f + 10000.0f,
					0.0f);
				gs_effect_set_vec4(randomvals[i], &r);
			}

			while (gs_effect_loop(solid, "Random"))
				gs_draw_sprite(nullptr, 0, cx, cy);
		}

	public:
		inline TestMode()
		{
			obs_get_video_info(&ovi);
			obs_add_main_render_callback(render_rand, this);

			for (uint32_t i = 0; i < 6; i++) {
				source[i] = obs_get_output_source(i);
				obs_source_release(source[i]);
				obs_set_output_source(i, nullptr);
			}
		}

		inline ~TestMode()
		{
			for (uint32_t i = 0; i < 6; i++)
				obs_set_output_source(i, source[i]);

			obs_remove_main_render_callback(render_rand, this);
			obs_reset_video(&ovi);
		}

		inline void SetVideo(int cx, int cy, int fps_num, int fps_den)
		{
			obs_video_info newOVI = ovi;

			newOVI.output_width = (uint32_t)cx;
			newOVI.output_height = (uint32_t)cy;
			newOVI.fps_num = (uint32_t)fps_num;
			newOVI.fps_den = (uint32_t)fps_den;

			obs_reset_video(&newOVI);
		}
	};

	/* this is used to estimate the lower bitrate limit for a given
	 * resolution/fps.  yes, it is a totally arbitrary equation that gets
	 * the closest to the expected values */
	long double RecordingTest::EstimateBitrateVal(int cx, int cy, int fps_num, int fps_den)
	{
		long fps = (long double)fps_num / (long double)fps_den;
		long double areaVal = pow((long double)(cx * cy), 0.85l);
		return areaVal * sqrt(pow(fps, 1.1l));
	}

	long double RecordingTest::EstimateMinBitrate(int cx, int cy, int fps_num, int fps_den)
	{
		long double val = EstimateBitrateVal(1920, 1080, 60, 1) / 5800.0l;
		return EstimateBitrateVal(cx, cy, fps_num, fps_den) / val;
	}

	long double RecordingTest::EstimateUpperBitrate(int cx, int cy, int fps_num, int fps_den)
	{
		long double val = EstimateBitrateVal(1280, 720, 30, 1) / 3000.0l;
		return EstimateBitrateVal(cx, cy, fps_num, fps_den) / val;
	}

	void RecordingTest::CalcBaseRes(int &baseCX, int &baseCY)
	{
		const int maxBaseArea = 1920 * 1200;
		const int clipResArea = 1920 * 1080;

		/* if base resolution unusually high, recalculate to a more reasonable
		 * value to start the downscaling at, based upon 1920x1080's area.
		 *
		 * for 16:9 resolutions this will always change the starting value to
		 * 1920x1080 */
		if ((baseCX * baseCY) > maxBaseArea) {
			long double xyAspect =
				(long double)baseCX / (long double)baseCY;
			baseCY = (int)sqrt((long double)clipResArea / xyAspect);
			baseCX = (int)((long double)baseCY * xyAspect);
		}
	}

	void RecordingTest::TestHardwareEncoding()
	{
		size_t idx = 0;
		const char *id;
		while (obs_enum_encoder_types(idx++, &id)) {
			if (strcmp(id, "ffmpeg_nvenc") == 0)
				_hardwareEncodingAvailable = _nvencAvailable = true;
			else if (strcmp(id, "obs_qsv11") == 0)
				_hardwareEncodingAvailable = _qsvAvailable = true;
			else if (strcmp(id, "amd_amf_h264") == 0)
				_hardwareEncodingAvailable = _vceAvailable = true;
		}
	}

	bool RecordingTest::TestSoftwareEncoding()
	{
		TestMode testMode;

		/* -----------------------------------*/
		/* create obs objects                 */

		OBSEncoder vencoder = obs_video_encoder_create("obs_x264",
			"test_x264", nullptr, nullptr);
		OBSEncoder aencoder = obs_audio_encoder_create("ffmpeg_aac",
			"test_aac", nullptr, 0, nullptr);
		OBSOutput output = obs_output_create("null_output",
			"null", nullptr, nullptr);
		obs_output_release(output);
		obs_encoder_release(vencoder);
		obs_encoder_release(aencoder);

		/* -----------------------------------*/
		/* configure settings                 */

		OBSData aencoder_settings = obs_data_create();
		OBSData vencoder_settings = obs_data_create();
		obs_data_release(aencoder_settings);
		obs_data_release(vencoder_settings);
		obs_data_set_int(aencoder_settings, "bitrate", 32);


		obs_data_set_int(vencoder_settings, "crf", 20);
		obs_data_set_string(vencoder_settings, "rate_control", "CRF");
		obs_data_set_string(vencoder_settings, "profile", "high");
		obs_data_set_string(vencoder_settings, "preset", "veryfast");


		/* -----------------------------------*/
		/* apply settings                     */

		obs_encoder_update(vencoder, vencoder_settings);
		obs_encoder_update(aencoder, aencoder_settings);

		/* -----------------------------------*/
		/* connect encoders/services/outputs  */

		obs_output_set_video_encoder(output, vencoder);
		obs_output_set_audio_encoder(output, aencoder, 0);

		/* -----------------------------------*/
		/* connect signals                    */

		auto on_stopped = [&]()
		{
			unique_lock<mutex> lock(_m);
			_cv.notify_one();
		};

		using on_stopped_t = decltype(on_stopped);

		auto pre_on_stopped = [](void *data, calldata_t *)
		{
			on_stopped_t &on_stopped =
				*reinterpret_cast<on_stopped_t*>(data);
			on_stopped();
		};

		signal_handler *sh = obs_output_get_signal_handler(output);
		signal_handler_connect(sh, "deactivate", pre_on_stopped, &on_stopped);

		/* -----------------------------------*/
		/* calculate starting resolution      */

		int baseCX = _baseResolutionCX;
		int baseCY = _baseResolutionCY;
		CalcBaseRes(baseCX, baseCY);

		/* -----------------------------------*/
		/* calculate starting test rates      */

		int pcores = os_get_physical_cores();
		int lcores = os_get_logical_cores();
		int maxDataRate;
		if (lcores > 8 || pcores > 4) {
			/* superb */
			maxDataRate = 1920 * 1200 * 60 + 1000;

		}
		else if (lcores > 4 && pcores == 4) {
			/* great */
			maxDataRate = 1920 * 1080 * 60 + 1000;

		}
		else if (pcores == 4) {
			/* okay */
			maxDataRate = 1920 * 1080 * 30 + 1000;

		}
		else {
			/* toaster */
			maxDataRate = 960 * 540 * 30 + 1000;
		}

		/* -----------------------------------*/
		/* perform tests                      */

		vector<Result> results;
		int i = 0;
		int count = 1;

		auto testRes = [&](long double div, int fps_num, int fps_den,
			bool force)
		{
			/* no need for more than 3 tests max */
			if (results.size() >= 3)
				return true;

			if (!fps_num || !fps_den) {
				fps_num = _specificFPSNum;
				fps_den = _specificFPSDen;
			}

			long double fps = ((long double)fps_num / (long double)fps_den);

			int cx = int((long double)baseCX / div);
			int cy = int((long double)baseCY / div);

			long double rate = (long double)cx * (long double)cy * fps;
			if (!force && rate > maxDataRate)
				return true;

			testMode.SetVideo(cx, cy, fps_num, fps_den);

			obs_encoder_set_video(vencoder, obs_get_video());
			obs_encoder_set_audio(aencoder, obs_get_audio());
			obs_encoder_update(vencoder, vencoder_settings);

			obs_output_set_media(output, obs_get_video(), obs_get_audio());

			unique_lock<mutex> ul(_m);
			if (_cancel)
				return false;

			if (!obs_output_start(output)) {
				blog(LOG_ERROR, "test obs_output_start failed");
				return false;
			}

			_cv.wait_for(ul, chrono::seconds(5));

			obs_output_stop(output);
			_cv.wait(ul);

			int skipped = (int)video_output_get_skipped_frames(
				obs_get_video());
			if (force || skipped <= 10)
				results.emplace_back(cx, cy, fps_num, fps_den);

			return !_cancel;
		};

		if (_specificFPSNum && _specificFPSDen) {
			count = 5;
			if (!testRes(1.0, 0, 0, false)) return false;
			if (!testRes(1.5, 0, 0, false)) return false;
			if (!testRes(1.0 / 0.6, 0, 0, false)) return false;
			if (!testRes(2.0, 0, 0, false)) return false;
			if (!testRes(2.25, 0, 0, true)) return false;
		}
		else {
			count = 10;
			if (!testRes(1.0, 60, 1, false)) return false;
			if (!testRes(1.0, 30, 1, false)) return false;
			if (!testRes(1.5, 60, 1, false)) return false;
			if (!testRes(1.5, 30, 1, false)) return false;
			if (!testRes(1.0 / 0.6, 60, 1, false)) return false;
			if (!testRes(1.0 / 0.6, 30, 1, false)) return false;
			if (!testRes(2.0, 60, 1, false)) return false;
			if (!testRes(2.0, 30, 1, false)) return false;
			if (!testRes(2.25, 60, 1, false)) return false;
			if (!testRes(2.25, 30, 1, true)) return false;
		}

		/* -----------------------------------*/
		/* find preferred settings            */

		int minArea = 960 * 540 + 1000;

		if (!_specificFPSNum && _preferHighFPS && results.size() > 1) {
			Result &result1 = results[0];
			Result &result2 = results[1];

			if (result1.fps_num == 30 && result2.fps_num == 60) {
				int nextArea = result2.cx * result2.cy;
				if (nextArea >= minArea)
					results.erase(results.begin());
			}
		}

		Result result = results.front();
		_idealResolutionCX = result.cx;
		_idealResolutionCY = result.cy;
		_idealFPSNum = result.fps_num;
		_idealFPSDen = result.fps_den;

		long double fUpperBitrate = EstimateUpperBitrate(
			result.cx, result.cy, result.fps_num, result.fps_den);

		int upperBitrate = int(floor(fUpperBitrate / 50.0l) * 50.0l);

		if (_streamingEncoder != Encoder::x264) {
			upperBitrate *= 114;
			upperBitrate /= 100;
		}

		if (_idealBitrate > upperBitrate)
			_idealBitrate = upperBitrate;

		_softwareTested = true;
		return true;
	}

	void RecordingTest::FindIdealHardwareResolution()
	{
		int baseCX = _baseResolutionCX;
		int baseCY = _baseResolutionCY;
		CalcBaseRes(baseCX, baseCY);

		vector<Result> results;

		int pcores = os_get_physical_cores();
		int maxDataRate;
		if (pcores >= 4) {
			maxDataRate = 1920 * 1200 * 60 + 1000;
		}
		else {
			maxDataRate = 1280 * 720 * 30 + 1000;
		}

		auto testRes = [&](long double div, int fps_num, int fps_den,
			bool force)
		{
			if (results.size() >= 3)
				return;

			if (!fps_num || !fps_den) {
				fps_num = _specificFPSNum;
				fps_den = _specificFPSDen;
			}

			long double fps = ((long double)fps_num / (long double)fps_den);

			int cx = int((long double)baseCX / div);
			int cy = int((long double)baseCY / div);

			long double rate = (long double)cx * (long double)cy * fps;
			if (!force && rate > maxDataRate)
				return;

			Encoder encType = _streamingEncoder;
			bool nvenc = encType == Encoder::NVENC;

			int minBitrate = EstimateMinBitrate(cx, cy, fps_num, fps_den);

			/* most hardware encoders don't have a good quality to bitrate
			 * ratio, so increase the minimum bitrate estimate for them.
			 * NVENC currently is the exception because of the improvements
			 * its made to its quality in recent generations. */
			if (!nvenc) minBitrate = minBitrate * 114 / 100;

			force = true;
			if (force || _idealBitrate >= minBitrate)
				results.emplace_back(cx, cy, fps_num, fps_den);
		};

		if (_specificFPSNum && _specificFPSDen) {
			testRes(1.0, 0, 0, false);
			testRes(1.5, 0, 0, false);
			testRes(1.0 / 0.6, 0, 0, false);
			testRes(2.0, 0, 0, false);
			testRes(2.25, 0, 0, true);
		}
		else {
			testRes(1.0, 60, 1, false);
			testRes(1.0, 30, 1, false);
			testRes(1.5, 60, 1, false);
			testRes(1.5, 30, 1, false);
			testRes(1.0 / 0.6, 60, 1, false);
			testRes(1.0 / 0.6, 30, 1, false);
			testRes(2.0, 60, 1, false);
			testRes(2.0, 30, 1, false);
			testRes(2.25, 60, 1, false);
			testRes(2.25, 30, 1, true);
		}

		int minArea = 960 * 540 + 1000;

		if (!_specificFPSNum && _preferHighFPS && results.size() > 1) {
			Result &result1 = results[0];
			Result &result2 = results[1];

			if (result1.fps_num == 30 && result2.fps_num == 60) {
				int nextArea = result2.cx * result2.cy;
				if (nextArea >= minArea)
					results.erase(results.begin());
			}
		}

		Result result = results.front();
		_idealResolutionCX = result.cx;
		_idealResolutionCY = result.cy;
		_idealFPSNum = result.fps_num;
		_idealFPSDen = result.fps_den;
	}

	const char *RecordingTest::GetEncoderId(Encoder enc)
	{
		switch (enc) {
		case Encoder::NVENC:
			return SIMPLE_ENCODER_NVENC;
		case Encoder::QSV:
			return SIMPLE_ENCODER_QSV;
		case Encoder::AMD:
			return SIMPLE_ENCODER_AMD;
		default:
			return SIMPLE_ENCODER_X264;
		}
	};

	void RecordingTest::setcallback(std::function<void()> cb)
	{
		_callback = cb;
	}

	void RecordingTest::testRecordingEncoderThread(config_t *basicConfig, FPSType fpsType)
	{
		if (!_hardwareEncodingAvailable && !_softwareTested) {
			if (!TestSoftwareEncoding()) {
				return;
			}
		}

		if (_hardwareEncodingAvailable)
			FindIdealHardwareResolution();

		_recordingQuality = Quality::High;

		bool recordingOnly = true;

		if (_hardwareEncodingAvailable) {
			if (_nvencAvailable)
				_recordingEncoder = Encoder::NVENC;
			else if (_qsvAvailable)
				_recordingEncoder = Encoder::QSV;
			else
				_recordingEncoder = Encoder::AMD;
		}
		else {
			_recordingEncoder = Encoder::x264;
		}

		if (_recordingEncoder != Encoder::Stream)
			config_set_string(basicConfig, "SimpleOutput", "RecEncoder",
				GetEncoderId(_recordingEncoder));

		const char *quality = _recordingQuality == Quality::High
			? "Small"
			: "Stream";

		config_set_string(basicConfig, "Output", "Mode", "Simple");
		config_set_string(basicConfig, "SimpleOutput", "RecQuality", quality);
		config_set_int(basicConfig, "Video", "BaseCX", _baseResolutionCX);
		config_set_int(basicConfig, "Video", "BaseCY", _baseResolutionCY);
		config_set_int(basicConfig, "Video", "OutputCX", _idealResolutionCX);
		config_set_int(basicConfig, "Video", "OutputCY", _idealResolutionCY);

		if (fpsType != FPSType::UseCurrent) {
			config_set_uint(basicConfig, "Video", "FPSType", 0);
			config_set_string(basicConfig, "Video", "FPSCommon",
				std::to_string(_idealFPSNum).c_str());
		}
		config_save_safe(basicConfig, "tmp", nullptr);
		_callback();
	}

	void RecordingTest::excuteTestThread(config_t *basicConfig, FPSType fpsType, int witdh, int height)
	{
		blog(LOG_INFO, "=========================================================");
		blog(LOG_INFO, "===testing your hardware and software===");
		blog(LOG_INFO, "===it takes few minutes===");
		obs_video_info ovi;
		obs_get_video_info(&ovi);

		if (witdh == 0 || height == 0)
		{
			_baseResolutionCX = ovi.base_width;
			_baseResolutionCY = ovi.base_height;
		}

		switch (fpsType) {
		case FPSType::PreferHighFPS:
			_specificFPSNum = 0;
			_specificFPSDen = 0;
			_preferHighFPS = true;
			break;
		case FPSType::PreferHighRes:
			_specificFPSNum = 0;
			_specificFPSDen = 0;
			_preferHighFPS = false;
			break;
		case FPSType::UseCurrent:
			_specificFPSNum = ovi.fps_num;
			_specificFPSDen = ovi.fps_den;
			_preferHighFPS = false;
			break;
		case FPSType::fps30:
			_specificFPSNum = 30;
			_specificFPSDen = 1;
			_preferHighFPS = false;
			break;
		case FPSType::fps60:
			_specificFPSNum = 60;
			_specificFPSDen = 1;
			_preferHighFPS = false;
			break;
		}

		TestHardwareEncoding();
		_testThread = std::thread(std::mem_fn(&RecordingTest::testRecordingEncoderThread), this, basicConfig, fpsType);
	}

	RecordingTest::RecordingTest()
	{

	}

	RecordingTest::~RecordingTest()
	{
		if (_testThread.joinable()) {
			{
				unique_lock<mutex> ul(_m);
				_cancel = true;
				_cv.notify_one();
			}
			_testThread.join();
		}
	}
}

