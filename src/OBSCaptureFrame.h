
#ifndef _OBS_CAPTURE_OUT_H_
#define _OBS_CAPTURE_OUT_H_

#include "OBSCommon.h"
#include "IOBSStudio.h"
#include <mutex>

namespace OBS {

	class OBSCaptureFrame
	{

	public:
		OBSCaptureFrame();
		~OBSCaptureFrame();

		//PixelInfo	*lockRawData();
		//void		unlockRawData();
		PixelInfo	*getRawData();
		bool		openPreview();
		void		closePreview();
		RecState    getState();
	private:
		static void OBSCaptureStopping(void *data, calldata_t *params);
		static void OBSStopCapturing(void *data, calldata_t *params);
		static void OBSStartCapturing(void *data, calldata_t *params);

		//void        frame_data_cb(ACE_MOT::IAceCallData* iacd);

	private:
		static RecState					s_state;

		OBSOutput						_frameOutput;
		OBSSignal						_startCapturing;
		OBSSignal						_stopCapturing;
		OBSSignal						_captureStopping;
		PixelInfo						_pixel_info;
		ACE_MOT::IAceCallData			*_iacd;
	};
}
#endif