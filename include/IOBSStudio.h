#ifndef _I_OBS_H_
#define _I_OBS_H_

#if defined(IOSB_EXPORTS)
#	define IOSB_DLL __declspec(dllexport)
#else
#	define IOSB_DLL __declspec(dllimport)
#endif

#include <string>
#include <vector>
#include <functional>

/*
*	预定义宏 OBS_OPEN_LOG 开启日志打印以及本地持久化
*   预定义宏 OBS_CPU_INFO 收集cpu使用率
*/

namespace OBS {

	/*可用摄像头*/
	typedef std::string Camera_Show_Name;
	typedef std::string Camera_ID;

	typedef std::vector< std::pair<Camera_Show_Name, Camera_ID> > Cameras;

	/*摄像头支持的分辨率，摄像头切换的时候，此容器会变更, 需要刷新显示*/
	typedef size_t		Resolution_Index;
	typedef std::string	Resolution_Show_Name;

	typedef std::vector< std::pair<Resolution_Index, Resolution_Show_Name> > CameraResolutionLists;

	/*当前摄像头当前分辨率下支持的帧率，随着摄像头以及分辨率的改变而改变，需要在切换摄像头或者切换分辨率的时候刷新显示*/
	typedef long long	Frame_Interval;
	typedef std::string FPS_Show_Name;

	typedef std::vector< std::pair<Frame_Interval, FPS_Show_Name> > CamerasFPS;

	/*可用的音箱，扬声器等*/
	typedef std::string Desktop_Audio_ID;
	typedef std::string Desktop_Audio_Show_Name;
	typedef std::vector<std::pair<Desktop_Audio_ID, Desktop_Audio_Show_Name>> DesktopAudios;

	/*可用的麦克风*/
	typedef std::string Aux_Audio_ID;
	typedef std::string Aux_Audio_Show_Name;
	typedef std::vector<std::pair<Aux_Audio_ID, Aux_Audio_Show_Name>> AuxAudios;

	/*视频输出分辨率*/
	typedef std::string Output_Resulution_Show_Name;
	typedef std::vector<Output_Resulution_Show_Name>	OutputResulutions;

	enum class AudioType //audio设备类型
	{
		DeskTop,  //台式音响，扬声器
		AUX      //麦克风
	};

	enum class Quality
	{
		Stream, //与流相同的质量 "Stream"
		High,  //高质量，中等文件大小 "Small"
		VeryHigh, //近乎无损质量，大文件大小 "HQ"
		SuperHigh //无损质量，变态大小，不考虑 "Lossless"
	};

	enum class Encoder {
		x264,		//软编码x264
		NVENC,		//N卡硬件编码nvenc
		QSV,		//硬编码QSV
		AMD,		//硬编码AMD
		Stream
	};

	enum class FPSType : int {
		PreferHighFPS,//60 或者 30，但尽可能选择60
		PreferHighRes,//60 或者 30，但优先选择高分辨率
		UseCurrent,//使用当前默认
		fps30,//30帧
		fps60//60帧
	};

	enum class RecState {
		Idle,
		Recording,
		Stopping,
		Stopped
	};

	class IOBS;

	typedef struct PixelInfo
	{
		uint8_t	*rgba;
		size_t   width;
		size_t   height;
	}PixelInfo;

