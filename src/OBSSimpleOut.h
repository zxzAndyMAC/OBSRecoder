
#ifndef _SIMPLE_OUT_H_
#define _SIMPLE_OUT_H_

#include "IOBSStudio.h"
#include "OBSAudioEncoders.hpp"
#include "OBSMain.h"
#include <string>

using namespace std;

namespace OBS {

	class OBSMain;
	class SimpleOut
	{
	public:
		SimpleOut(OBSMain* m);
		~SimpleOut();

		void		simpleoutputInit();
		bool		startRecording(const char* flag = NULL);
		void		stopRecording();
		RecState	getState();

	private:
		bool		createAACEncoder(OBSEncoder &res, string &id, int bitrate, const char *name, size_t idx);
		void		loadStreamingPreset_h264(const char *encoderId);
		int			getAudioBitrate();
		void		loadRecordingPreset_Lossless();
		void		loadRecordingPreset_h264(const char *encoderId);
		void        loadRecordingPreset();
		void		recordingInit();
		void		updateRecording();
		void		setupOutputs();
		void		updateRecordingSettings();
		bool		configureRecording(bool updateReplayBuffer);
		void		remove_reserved_file_characters(string &s);
		void		findBestFilename(string &strPath, bool noSpace);
		int			calcCRF(int crf);
		bool		icq_available(obs_encoder_t *encoder);
		void		updateRecordingSettings_qsv11(int crf);
		void		updateRecordingSettings_x264_crf(int crf);
		void		updateRecordingSettings_amd_cqp(int cqp);
		void		updateRecordingSettings_nvenc(int cqp);
		void		updateStreamingSettings_amd(obs_data_t *settings, int bitrate);
		void		update();
		string		generateSpecifiedFilename(const char *extension, bool noSpace, const char *format);

		static void OBSStartRecording(void *data, calldata_t *params);
		static void OBSStopRecording(void *data, calldata_t *params);
		static void OBSRecordStopping(void *data, calldata_t *params);
	private:
		static RecState    s_state;
		OBSEncoder	_h264Streaming;
		OBSEncoder	_aacRecording;
		OBSEncoder	_aacStreaming;
		OBSEncoder	_h264Recording;
		OBSOutput	_fileOutput;
		OBSOutput	_replayBuffer;
		OBSSignal	_startRecording;
		OBSSignal	_stopRecording;
		OBSSignal	_recordStopping;
		string		_aacStreamEncID;
		string		_aacRecEncID;
		string		_videoEncoder;
		string		_videoQuality;
		bool		_ffmpegOutput = false;
		bool		_usingRecordingPreset = false;
		bool		_lowCPUx264 = false;
		string		_flag;
		OBSMain*	_main;
	};

}

#endif
