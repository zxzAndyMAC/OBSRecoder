
#include "OBSVolControl.h"

namespace OBS {

	void VolControl::OBSVolumeChanged(void *data, float db)
	{
		VolControl *volControl = static_cast<VolControl*>(data);

		blog(LOG_INFO, "Source %s: volume change to:%f", volControl->getSourceName(), db);
	}

	void VolControl::OBSVolumeLevel(void *data,
		const float magnitude[MAX_AUDIO_CHANNELS],
		const float peak[MAX_AUDIO_CHANNELS],
		const float inputPeak[MAX_AUDIO_CHANNELS])
	{
		VolControl *volControl = static_cast<VolControl*>(data);

		volControl->setLevels(magnitude, peak, inputPeak);
	}

	void VolControl::OBSVolumeMuted(void *data, calldata_t *calldata)
	{
		VolControl *volControl = static_cast<VolControl*>(data);
		bool muted = calldata_bool(calldata, "muted");

		const char* str = muted ? "muted" : "unmuted";
		blog(LOG_INFO, "Source %s: %s", volControl->getSourceName(), str);
	}

	VolControl::VolControl(obs_source_t* source)
		:_source(source),
		_noise_suppress_filter(nullptr),
		_noise_gate_filter(nullptr),
		_obs_fader(obs_fader_create(OBS_FADER_CUBIC)),
		_obs_volmeter(obs_volmeter_create(OBS_FADER_LOG)),
		_currentLastUpdateTime(0),
		_displayNrAudioChannels(0),
		_lastRedrawTime(0),
		_peakDecayRate(23.53),                              //  20 dB / 1.7 sec
		_peakHoldDuration(20.0),                          //  20 seconds
		_inputPeakHoldDuration(1.0),                        //  1 second
		_magnitudeIntegrationTime(0.3),                     //  99% in 300 ms
		_minimumLevel(-60.0)                               // -60 dB
	{
		_source_name = obs_source_get_name(source);
		_id = obs_source_get_id(source);

		obs_fader_add_callback(_obs_fader, OBSVolumeChanged, this);
		obs_volmeter_add_callback(_obs_volmeter, OBSVolumeLevel, this);

		signal_handler_connect(obs_source_get_signal_handler(_source),
			"mute", OBSVolumeMuted, this);

		obs_fader_attach_source(_obs_fader, _source);
		obs_volmeter_attach_source(_obs_volmeter, _source);

		obs_volmeter_set_peak_meter_type(_obs_volmeter, SAMPLE_PEAK_METER);

		resetLevels();

		if (strcmp(_id.c_str(), OUTPUT_AUDIO_SOURCE) == 0)
		{
			setMuted(true);
		}

		setFilters();
	}

	VolControl::~VolControl()
	{
		if (_noise_suppress_filter != nullptr)
		{
			obs_source_filter_remove(_source, _noise_suppress_filter);
		}

		if (_noise_gate_filter != nullptr)
		{
			obs_source_filter_remove(_source, _noise_gate_filter);
		}
		
		obs_fader_remove_callback(_obs_fader, OBSVolumeChanged, this);
		obs_volmeter_remove_callback(_obs_volmeter, OBSVolumeLevel, this);

		signal_handler_disconnect(obs_source_get_signal_handler(_source),
			"mute", OBSVolumeMuted, this);

		obs_fader_destroy(_obs_fader);
		obs_volmeter_destroy(_obs_volmeter);
	}

	void VolControl::setFilters()
	{
		if (strcmp(_id.c_str(), INPUT_AUDIO_SOURCE) == 0)
		{
			addFilter(_noise_suppress_filter, NOISE_SUPPRESS_FILTER, "noise_suppress");
			addFilter(_noise_gate_filter, NOISE_GATE_FILTER, "noise_gate");
		}
	}

	void VolControl::addFilter(obs_source_t *&filter, const std::string &id, const std::string &name)
	{
		if (filter != nullptr)
		{
			return;
		}

		filter = obs_source_create(id.c_str(), name.c_str(),
			nullptr, nullptr);
		if (filter) {
			const char *sourceName = obs_source_get_name(_source);

			blog(LOG_INFO, "User added filter '%s' (%s) "
				"to source '%s'",
				name.c_str(), id, sourceName);

			obs_source_filter_add(_source, filter);
			obs_source_release(filter);
		}
	}

