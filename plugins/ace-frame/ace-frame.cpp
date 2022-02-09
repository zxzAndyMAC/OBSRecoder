
#include "ace-frame.h"
#include <media-io/video-io.h>
#include <memory>
#include <string>
#include <libyuv.h>

#define FRAME_DATA "frame_data"
#define FRAME_WIDTH "width"
#define FRAME_HEIGHT "height"
#define GET_FRAME_INFO "get_frame_info"

#define COPY_FRAME_INFO "copy_frame_info"
#define COPY_WIDTH	"copy_width"
#define COPY_HEIGHT	"copy_height"
#define COPY_FRAME_DATA "copy_frame_data"

namespace OBSACE {

	static pthread_mutex_t		s_write_mutex;

	OBSAceFrame::OBSAceFrame()
	{
		std::memset(&_filter, 0, sizeof(obs_output_info));

		_filter.id = "ace-frame";
		_filter.flags = OBS_OUTPUT_VIDEO;
		_filter.get_name = get_name;
		_filter.create = create;
		_filter.destroy = destroy;
		_filter.start = start;
		_filter.stop = stop;
		_filter.raw_video = receive_video;
		_filter.encoded_packet = encoded_packet;
		//_filter.raw_audio2 = receive_audio;

		obs_register_output(&_filter);
	}

	OBSAceFrame::~OBSAceFrame()
	{
		
	}

	const char *OBSAceFrame::get_name(void *name)
	{
		return "ACE Frame";
	}

	void OBSAceFrame::encoded_packet(void *ptr, struct encoder_packet *packet)
	{
		Instance* in = static_cast<Instance*>(ptr);
		return in->encoded_packet(packet);
	}

	void *OBSAceFrame::create(obs_data_t *settings, obs_output_t *output)
	{
		return new Instance(settings, output);
	}

	void OBSAceFrame::destroy(void *ptr)
	{
		delete static_cast<Instance*>(ptr);
	}

	bool OBSAceFrame::start(void *ptr)
	{
		Instance* in = static_cast<Instance*>(ptr);
		return in->start();
	}

	void OBSAceFrame::stop(void *ptr, uint64_t ts)
	{
		Instance* in = static_cast<Instance*>(ptr);
		in->stop(ts);
	}

	void OBSAceFrame::receive_video(void *ptr, struct video_data *frame)
	{
		Instance* in = static_cast<Instance*>(ptr);
		in->receive_video(frame);
	}

	void OBSAceFrame::receive_audio(void *param, size_t mix_idx, struct audio_data *frame)
	{

	}

	Instance::Instance(obs_data_t	*settings, obs_output_t *output)
		:_output(output),
		_settings(settings),
		_height(0),
		_width(0),
		_stopping_time(0),
		_stopping(false),
		_res_change(false),
		_rgba(nullptr),
		_I420(nullptr),
		_bgra(nullptr)
	{
		//_iacd = ACE_MOT::createIAC();
		//_iacd->set(FRAME_WIDTH, &_width);
		//_iacd->set(FRAME_HEIGHT, &_height);

		pthread_mutex_init_value(&s_write_mutex);
		if (pthread_mutex_init(&s_write_mutex, NULL) != 0)
			blog(LOG_ERROR, "init pthread_mutex failed");

		std::memset(&_scale, 0, sizeof(video_scale_info));
		_scale.colorspace = video_colorspace::VIDEO_CS_DEFAULT;
		_scale.range = video_range_type::VIDEO_RANGE_DEFAULT;
		_scale.format = video_format::VIDEO_FORMAT_RGBA;
		_scale.width = 1;
		_scale.height = 1;
		//proc_handler_t *ph = obs_output_get_proc_handler(_output);
		//proc_handler_add(ph, "void copy_frame_data()", Instance::copy_frame_data, this);
		ACE_MOT::getAceMonitor()->set_cb(COPY_FRAME_INFO, std::bind(&Instance::copy_frame_data, this, std::placeholders::_1));
	}

	Instance::~Instance()
	{
		ACE_MOT::releaseAceMonitor();
		if (_rgba != nullptr)
		{
			bfree(_rgba);
			_rgba = nullptr;
		}
		//ACE_MOT::releaseIAC(_iacd);

		if (_I420 != nullptr)
		{
			bfree(_I420);
			_I420 = nullptr;
		}

		if (_bgra != nullptr)
		{
			bfree(nullptr);
			_bgra = nullptr;
		}
		pthread_mutex_destroy(&s_write_mutex);
	}