	/*绿布抠像相关结构图*/
	typedef struct ChromaKey
	{
		/*
		* 关键的颜色类型
		* 英文描述：Key Color Type
		* 列表展示："green" "blue" "magenta" "custom"，分别为绿色，蓝色，品红，自定义，初始默认值绿色
		*/
		std::string key_color_type = "green";

		/*
		* 关键的颜色,只有在key_color_type设置为“custom”的时候才生效,否则隐藏此项
		* 英文描述：Key Color Type
		* 列表展示："green" "blue" "magenta" "custom"，分别为绿色，蓝色，品红，自定义，初始默认值绿色
		*/
		int	key_color = 0x00FF00;

		/*
		* 相似度
		* 英文描述：Similarity (1-1000)
		* range: 1~1000   初始化默认内部设置值为400
		*/
		int similarity = 400;

		/*
		* 平滑
		* 英文描述：Smoothness (1-1000)
		* range: 1~1000   初始化默认内部设置值为80
		*/
		int smoothness = 80;

		/*
		* 平滑
		* 英文描述：Key Color Spill Reduction (1-1000)
		* range: 1~1000   初始化默认内部设置值为100
		*/
		int spill = 100;

		/*
		* 不透明度
		* 英文描述：Opacity
		* range: 0~100   初始化默认内部设置值为100
		*/
		int opacity = 100;

		/*
		* 对比度
		* 英文描述：Contrast
		* range:  -1.00 ~ 1.00   初始化默认内部设置值为0
		*/
		double contrast = 0.00f;

		/*
		* 亮度
		* 英文描述：Brightness
		* range:  -1.00 ~ 1.00   初始化默认内部设置值为0
		*/
		double brightness = 0.00f;

		/*
		* 伽马
		* 英文描述：Gamma
		* range:  -1.00 ~ 1.00   初始化默认内部设置值为0
		*/
		double gamma = 0.00f;
	}ChromaKey;

	/*噪声抑制相关结构体*/
	typedef struct NoiseParameters
	{
		/*
		* 设置噪声抑制程度
		* 英文描述：Suppression Level (dB)
		* range: -60 ~ 0   初始化默认内部设置值为-30
		*/
		int		suppress_level = -30;

		/*
		* 设置噪声关闭阈值
		* 英文描述：Close Threshold (dB)
		* range: -96.00 ~ 0.00   初始化默认内部设置值为-32.00
		*/
		double	close_threshold = -32.00f;

		/*
		* 设置噪声打开阈值
		* 英文描述：Open Threshold (dB)
		* range: -96.00 ~ 0.00   初始化默认内部设置值为-26.00
		*/
		double	open_threshold = -26.00f;

		/*
		* 设置噪声阈值触发时间（毫秒）
		* 英文描述：Attack Time (milliseconds)
		* range: 0~10000   初始化默认内部设置值为25
		*/
		int		attack_time = 25;

		/*
		* 设置噪声阈值保持时间（毫秒）
		* 英文描述：Hold Time (milliseconds)
		* range: 0~10000   初始化默认内部设置值为200
		*/
		int		hold_time = 200;

		/*
		* 设置噪声阈值释放时间（毫秒）
		* 英文描述：Release Time (milliseconds)
		* range: time  0~10000   初始化默认内部设置值为150
		*/
		int		release_time = 150;
	}NoiseParameters;


	class IOSB_DLL OBSStudio
	{
	public:
		static IOBS* createInstance();
		static IOBS* getInstance();
		static void  release(); //程序退出时调用
	};

	class IOSB_DLL IOBS
	{
	public:

		/*开启关闭预览模式*/
		virtual bool								openPreview() = 0;
		virtual void								closePreview()= 0;

		/*获取视频帧数据*/
		virtual PixelInfo							*getRawData() = 0;

		/*取得cpu使用率*/
		virtual double								getCpuInfo() = 0;

		/************************************************************************/
		/*                                 录像相关		                       */
		/************************************************************************/

		/*
		* 开始录像
		* @parameter flag 标记，命名,和设置的录像文件格式组合成最终文件名
		*/
		virtual bool								startRecording(const char* flag) = 0;

		/*
		* 停止录像
		*/
		virtual void								stopRecording() = 0;

		/*
		* 获取当前录像状态
		*/
		virtual const RecState						getRecordState() const = 0;

		/*
		* 获取当前截取录像帧状态
		*/
		virtual const RecState						getCamFrameState() const = 0;

		/*
		* 设置录像的文件格式，flv，mp4等
		* @parameter format 格式
		*/
		virtual void								setRecFormat(const char* format) = 0;

		/*
		* 测试设备，检测最佳录像设置
		* @parameter FPSType 设置帧率
		* @parameter cb_whenfinish 检测结束的回调
		*/
		virtual void								testDevice(FPSType style, std::function<void()> cb_whenfinish) = 0;

