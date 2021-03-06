#ifndef _OBS_COMMON_H_
#define _OBS_COMMON_H_

#include <obs.h>
#include <obs.hpp>
#include <obs-module.h>
#include <util/util.hpp>
#include <util/config-file.h>
#include <util/platform.h>
#include <util/windows/win-version.h>
#include <util/dstr.h>
#include <util/lexer.h>
#include <iace-monitor.h>

namespace OBS {

	struct BaseLexer {
		lexer lex;
	public:
		inline BaseLexer() { lexer_init(&lex); }
		inline ~BaseLexer() { lexer_free(&lex); }
		operator lexer*() { return &lex; }
	};

#define CLAMP(x, min, max) ((x) < (min) ? (min) : ((x) > (max) ? (max) : (x)))
#define MAKE_DSHOW_FPS(fps)                 (10000000LL/(fps))
#define MAKE_DSHOW_FRACTIONAL_FPS(den, num) ((num)*10000000LL/(den))
#define FPS_HIGHEST   0LL
#define FPS_MATCHING -1LL

#define VOLUME_METER_DECAY_FAST        23.53
#define VOLUME_METER_DECAY_MEDIUM      11.76
#define VOLUME_METER_DECAY_SLOW        8.57

#define NOISE_SUPPRESS_FILTER				    "noise_suppress_filter"//噪声抑制
#define N_SUPPRESS_LEVEL					    "suppress_level" //默认值-30,  -60~0

#define NOISE_GATE_FILTER					    "noise_gate_filter"//噪声阈值
#define N_OPEN_THRESHOLD						"open_threshold" //默认值-26   -96~0
#define N_CLOSE_THRESHOLD						"close_threshold"//默认值-32	-96~0
#define N_ATTACK_TIME							"attack_time"//默认值25ms   0~10000
#define N_HOLD_TIME								"hold_time"//默认值200ms	0~10000
#define N_RELEASE_TIME							"release_time"//默认值150ms	0~10000

#define CK_SETTING_OPACITY                "opacity"
#define CK_SETTING_CONTRAST               "contrast"
#define CK_SETTING_BRIGHTNESS             "brightness"
#define CK_SETTING_GAMMA                  "gamma"
#define CK_SETTING_COLOR_TYPE             "key_color_type"
#define CK_SETTING_KEY_COLOR              "key_color"
#define CK_SETTING_SIMILARITY             "similarity"
#define CK_SETTING_SMOOTHNESS             "smoothness"
#define CK_SETTING_SPILL                  "spill"

#define SIMPLE_ENCODER_X264                    "x264"
#define SIMPLE_ENCODER_X264_LOWCPU             "x264_lowcpu"
#define SIMPLE_ENCODER_QSV                     "qsv"
#define SIMPLE_ENCODER_NVENC                   "nvenc"
#define SIMPLE_ENCODER_AMD                     "amd"

#define INPUT_AUDIO_SOURCE  "wasapi_input_capture"
#define OUTPUT_AUDIO_SOURCE "wasapi_output_capture"

#define DSHOW_SOURCE "dshow_input"
#define DSHOW_SOURCE_CHANNEL 0

#define COLOR_SOURCE "color_source"
#define COLOR_SOURCE_CHANNEL 2

#define DESKTOP_AUDIO_CHANNEL 1
#define AUX_AUDIO_CHANNEL 3

#define STARTUP_SEPARATOR \
	"==== Startup complete ==============================================="
#define SHUTDOWN_SEPARATOR \
	"==== Shutting down =================================================="

#define UNSUPPORTED_ERROR \
	"Failed to initialize video:\n\nRequired graphics API functionality " \
	"not found.  Your GPU may not be supported."

#define UNKNOWN_ERROR \
	"Failed to initialize video.  Your GPU may not be supported, " \
	"or your graphics drivers may need to be updated."

#define MAX_REPEATED_LINES 30
#define MAX_CHAR_VARIATION (255 * 3)

#define CROSS_DIST_CUTOFF 2000.0

//#define OBS_USE_EXEDIR 1 //是否将日志存放在exe同级目录下

	static char *GetConfigPathPtr(const char *name)
	{
		return os_get_config_path_ptr(name);
	}

#define OBS_FLAG "ACEkid_OBS"

#if OBS_USE_EXEDIR == 0
#define OBS_LOGS_DIR GetConfigPathPtr(OBS_FLAG)
#define OBS_LOG(log) GetConfigPathPtr(OBS_FLAG##log)
#define OBS_CRASH(crash) GetConfigPathPtr(OBS_FLAG##crash)
#define OBS_CONFIG_DIR(flag) GetConfigPathPtr(OBS_FLAG##flag)
#define OBS_VIDEO_DIR(flag)  GetConfigPathPtr(OBS_FLAG##flag)
#else
#define OBS_LOGS_DIR OBS_FLAG
#define OBS_LOG(log) OBS_FLAG##log
#define OBS_CRASH(crash) OBS_FLAG##crash
#define OBS_CONFIG_DIR(flag) OBS_FLAG##flag
#define OBS_VIDEO_DIR(flag) OBS_FLAG##flag
#endif

}

#endif
