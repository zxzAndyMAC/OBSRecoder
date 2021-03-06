#ifndef _OBS_REC_H
#define _OBS_REC_H

#include "OBSCommon.h"
#include "IOBSStudio.h"
#include <mutex>
#include <condition_variable>
#include <functional>

namespace OBS {

	class RecordingTest
	{
	public:
		RecordingTest();
		~RecordingTest();

	public:
		void						setcallback(std::function<void()> cb);
		void						excuteTestThread(config_t *basicConfig, FPSType fpsType, int witdh = 0, int height = 0);

	private:
		void						testRecordingEncoderThread(config_t *basicConfig, FPSType fpsType);
		const char					*GetEncoderId(Encoder enc);
		void						FindIdealHardwareResolution();
		bool						TestSoftwareEncoding();
		void						TestHardwareEncoding();
		void						CalcBaseRes(int &baseCX, int &baseCY);
		long double					EstimateUpperBitrate(int cx, int cy, int fps_num, int fps_den);
		long double					EstimateMinBitrate(int cx, int cy, int fps_num, int fps_den);
		long double					EstimateBitrateVal(int cx, int cy, int fps_num, int fps_den);
	private:
		std::thread					_testThread;
		std::condition_variable		_cv;
		std::mutex					_m;
		std::function<void()>		_callback;

		bool						_hardwareEncodingAvailable = false;
		bool						_nvencAvailable = false;
		bool						_qsvAvailable = false;
		bool						_vceAvailable = false;
		bool						_softwareTested = false;
		bool						_cancel = false;

		Quality						_recordingQuality;
		Encoder						_recordingEncoder;

		int							_baseResolutionCX = 1920;
		int							_baseResolutionCY = 1080;
		int							_idealResolutionCX = 1280;
		int							_idealResolutionCY = 720;
		int							_idealBitrate = 2500;
		int							_idealFPSNum = 60;
		int							_idealFPSDen = 1;

		Encoder						_streamingEncoder = Encoder::x264;

		int							_specificFPSNum = 0;
		int							_specificFPSDen = 0;
		bool						_preferHighFPS = false;
	};
}

#endif // _OBS_REC_H