		/*
		* 设置录像的保存路径
		* @parameter dir 路径
		*/
		virtual void								setVideoSafeDir(const char* dir) = 0;

		/*
		* 设置视频输出分辨率
		* @parameter Output_Resulution_Show_Name eg. "1280x720"
		*/
		virtual void								setOutputResolution(const Output_Resulution_Show_Name &name) = 0;

		/*
		* 获取支持的视频输出分辨率组合，每次改变摄像头分辨率，这个组合都会变，需要刷新显示
		*/
		virtual const OutputResulutions&			getOutputResolutions() const = 0;


		/************************************************************************/
		/*                                 摄像头相关                           */
		/************************************************************************/

		/*
		* 返回可用摄像头数组<Camera_Show_Name, Camera_ID>
		*/
		virtual const Cameras&						getCameras() const = 0;

		/*
		* 返回当前摄像头支持的分辨率数组<Resolution_Index, Resolution_Show_Name>
		*/
		virtual const CameraResolutionLists&		getCameraResolutionLists() const = 0;

		/*
		* 返回当前摄像头以及当前分辨率下支持的帧率数组<Frame_Interval, FPS_Show_Name>
		*/
		virtual const CamerasFPS&					getFPSlists() const = 0;

		/*
		* 设置摄像头，设置后需要重新获取支持的分辨率组合和支持的帧率
		* @parameter device_id  摄像头id，通过getCameras()返回获取
		*/
		virtual void								set_cam_device(const Camera_ID &device_id) = 0;

		/*
		* 设置分辨率,设置后需要刷新显示支持的帧率组和支持的输出分辨率组
		*/
		virtual void								set_cam_resolution(const Resolution_Index &index) = 0;

		/*
		* 设置帧率
		*/
		virtual	void								set_fps(const Frame_Interval &interval) = 0;

		/*
		* 更新摄像头，如果有外接新摄像头
		*/
		virtual void								refreshCameras() = 0;

		/************************************************************************/
		/*                                 音频设备相关                         */
		/************************************************************************/

		/*
		* 获取可用的音响，扬声器设备等
		*/
		virtual const DesktopAudios&				getDesktopAudios() const = 0;

		/*
		* 获取可用的麦克风设备
		*/
		virtual const AuxAudios&					getAuxAudios() const = 0;

		/*
		* 设置扬声器
		*/
		virtual void								setDesktopAudioDevice(const Desktop_Audio_ID &id) = 0;

		/*
		* 设置麦克风
		*/
		virtual void								setAuxAudioDevice(const Aux_Audio_ID &id) = 0;

		/*
		* 设置麦克风静音
		*/
		virtual void								setAuxAudioMute(bool mute) = 0;

		/*
		* 设置桌面音响扬声器静音
		*/
		virtual void								setDesktopAudioMute(bool mute) = 0;

		/*
		* 获取每个声道的音量值，并返回当前声道数, 音量有效值-60~0，小于-60直接忽略
		* @parameter atype 设备类型
		* @parameter data 存储声道音量带数组
		* @parameter size 数组大小，默认8
		* return 声道数量
		*/
		virtual const int							getVolumeLevel(AudioType atype, float data[], unsigned size/* = 8*/) = 0;

		/************************************************************************/
		/*                                 噪声相关								*/
		/************************************************************************/

		virtual void								setNoiseParameters(NoiseParameters &np) = 0;

		/************************************************************************/
		/*                           抠像+背景色                                */
		/************************************************************************/

		/*
		* 开启关闭背景色
		*/
		virtual void								setBackgroundColorVisible(bool visible) = 0;

		/*
		* 设置背景色
		*/
		virtual void								setBackgroundColor(int color /*0xFF00FF00*/) = 0;

		/*
		* 开启关闭色度键，绿布抠像
		*/
		virtual void								setChromaEnable(bool enable) = 0;

		/*
		* 设置绿布抠像参数
		*/
		virtual void								setChromaParameters(ChromaKey &ck) = 0;
	};
}

#endif // _I_OBS_H_
