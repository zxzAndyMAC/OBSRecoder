#ifndef _OBS_MAIN_H
#define _OBS_MAIN_H

#include "IOBSStudio.h"
#include "OBSCommon.h"
#include "OBSLog.h"
#include "OBSVolControl.h"
#include "OBSSimpleOut.h"
#include "OBSDshowSource.h"
#include "OBSColorSource.h"
#include "OBSAudioEncoders.hpp"
#include "OBSCaptureFrame.h"
#include "OBSRecordingTest.h"
#include <vector>


using namespace std;

namespace OBS {

	class SimpleOut;
	class OBSMain : public IOBS
	{
	public:
		OBSMain();
		~OBSMain();

		/*设置录像的保存路径
		*@parameter dir 路径
		*/
		void								setVideoSafeDir(const char* dir)								override;

		/*设置录像的文件格式，flv，mp4等
		*@parameter format 格式
		*/
		void								setRecFormat(const char* format)								override;

		/*测试设备，检测最佳录像设置*/
		void								testDevice(FPSType style, std::function<void()> cb)				override;

		/*更新摄像头*/
		void								refreshCameras()												override;

		/*设置桌面音响扬声器静音*/
		void								setDesktopAudioMute(bool mute)									override;

		/*设置麦克风静音*/
		void								setAuxAudioMute(bool mute)										override;

		void								setOutputResolution(const Output_Resulution_Show_Name &name)	override;

		void								setAuxAudioDevice(const Aux_Audio_ID &id)						override;
		void								setDesktopAudioDevice(const Desktop_Audio_ID &id)				override;

		void								set_fps(const Frame_Interval &interval)							override;
		void								set_cam_resolution(const Resolution_Index &index)				override;
		void								set_cam_device(const Camera_ID &device_id)						override;
		bool								startRecording(const char* flag)								override;
		void								stopRecording()													override;
		const RecState						getRecordState() const											override;
		const RecState						getCamFrameState() const										override;
		double								getCpuInfo()													override;

		bool								openPreview()													override;
		void								closePreview()													override;

		void								setNoiseParameters(NoiseParameters &np)							override;
		void								setBackgroundColorVisible(bool visible)							override;
		void								setBackgroundColor(int color /*0xFF00FF00*/)					override;
		void								setChromaEnable(bool enable)									override;
		void								setChromaParameters(ChromaKey &ck)								override;

		/*获取每个声道的音量值，并返回当前声道数
		*@parameter atype 设备类型
		*@parameter data 存储声道音量带数组
		*@parameter size 数组大小，默认8
		*return 声道数量
		*/
		const int							getVolumeLevel(AudioType atype, float data[], unsigned size)	override;

		PixelInfo							*getRawData()													override;

		inline const CamerasFPS&			getFPSlists() const												override { return _dshow->getFPSlists(); }
		inline const CameraResolutionLists&	getCameraResolutionLists() const								override { return _dshow->getCameraResolutionLists(); }
		inline const Cameras&				getCameras() const												override { return _dshow->getCameras(); }
		inline const OutputResulutions&		getOutputResolutions() const									override { return _outputresolutions; }
		inline const DesktopAudios&			getDesktopAudios() const										override { return _dektopaudios; }
		inline const AuxAudios&				getAuxAudios() const											override { return _auxaudios; }
	public:
		/*
		*判断当前环境支持的编码器
		*/
		bool		encoderAvailable(const char *encoder);

		bool		resetAudio();
		inline config_t*	getBaseConfig() { return _basicConfig; };
		bool		resetVideo();
		void		resetAudioDevice(const char *sourceId, const char *deviceId, const char *deviceDesc, int channel);

	private:
		static void sourceActivated(void *data, calldata_t *params);
		static void sourceDeactivated(void *data, calldata_t *params);
		static bool centerAlignSelectedItems(obs_scene_t *scene, obs_sceneitem_t *item, void *param);
		
	private:		
		void		init();
		void		setmute(bool mute, const char *id);
		bool		initBasicConfigDefaults();
		void		saveConfig();
		uint32_t	getWindowsVersion();
		string		currentDateTimeString();
		void		initOBSCallbacks();
		void		printSegmentation(const char* name);
		void		deactivateAudioSource(obs_source_t *source);
		void		getConfigFPS(uint32_t &num, uint32_t &den);
		void		getFPSCommon(uint32_t &num, uint32_t &den);
		void		getFPSInteger(uint32_t &num, uint32_t &den);
		void		getFPSFraction(uint32_t &num, uint32_t &den);
		void		getFPSNanoseconds(uint32_t &num, uint32_t &den);
		enum video_format getVideoFormatFromName(const char *name);
		enum obs_scale_type getScaleType(config_t *basicConfig);
		int			attemptToResetVideo(struct obs_video_info *ovi);
		void		initBasicConfigDefaults2();
		void		checkForSimpleModeX264Fallback();
		void		createFirstRunSources();
		bool		hasAudioDevices(const char *source_id);
		void		loadAudioDevices();
		void		loadListValues(obs_property_t *prop, int index);
		void		setResolution(const std::string &res);
		bool		convertResText(const char *res, uint32_t &cx, uint32_t &cy);
		void		resetDownscales(uint32_t cx, uint32_t cy);
		string		resString(uint32_t cx, uint32_t cy);
		void		tryresetVedio();
	private:
		OBSLog						_log;
		config_t					*_basicConfig;
		SimpleOut					*_simpleout;
		obs_scene_t					*_scene;
		DshowSource					*_dshow;
		ColorSource					*_color_source;
		std::vector<VolControl*>	_volumes;
		DesktopAudios				_dektopaudios;
		AuxAudios					_auxaudios;
		OutputResulutions			_outputresolutions;
		RecordingTest				*_test;
		obs_bounds_type				_boundsType;
		OBSCaptureFrame				*_capture_frame;
#ifdef OBS_CPU_INFO
		os_cpu_usage_info_t			*_cpu_info;
#endif // OBS_CPU_INFO

	};
}

#endif // _OBS_MAIN_H