	void VolControl::setVolume(int vol)
	{
		obs_fader_set_deflection(_obs_fader, float(vol) * 0.01f);
	}

	void VolControl::setMuted(bool muted)
	{
		obs_source_set_muted(_source, muted);
	}

	void VolControl::setLevels(const float magnitude[MAX_AUDIO_CHANNELS],
		const float peak[MAX_AUDIO_CHANNELS],
		const float inputPeak[MAX_AUDIO_CHANNELS])
	{
		uint64_t ts = os_gettime_ns();

		std::lock_guard<std::mutex> locker(_data_mutex);

		_currentLastUpdateTime = ts;
		for (int channelNr = 0; channelNr < MAX_AUDIO_CHANNELS; channelNr++) {
			_currentMagnitude[channelNr] = magnitude[channelNr];
			_currentPeak[channelNr] = peak[channelNr];
			_currentInputPeak[channelNr] = inputPeak[channelNr];
		}

		// In case there are more updates then redraws we must make sure
		// that the ballistics of peak and hold are recalculated.
		calculateBallistics(ts);
	}

	void VolControl::calculateBallistics(uint64_t ts, double timeSinceLastRedraw)
	{
		for (int channelNr = 0; channelNr < MAX_AUDIO_CHANNELS; channelNr++)
			calculateBallisticsForChannel(channelNr, ts, timeSinceLastRedraw);
	}

	void VolControl::calculateBallisticsForChannel(int channelNr, uint64_t ts, double timeSinceLastRedraw)
	{
		if (_currentPeak[channelNr] >= _displayPeak[channelNr] ||
			isnan(_displayPeak[channelNr])) {
			// Attack of peak is immediate.
			_displayPeak[channelNr] = _currentPeak[channelNr];
		}
		else {
			// Decay of peak is 40 dB / 1.7 seconds for Fast Profile
			// 20 dB / 1.7 seconds for Medium Profile (Type I PPM)
			// 24 dB / 2.8 seconds for Slow Profile (Type II PPM)
			float decay = float(_peakDecayRate * timeSinceLastRedraw);
			_displayPeak[channelNr] = CLAMP(_displayPeak[channelNr] - decay,
				_currentPeak[channelNr], 0);
		}

		if (_currentPeak[channelNr] >= _displayPeakHold[channelNr] ||
			!isfinite(_displayPeakHold[channelNr])) {
			// Attack of peak-hold is immediate, but keep track
			// when it was last updated.
			_displayPeakHold[channelNr] = _currentPeak[channelNr];
			_displayPeakHoldLastUpdateTime[channelNr] = ts;
		}
		else {
			// The peak and hold falls back to peak
			// after 20 seconds.
			double timeSinceLastPeak = (uint64_t)(ts -
				_displayPeakHoldLastUpdateTime[channelNr]) * 0.000000001;
			if (timeSinceLastPeak > _peakHoldDuration) {
				_displayPeakHold[channelNr] = _currentPeak[channelNr];
				_displayPeakHoldLastUpdateTime[channelNr] = ts;
			}
		}

		if (_currentInputPeak[channelNr] >= _displayInputPeakHold[channelNr] ||
			!isfinite(_displayInputPeakHold[channelNr])) {
			// Attack of peak-hold is immediate, but keep track
			// when it was last updated.
			_displayInputPeakHold[channelNr] = _currentInputPeak[channelNr];
			_displayInputPeakHoldLastUpdateTime[channelNr] = ts;
		}
		else {
			// The peak and hold falls back to peak after 1 second.
			double timeSinceLastPeak = (uint64_t)(ts -
				_displayInputPeakHoldLastUpdateTime[channelNr]) *
				0.000000001;
			if (timeSinceLastPeak > _inputPeakHoldDuration) {
				_displayInputPeakHold[channelNr] =
					_currentInputPeak[channelNr];
				_displayInputPeakHoldLastUpdateTime[channelNr] =
					ts;
			}
		}

		if (!isfinite(_displayMagnitude[channelNr])) {
			// The statements in the else-leg do not work with
			// NaN and infinite displayMagnitude.
			_displayMagnitude[channelNr] = _currentMagnitude[channelNr];
		}
		else {
			// A VU meter will integrate to the new value to 99% in 300 ms.
			// The calculation here is very simplified and is more accurate
			// with higher frame-rate.
			float attack = float((_currentMagnitude[channelNr] -
				_displayMagnitude[channelNr]) *
				(timeSinceLastRedraw /
					_magnitudeIntegrationTime) * 0.99);
			_displayMagnitude[channelNr] = CLAMP(_displayMagnitude[channelNr]
				+ attack, (float)_minimumLevel, 0);
		}
	}

