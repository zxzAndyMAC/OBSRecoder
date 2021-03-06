#ifndef _OBS_DSHOW_H
#define _OBS_DSHOW_H

#include "OBSCommon.h"
#include "IOBSStudio.h"
#include "OBSBaseSource.h"
#include <string>
#include <vector>

using namespace std;

namespace OBS {

	class DshowSource : public BaseSource
	{
	public:
		DshowSource();
		~DshowSource();
	
	public:
		virtual void	create(const char* name, obs_scene_t *scene) override;

		/*设置摄像头*/
		void			set_cam_device(std::string device_id);
		/*设置分辨率*/
		const char*		set_cam_resolution(size_t index);
		/*设置帧率*/
		void			set_fps(long long interval);

		void			setScale(const float &x, const float &y);

		inline const Cameras&					getCameras() const { return _vecCam; }
		inline const CameraResolutionLists&		getCameraResolutionLists() const { return _resolutions; }
		inline const CamerasFPS&				getFPSlists() const { return _fps; }

		inline const string&					getDefaultres() const { return _defaultresl; }
		inline obs_source_t						*getSource() const { return _source; }

		void			setChromaEnable(bool enable);
		void			setChromaParameters(ChromaKey &ck);
	private:
		void			init_cams_info();
		void			get_cam_info(std::string device_id);
		void			get_cam_fps();
		long long		getOBSFPS(int fps);
		const char*	    getFPSName(long long interval);

	private:	
		string					_defaultresl;	
		Cameras					_vecCam;
		CameraResolutionLists   _resolutions;
		CamerasFPS				_fps;
		obs_source_t			*_chroma_key_filter;
	};
}

#endif