	bool Instance::start()
	{
		/*video_t *video = obs_output_video(_output);
		uint32_t o_width = obs_output_get_width(_output);
		uint32_t o_height = obs_output_get_height(_output);
		uint32_t v_width = video_output_get_width(video);
		uint32_t v_height = video_output_get_height(video);
		video_format format = video_output_get_format(video);*/
		//AVPixelFormat format = obs_to_ffmpeg_video_format(video_output_get_format(video));
		if (!obs_output_can_begin_data_capture(_output, 0))
			return false;
		obs_output_set_video_conversion(_output, NULL);
		obs_output_begin_data_capture(_output, 0);
		return true;
	}

	void Instance::encoded_packet(struct encoder_packet *packet)
	{
		if (_stopping)
		{
			if (packet->sys_dts_usec >= (int64_t)(_stopping_time/1000))
				obs_output_end_data_capture(_output);
		}
		
	}

	void Instance::stop(uint64_t ts)
	{
		if (ts == 0)
		{
			obs_output_end_data_capture(_output);
		}
		else
		{
			_stopping = true;
			_stopping_time = ts;
		}
	}
	
	void Instance::copy_frame_data(ACE_MOT::IAceCallData *iacd)
	{
		pthread_mutex_lock(&s_write_mutex);

		uint8_t **data = (uint8_t**)iacd->get(COPY_FRAME_DATA);
		size_t* width = (size_t*)iacd->get(COPY_WIDTH);
		size_t* height = (size_t*)iacd->get(COPY_HEIGHT);
		
		if (*data == NULL || *width != _width || *height != _height)
		{
			if (*data != NULL)
			{
				bfree(*data);
				*data = NULL;
			}
			*data = (uint8_t*)bzalloc(_height * _width * 4 * sizeof(uint8_t));
		}
		*width = _width;
		*height = _height;


		memset(*data, 0, _height * _width * 4 * sizeof(uint8_t));

		memcpy(*data, _rgba, _height * _width * 4 * sizeof(uint8_t));

		pthread_mutex_unlock(&s_write_mutex);
	}

	void Instance::receive_video(struct video_data *frame)
	{
		if (_stopping)
		{
			if (frame->timestamp >= _stopping_time)
			{
				obs_output_end_data_capture(_output);
				return;
			}
		}

		pthread_mutex_lock(&s_write_mutex);
		struct video_output *video = obs_output_video(_output);

		video_format format = video_output_get_format(video);

		uint32_t v_width = video_output_get_width(video);
		uint32_t v_height = video_output_get_height(video);

		if (_width == 0 || _height == 0)
		{
			_width = v_width;
			_height = v_height;
		}
		else if (_width != v_width || _height!= v_height)
		{
			_width = v_width;
			_height = v_height;
			if (_rgba != nullptr)
			{
				bfree(_rgba);
				_rgba = (uint8_t*)bzalloc(_height * _width * 4 * sizeof(uint8_t));
				//_iacd->set(FRAME_DATA, _rgba);

				bfree(_bgra);
				_bgra = (uint8_t*)bzalloc(_height * _width * 4 * sizeof(uint8_t));

				bfree(_I420);
				_I420 = (uint8_t*)bzalloc((_width * _height * 3) >> 1);
			}
			_res_change = true;
		}
		

		if (_rgba == nullptr)
		{
			_rgba = (uint8_t*)bzalloc(_height * _width * 4 * sizeof(uint8_t));
			//_iacd->set(FRAME_DATA, _rgba);
			//_I420 = (uint8_t*)bzalloc(((_height * _height * 3) >> 1) * sizeof(uint8_t));
			_bgra = (uint8_t*)bzalloc(_height * _width * 4 * sizeof(uint8_t));
			_I420 = (uint8_t*)bzalloc((_width * _height * 3) >> 1);
		}

		
		memset(_I420, 0, (_width * _height * 3) >> 1);
		memcpy(_I420, *frame->data, (_width * _height * 3) >> 1);

		memset(_rgba, 0, _height * _width * 4 * sizeof(uint8_t));
		memset(_bgra, 0, _height * _width * 4 * sizeof(uint8_t));

		//I420 to RGBA
		libyuv::I420ToRGBA(
							&_I420[0],								_width,
							&_I420[_width * _height],				_width >> 1,
							&_I420[(_width * _height * 5) >> 2],	_width >> 1,
							_bgra,									_width * 4,
							_width,	_height
							);

		libyuv::ARGBToBGRA(
							_bgra,									_width * 4,
							_rgba,									_width * 4,
							_width,	_height
							);

		libyuv::ARGBToI420(
							_rgba,									_width * 4,
							&_I420[0],								_width,
							&_I420[_width * _height],				_width >> 1,
							&_I420[(_width * _height * 5) >> 2],	_width >> 1,
							_width,	_height
							);

		//memcpy(_data, frame->data[0], _height * _width * 4 * sizeof(uint8_t));
	
		//ACE_MOT::getAceMonitor()->call(GET_FRAME_INFO, _iacd);
		
		pthread_mutex_unlock(&s_write_mutex);
	}
	
}