	void VolControl::resetLevels()
	{
		_currentLastUpdateTime = 0;
		for (int channelNr = 0; channelNr < MAX_AUDIO_CHANNELS; channelNr++) {
			_currentMagnitude[channelNr] = -M_INFINITE;
			_currentPeak[channelNr] = -M_INFINITE;
			_currentInputPeak[channelNr] = -M_INFINITE;

			_displayMagnitude[channelNr] = -M_INFINITE;
			_displayPeak[channelNr] = -M_INFINITE;
			_displayPeakHold[channelNr] = -M_INFINITE;
			_displayPeakHoldLastUpdateTime[channelNr] = 0;
			_displayInputPeakHold[channelNr] = -M_INFINITE;
			_displayInputPeakHoldLastUpdateTime[channelNr] = 0;
		}
	}

	void VolControl::handleChannelCofigurationChange()
	{

		std::lock_guard<std::mutex> locker(_data_mutex);

		int currentNrAudioChannels = obs_volmeter_get_nr_channels(_obs_volmeter);
		if (_displayNrAudioChannels != currentNrAudioChannels) {
			_displayNrAudioChannels = currentNrAudioChannels;
			resetLevels();
		}
	}

	void VolControl::detectIdle(uint64_t ts)
	{
		double timeSinceLastUpdate = (ts - _currentLastUpdateTime) * 0.000000001;
		if (timeSinceLastUpdate > 0.5)
			resetLevels();
	}

	const int VolControl::getVolumeLevel(float data[], unsigned size)
	{
		uint64_t ts = os_gettime_ns();
		double timeSinceLastRedraw = (ts - _lastRedrawTime) * 0.000000001;

		handleChannelCofigurationChange();

		std::lock_guard<std::mutex> locker(_data_mutex);

		calculateBallistics(ts, timeSinceLastRedraw);
		detectIdle(ts);

		for (int channelNr = 0; channelNr < MAX_AUDIO_CHANNELS; channelNr++)
		{
			data[channelNr] = _displayPeak[channelNr];
		}

		_lastRedrawTime = ts;
		return _displayNrAudioChannels;
	}

	void VolControl::setNoiseParameters(NoiseParameters &np)
	{

		if (_noise_suppress_filter != nullptr)
		{
			obs_data_t *settings = obs_source_get_settings(_noise_suppress_filter);

			obs_data_set_int(settings, N_SUPPRESS_LEVEL, np.suppress_level);

			obs_source_update(_noise_suppress_filter, settings);

			obs_data_release(settings);
		}
		

		if (_noise_gate_filter != nullptr)
		{
			obs_data_t *gate_settings = obs_source_get_settings(_noise_gate_filter);
			
			obs_data_set_double(gate_settings, N_CLOSE_THRESHOLD, np.close_threshold);		
			obs_data_set_double(gate_settings, N_OPEN_THRESHOLD, np.open_threshold);		
			obs_data_set_int(gate_settings, N_ATTACK_TIME, np.attack_time);	
			obs_data_set_int(gate_settings, N_RELEASE_TIME, np.release_time);
			obs_data_set_int(gate_settings, N_HOLD_TIME, np.hold_time);			

			obs_source_update(_noise_gate_filter, gate_settings);

			obs_data_release(gate_settings);
		}
	}
}
