#ifndef _OBS_VOL_CTRL_H_
#define _OBS_VOL_CTRL_H_

#include "ObsCommon.h"
#include "IOBSStudio.h"
#include <string>
#include <mutex>

namespace OBS {

	class VolControl
	{

	public:
		explicit VolControl(obs_source_t* source);
		~VolControl();

		inline obs_source_t *getSource() const { return _source; }
		inline const char	*getSourceName() const { return _source_name; }

		void setNoiseParameters(NoiseParameters &np);

		/*设置音量
		*@parameter vol 0~100
		*/
		void				setVolume(int vol);

		/*是否静音
		*@parameter muted true静音，false取消静音
		*/
		void				setMuted(bool muted);

		/*获取每个声道的音量值，并返回当前声道数
		*@parameter data 存储声道音量带数组
		*@parameter size 数组大小，默认8
		*return 声道数量
		*/
		const int			getVolumeLevel(float data[], unsigned size = MAX_AUDIO_CHANNELS);

		inline const char	*getID() const { return _id.c_str(); }
	private:
		static void			OBSVolumeChanged(void *param, float db);
		static void			OBSVolumeLevel( void *data,
											const float magnitude[MAX_AUDIO_CHANNELS],
											const float peak[MAX_AUDIO_CHANNELS],
											const float inputPeak[MAX_AUDIO_CHANNELS]);
		static void			OBSVolumeMuted(void *data, calldata_t *calldata);

		void				setLevels(
										const float magnitude[MAX_AUDIO_CHANNELS],
										const float peak[MAX_AUDIO_CHANNELS],
										const float inputPeak[MAX_AUDIO_CHANNELS]);

		void				resetLevels();

		void				calculateBallistics(uint64_t ts, double timeSinceLastRedraw = 0.0);
		void				calculateBallisticsForChannel(int channelNr, uint64_t ts, double timeSinceLastRedraw);
		void				handleChannelCofigurationChange();
		void				detectIdle(uint64_t ts);
		void				setFilters();
		void				addFilter(obs_source_t *&filter, const std::string &id, const std::string &name);
	private:
		obs_source_t		* _source;
		obs_fader_t			*_obs_fader;
		obs_volmeter_t		*_obs_volmeter;
		const char			*_source_name;
		std::mutex			_data_mutex;
		uint64_t			_currentLastUpdateTime;
		float				_currentMagnitude[MAX_AUDIO_CHANNELS];
		float				_currentPeak[MAX_AUDIO_CHANNELS];
		float				_currentInputPeak[MAX_AUDIO_CHANNELS];
		int					_displayNrAudioChannels;
		float				_displayMagnitude[MAX_AUDIO_CHANNELS];
		float				_displayPeak[MAX_AUDIO_CHANNELS];
		float				_displayPeakHold[MAX_AUDIO_CHANNELS];
		uint64_t			_displayPeakHoldLastUpdateTime[MAX_AUDIO_CHANNELS];
		float				_displayInputPeakHold[MAX_AUDIO_CHANNELS];
		uint64_t			_displayInputPeakHoldLastUpdateTime[MAX_AUDIO_CHANNELS];
		uint64_t			_lastRedrawTime;
		double				_peakDecayRate;
		double				_peakHoldDuration;
		double				_inputPeakHoldDuration;
		double				_magnitudeIntegrationTime;
		double				_minimumLevel;
		std::string			_id;
		obs_source_t		*_noise_suppress_filter;
		obs_source_t		*_noise_gate_filter;
	};
}
#endif
