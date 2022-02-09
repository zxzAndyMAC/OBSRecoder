#ifndef _ACE_FRAME_PLUGIN_OBS_
#define _ACE_FRAME_PLUGIN_OBS_

#include <obs-module.h>
#include <obs.hpp>
#include <util/threading.h>
#include "iace-monitor.h"

namespace OBSACE {

	class Instance
	{
	public:
		Instance(obs_data_t	*settings, obs_output_t *output);
		~Instance();

		bool start();
		void stop(uint64_t ts);
		void receive_video(struct video_data *frame);
		void copy_frame_data(ACE_MOT::IAceCallData *iacd);
		void encoded_packet(struct encoder_packet *packet);
	private:
		obs_data_t			*_settings;
		obs_output_t		*_output;
		video_scale_info    _scale;
		uint8_t             *_rgba;
		uint8_t				*_I420;
		uint8_t				*_bgra;
		uint32_t			_width;
		uint32_t			_height;
		bool				_res_change;
		bool				_stopping;
		uint64_t			_stopping_time;
		//ACE_MOT::IAceCallData	*_iacd;
	};

	class OBSAceFrame
	{
	public:
		OBSAceFrame();
		~OBSAceFrame();

		static const char *get_name(void *name);
		static void *create(obs_data_t *settings, obs_output_t *output);
		static void destroy(void *data);
		static bool start(void *data);
		static void stop(void *data, uint64_t ts);
		static void receive_video(void *param, struct video_data *frame);
		static void receive_audio(void *param, size_t mix_idx, struct audio_data *frame);
		static void encoded_packet(void *data, struct encoder_packet *packet);
	private:
		obs_output_info		_filter;
	};
}

#endif // _ACE_FRAME_PLUGIN_OBS_
