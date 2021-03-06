
#include "OBSCaptureFrame.h"
#include <mutex>
#include <functional>

namespace OBS {
	static std::mutex s_stateMutex;

	RecState OBSCaptureFrame::s_state = RecState::Idle;

	void OBSCaptureFrame::OBSStartCapturing(void *data, calldata_t *params)
	{
		blog(LOG_INFO, "=====================starting Capturing=========================");
		OBSCaptureFrame* so = static_cast<OBSCaptureFrame*>(data);
		std::unique_lock<std::mutex> lk(s_stateMutex, std::defer_lock);
		lk.try_lock();
		OBSCaptureFrame::s_state = RecState::Recording;
	}

	void OBSCaptureFrame::OBSStopCapturing(void *data, calldata_t *params)
	{
		blog(LOG_INFO, "=====================stop Capturing=========================");
		OBSCaptureFrame* so = static_cast<OBSCaptureFrame*>(data);
		std::unique_lock<std::mutex> lk(s_stateMutex, std::defer_lock);
		lk.try_lock();
		OBSCaptureFrame::s_state = RecState::Stopped;
	}

	void OBSCaptureFrame::OBSCaptureStopping(void *data, calldata_t *params)
	{
		blog(LOG_INFO, "=====================stopping Capturing=========================");
		OBSCaptureFrame* so = static_cast<OBSCaptureFrame*>(data);
		std::unique_lock<std::mutex> lk(s_stateMutex, std::defer_lock);
		lk.try_lock();
		OBSCaptureFrame::s_state = RecState::Stopping;
	}

	OBSCaptureFrame::OBSCaptureFrame()
		//:_lk(_dataMutex, std::defer_lock)
	{
		memset(&_pixel_info, 0, sizeof(struct PixelInfo));
		_pixel_info.rgba = NULL;
		_pixel_info.width = 0;
		_pixel_info.height = 0;

		_iacd = ACE_MOT::createIAC();
		_iacd->set("copy_width", &_pixel_info.width);
		_iacd->set("copy_height", &_pixel_info.height);
		_iacd->set("copy_frame_data", &_pixel_info.rgba);

		std::unique_lock<std::mutex> lk(s_stateMutex, std::defer_lock);
		lk.try_lock();
		OBSCaptureFrame::s_state = RecState::Idle;
		//ACE_MOT::getAceMonitor()->set_cb("get_frame_info", std::bind(&OBSCaptureFrame::frame_data_cb, this, std::placeholders::_1));
	}

	OBSCaptureFrame::~OBSCaptureFrame()
	{
		ACE_MOT::releaseIAC(_iacd);
		if (NULL != _pixel_info.rgba)
		{
			bfree(_pixel_info.rgba);
			_pixel_info.rgba = NULL;
		}
	}

	PixelInfo *OBSCaptureFrame::getRawData()
	{
		ACE_MOT::getAceMonitor()->call("copy_frame_info", _iacd);
		return &_pixel_info;
	}

	/*
	void OBSCaptureFrame::frame_data_cb(ACE_MOT::IAceCallData* iacd)
	{
		//std::unique_lock<std::mutex> lk(_dataMutex);
		_lk.lock();
		//_pixel_info.rgba = (uint8_t*)iacd->get("frame_data");
		//_pixel_info.width = *(size_t*)iacd->get("width");
		//_pixel_info.height = *(size_t*)iacd->get("height");
		size_t width = *(size_t*)iacd->get("width");
		size_t height = *(size_t*)iacd->get("height");
		if (NULL == _pixel_info.rgba || _pixel_info.width != width || _pixel_info.height != height)
		{
			_pixel_info.width = width;
			_pixel_info.height = height;
			if (NULL != _pixel_info.rgba)
			{
				bfree(_pixel_info.rgba);
				_pixel_info.rgba = NULL;
			}
	
			_pixel_info.rgba = (uint8_t*)bzalloc(height * width * 4 * sizeof(uint8_t));			
		}
		memset(_pixel_info.rgba, 0, height * width * 4 * sizeof(uint8_t));
		uint8_t* src = (uint8_t*)iacd->get("frame_data");
		memcpy(_pixel_info.rgba, src, height * width * 4 * sizeof(uint8_t));
		_lk.unlock();
	}

	PixelInfo *OBSCaptureFrame::lockRawData()
	{
		//std::unique_lock<std::mutex> lk(_dataMutex);
		_lk.lock();
		return &_pixel_info;
	}

	void OBSCaptureFrame::unlockRawData()
	{
		_lk.unlock();
	}
	*/

	bool OBSCaptureFrame::openPreview()
	{
		if (_frameOutput == nullptr)
		{
			_frameOutput = obs_output_create("ace-frame",
				"simple_frame_output", nullptr, nullptr);
			if (!_frameOutput)
				throw "Failed to create recording output "
				"(ace frame output)";
			obs_output_release(_frameOutput);
			_startCapturing.Connect(obs_output_get_signal_handler(_frameOutput),
				"start", OBSCaptureFrame::OBSStartCapturing, this/*this*/);
			_stopCapturing.Connect(obs_output_get_signal_handler(_frameOutput),
				"stop", OBSCaptureFrame::OBSStopCapturing, this/*this*/);
			_captureStopping.Connect(obs_output_get_signal_handler(_frameOutput),
				"stopping", OBSCaptureFrame::OBSCaptureStopping, this/*this*/);
		}

		std::unique_lock<std::mutex> lk(s_stateMutex, std::defer_lock);
		lk.try_lock();
		if (OBSCaptureFrame::s_state == RecState::Stopping || OBSCaptureFrame::s_state == RecState::Recording)
		{
			return false;
		}
		lk.unlock();

		if (obs_output_active(_frameOutput))
		{
			return false;
		}
		obs_output_set_media(_frameOutput, obs_get_video(), obs_get_audio());
		if (!obs_output_start(_frameOutput)) {
			blog(LOG_ERROR, "capture frame obs_output_start failed");
			return false;
		}


		return true;
	}

	void OBSCaptureFrame::closePreview()
	{
		std::unique_lock<std::mutex> lk(s_stateMutex, std::defer_lock);
		lk.try_lock();
		if (OBSCaptureFrame::s_state != RecState::Recording)
		{
			return;
		}
		lk.unlock();
		if (!obs_output_active(_frameOutput) || _frameOutput == nullptr)
		{
			return;
		}
		obs_output_stop(_frameOutput);
		//obs_output_force_stop(_frameOutput);
	}

	RecState OBSCaptureFrame::getState()
	{
		std::unique_lock<std::mutex> lk(s_stateMutex, std::defer_lock);
		lk.try_lock();
		return OBSCaptureFrame::s_state;
	}
}