#include "ReziAudioMiniaudioBackend.h"

#include <miniaudio.h>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#include <algorithm>
#include <cmath>

namespace EGE::ReziAudio
{
	struct StreamDataSource
	{
		ma_data_source_base base{};
		std::shared_ptr<IAudioStream> stream;
		bool initialized = false;
	};

	namespace
	{
		constexpr float DegreesToRadians =
			3.14159265358979323846f / 180.0f;

		ma_attenuation_model ToMiniaudio(AttenuationModel model)
		{
			switch (model)
			{
			case AttenuationModel::None:
				return ma_attenuation_model_none;
			case AttenuationModel::Linear:
				return ma_attenuation_model_linear;
			case AttenuationModel::Exponential:
				return ma_attenuation_model_exponential;
			case AttenuationModel::Inverse:
			default:
				return ma_attenuation_model_inverse;
			}
		}

		StreamDataSource* ResolveStream(ma_data_source* source)
		{
			return reinterpret_cast<StreamDataSource*>(source);
		}

		ma_result ReadStream(
			ma_data_source* source,
			void* output,
			ma_uint64 frameCount,
			ma_uint64* framesRead)
		{
			StreamDataSource* dataSource = ResolveStream(source);
			if (!dataSource || !dataSource->stream)
				return MA_INVALID_ARGS;
			if (!output)
			{
				const std::uint64_t cursor =
					dataSource->stream->GetCursorFrames();
				const bool seeked =
					dataSource->stream->SeekFrame(cursor + frameCount);
				if (framesRead)
					*framesRead = seeked ? frameCount : 0;
				return seeked ? MA_SUCCESS : MA_INVALID_OPERATION;
			}
			const std::uint64_t read = dataSource->stream->ReadFrames(
				static_cast<float*>(output), frameCount);
			if (framesRead)
				*framesRead = read;
			return read == 0 && frameCount != 0
				? MA_AT_END
				: MA_SUCCESS;
		}

		ma_result SeekStream(
			ma_data_source* source,
			ma_uint64 frame)
		{
			StreamDataSource* dataSource = ResolveStream(source);
			return dataSource && dataSource->stream &&
				dataSource->stream->SeekFrame(frame)
				? MA_SUCCESS
				: MA_INVALID_OPERATION;
		}

		ma_result GetStreamFormat(
			ma_data_source* source,
			ma_format* format,
			ma_uint32* channels,
			ma_uint32* sampleRate,
			ma_channel* channelMap,
			size_t channelMapCapacity)
		{
			StreamDataSource* dataSource = ResolveStream(source);
			if (!dataSource || !dataSource->stream)
				return MA_INVALID_ARGS;
			if (format)
				*format = ma_format_f32;
			if (channels)
				*channels = dataSource->stream->GetChannels();
			if (sampleRate)
				*sampleRate = dataSource->stream->GetSampleRate();
			if (channelMap)
			{
				ma_channel_map_init_standard(
					ma_standard_channel_map_default,
					channelMap,
					channelMapCapacity,
					dataSource->stream->GetChannels());
			}
			return MA_SUCCESS;
		}

		ma_result GetStreamCursor(
			ma_data_source* source,
			ma_uint64* cursor)
		{
			StreamDataSource* dataSource = ResolveStream(source);
			if (!dataSource || !dataSource->stream || !cursor)
				return MA_INVALID_ARGS;
			*cursor = dataSource->stream->GetCursorFrames();
			return MA_SUCCESS;
		}

		ma_result GetStreamLength(
			ma_data_source* source,
			ma_uint64* length)
		{
			StreamDataSource* dataSource = ResolveStream(source);
			if (!dataSource || !dataSource->stream || !length)
				return MA_INVALID_ARGS;
			*length = dataSource->stream->GetLengthFrames();
			return *length == 0 ? MA_NOT_IMPLEMENTED : MA_SUCCESS;
		}

		ma_result SetStreamLooping(
			ma_data_source* source,
			ma_bool32 looping)
		{
			StreamDataSource* dataSource = ResolveStream(source);
			if (!dataSource || !dataSource->stream)
				return MA_INVALID_ARGS;
			dataSource->stream->SetLooping(looping == MA_TRUE);
			return MA_SUCCESS;
		}

		ma_data_source_vtable StreamVTable{
			ReadStream,
			SeekStream,
			GetStreamFormat,
			GetStreamCursor,
			GetStreamLength,
			SetStreamLooping,
			0};

		bool InitializeStreamDataSource(
			StreamDataSource& dataSource,
			std::shared_ptr<IAudioStream> stream)
		{
			if (!stream || stream->GetChannels() == 0 ||
				stream->GetSampleRate() == 0)
			{
				return false;
			}
			dataSource.stream = std::move(stream);
			ma_data_source_config config = ma_data_source_config_init();
			config.vtable = &StreamVTable;
			dataSource.initialized =
				ma_data_source_init(&config, &dataSource.base) == MA_SUCCESS;
			if (!dataSource.initialized)
				dataSource.stream.reset();
			return dataSource.initialized;
		}

		void UninitializeStreamDataSource(
			StreamDataSource& dataSource)
		{
			if (dataSource.initialized)
				ma_data_source_uninit(&dataSource.base);
			dataSource.initialized = false;
			dataSource.stream.reset();
		}
	}

	MiniaudioBackend::VoiceSlot::VoiceSlot() = default;

	MiniaudioBackend::VoiceSlot::~VoiceSlot()
	{
		if (streamDataSource)
			UninitializeStreamDataSource(*streamDataSource);
	}

	MiniaudioBackend::VoiceSlot::VoiceSlot(VoiceSlot&&) noexcept = default;

	MiniaudioBackend::VoiceSlot&
	MiniaudioBackend::VoiceSlot::operator=(VoiceSlot&&) noexcept = default;

	MiniaudioBackend::~MiniaudioBackend()
	{
		Shutdown();
	}

	bool MiniaudioBackend::Initialize(ma_engine& engine)
	{
		std::scoped_lock lock(mutex_);
		Shutdown();
		engine_ = &engine;

		for (std::size_t index = 0; index < buses_.size(); ++index)
		{
			buses_[index] = std::make_unique<ma_sound>();
			ma_sound* parent =
				index == BusIndex(Bus::Master)
				? nullptr
				: buses_[BusIndex(Bus::Master)].get();
			if (ma_sound_group_init(
					engine_, 0, parent, buses_[index].get()) != MA_SUCCESS)
			{
				Shutdown();
				return false;
			}
			busInitialized_[index] = true;
		}
		return true;
	}

	void MiniaudioBackend::Shutdown()
	{
		std::scoped_lock lock(mutex_);
		for (VoiceSlot& voice : voices_)
		{
			if (voice.allocated && voice.sound)
				ma_sound_uninit(voice.sound.get());
			voice.sound.reset();
			voice.streamDataSource.reset();
			voice.allocated = false;
			voice.paused = false;
		}

		for (std::size_t offset = 0; offset < buses_.size(); ++offset)
		{
			const std::size_t index = buses_.size() - 1 - offset;
			if (buses_[index] && busInitialized_[index])
				ma_sound_group_uninit(buses_[index].get());
			buses_[index].reset();
			busInitialized_[index] = false;
		}
		engine_ = nullptr;
	}

	bool MiniaudioBackend::IsReady() const
	{
		std::scoped_lock lock(mutex_);
		return engine_ != nullptr &&
			buses_[BusIndex(Bus::Master)] != nullptr;
	}

	PlaybackHandle MiniaudioBackend::CreateVoice(
		const VoiceCreateInfo& createInfo)
	{
		std::scoped_lock lock(mutex_);
		if (!IsReady() || createInfo.filePath.empty())
			return {};

		std::size_t index = 0;
		for (; index < voices_.size(); ++index)
		{
			if (!voices_[index].allocated)
				break;
		}
		if (index == voices_.size())
			voices_.emplace_back();

		VoiceSlot& slot = voices_[index];
		slot.sound = std::make_unique<ma_sound>();
		slot.streamDataSource.reset();
		slot.generation =
			slot.generation == std::numeric_limits<std::uint32_t>::max()
				? 1
				: slot.generation + 1;

		const ma_uint32 flags = createInfo.settings.streaming
			? MA_SOUND_FLAG_STREAM
			: MA_SOUND_FLAG_DECODE;
		if (ma_sound_init_from_file(
				engine_,
				createInfo.filePath.c_str(),
				flags,
				ResolveBus(createInfo.settings.bus),
				nullptr,
				slot.sound.get()) != MA_SUCCESS)
		{
			slot.sound.reset();
			return {};
		}

		slot.allocated = true;
		slot.paused = false;
		ApplySettings(*slot.sound, createInfo.settings);
		SetTransform(
			{static_cast<std::uint32_t>(index), slot.generation},
			createInfo.transform);
		return {
			static_cast<std::uint32_t>(index),
			slot.generation};
	}

	PlaybackHandle MiniaudioBackend::CreateStreamVoice(
		std::shared_ptr<IAudioStream> stream,
		const VoiceSettings& settings,
		const AudioTransform& transform)
	{
		std::scoped_lock lock(mutex_);
		if (!IsReady() || !stream)
			return {};

		std::size_t index = 0;
		for (; index < voices_.size(); ++index)
		{
			if (!voices_[index].allocated)
				break;
		}
		if (index == voices_.size())
			voices_.emplace_back();

		VoiceSlot& slot = voices_[index];
		slot.sound = std::make_unique<ma_sound>();
		slot.streamDataSource = std::make_unique<StreamDataSource>();
		slot.generation =
			slot.generation == std::numeric_limits<std::uint32_t>::max()
				? 1
				: slot.generation + 1;
		if (!InitializeStreamDataSource(
				*slot.streamDataSource, std::move(stream)) ||
			ma_sound_init_from_data_source(
				engine_,
				&slot.streamDataSource->base,
				0,
				ResolveBus(settings.bus),
				slot.sound.get()) != MA_SUCCESS)
		{
			slot.sound.reset();
			slot.streamDataSource.reset();
			return {};
		}

		slot.allocated = true;
		slot.paused = false;
		ApplySettings(*slot.sound, settings);
		SetTransform(
			{static_cast<std::uint32_t>(index), slot.generation},
			transform);
		return {
			static_cast<std::uint32_t>(index),
			slot.generation};
	}

	bool MiniaudioBackend::DestroyVoice(PlaybackHandle handle)
	{
		std::scoped_lock lock(mutex_);
		VoiceSlot* slot = Resolve(handle);
		if (!slot)
			return false;

		ma_sound_uninit(slot->sound.get());
		slot->sound.reset();
		slot->streamDataSource.reset();
		slot->allocated = false;
		slot->paused = false;
		return true;
	}

	bool MiniaudioBackend::Play(PlaybackHandle handle)
	{
		std::scoped_lock lock(mutex_);
		VoiceSlot* slot = Resolve(handle);
		if (!slot)
			return false;
		slot->paused = false;
		return ma_sound_start(slot->sound.get()) == MA_SUCCESS;
	}

	bool MiniaudioBackend::Pause(PlaybackHandle handle)
	{
		std::scoped_lock lock(mutex_);
		VoiceSlot* slot = Resolve(handle);
		if (!slot)
			return false;
		slot->paused = true;
		return ma_sound_stop(slot->sound.get()) == MA_SUCCESS;
	}

	bool MiniaudioBackend::Stop(PlaybackHandle handle)
	{
		std::scoped_lock lock(mutex_);
		VoiceSlot* slot = Resolve(handle);
		if (!slot)
			return false;
		slot->paused = false;
		const bool stopped =
			ma_sound_stop(slot->sound.get()) == MA_SUCCESS;
		return ma_sound_seek_to_pcm_frame(
			slot->sound.get(), 0) == MA_SUCCESS && stopped;
	}

	bool MiniaudioBackend::FadeTo(
		PlaybackHandle handle,
		float targetVolume,
		float durationSeconds)
	{
		std::scoped_lock lock(mutex_);
		VoiceSlot* slot = Resolve(handle);
		if (!slot)
			return false;

		targetVolume = std::max(targetVolume, 0.0f);
		durationSeconds = std::max(durationSeconds, 0.0f);
		if (durationSeconds <= 0.0f)
		{
			ma_sound_set_volume(slot->sound.get(), targetVolume);
			return true;
		}

		const double milliseconds =
			static_cast<double>(durationSeconds) * 1000.0;
		const ma_uint64 durationMilliseconds =
			static_cast<ma_uint64>(std::min(
				milliseconds,
				static_cast<double>(
					std::numeric_limits<ma_uint64>::max())));
		ma_sound_set_fade_in_milliseconds(
			slot->sound.get(),
			-1.0f,
			targetVolume,
			durationMilliseconds);
		return true;
	}

	bool MiniaudioBackend::StopWithFade(
		PlaybackHandle handle,
		float durationSeconds)
	{
		std::scoped_lock lock(mutex_);
		VoiceSlot* slot = Resolve(handle);
		if (!slot)
			return false;

		durationSeconds = std::max(durationSeconds, 0.0f);
		if (durationSeconds <= 0.0f)
			return Stop(handle);

		const double milliseconds =
			static_cast<double>(durationSeconds) * 1000.0;
		const ma_uint64 durationMilliseconds =
			static_cast<ma_uint64>(std::min(
				milliseconds,
				static_cast<double>(
					std::numeric_limits<ma_uint64>::max())));
		slot->paused = false;
		return ma_sound_stop_with_fade_in_milliseconds(
			slot->sound.get(),
			durationMilliseconds) == MA_SUCCESS;
	}

	bool MiniaudioBackend::SeekSeconds(
		PlaybackHandle handle,
		float seconds)
	{
		std::scoped_lock lock(mutex_);
		VoiceSlot* slot = Resolve(handle);
		return slot &&
			ma_sound_seek_to_second(
				slot->sound.get(),
				std::max(seconds, 0.0f)) == MA_SUCCESS;
	}

	float MiniaudioBackend::GetPlaybackSeconds(
		PlaybackHandle handle) const
	{
		std::scoped_lock lock(mutex_);
		const VoiceSlot* slot = Resolve(handle);
		float seconds = 0.0f;
		return slot &&
			ma_sound_get_cursor_in_seconds(
				slot->sound.get(), &seconds) == MA_SUCCESS
				? seconds
				: 0.0f;
	}

	float MiniaudioBackend::GetPlaybackLengthSeconds(
		PlaybackHandle handle) const
	{
		std::scoped_lock lock(mutex_);
		const VoiceSlot* slot = Resolve(handle);
		float seconds = 0.0f;
		return slot &&
			ma_sound_get_length_in_seconds(
				slot->sound.get(), &seconds) == MA_SUCCESS
				? seconds
				: 0.0f;
	}

	bool MiniaudioBackend::SetSettings(
		PlaybackHandle handle,
		const VoiceSettings& settings)
	{
		std::scoped_lock lock(mutex_);
		VoiceSlot* slot = Resolve(handle);
		return slot && ApplySettings(*slot->sound, settings);
	}

	bool MiniaudioBackend::SetTransform(
		PlaybackHandle handle,
		const AudioTransform& transform)
	{
		std::scoped_lock lock(mutex_);
		VoiceSlot* slot = Resolve(handle);
		if (!slot)
			return false;

		ma_sound_set_position(
			slot->sound.get(),
			transform.position.x,
			transform.position.y,
			transform.position.z);
		ma_sound_set_direction(
			slot->sound.get(),
			transform.forward.x,
			transform.forward.y,
			transform.forward.z);
		ma_sound_set_velocity(
			slot->sound.get(),
			transform.velocity.x,
			transform.velocity.y,
			transform.velocity.z);
		return true;
	}

	bool MiniaudioBackend::SetListener(
		std::uint32_t index,
		const AudioTransform& transform,
		bool enabled)
	{
		std::scoped_lock lock(mutex_);
		if (!IsReady() || index >= ma_engine_get_listener_count(engine_))
			return false;

		ma_engine_listener_set_enabled(
			engine_, index, enabled ? MA_TRUE : MA_FALSE);
		ma_engine_listener_set_position(
			engine_, index,
			transform.position.x,
			transform.position.y,
			transform.position.z);
		ma_engine_listener_set_direction(
			engine_, index,
			transform.forward.x,
			transform.forward.y,
			transform.forward.z);
		ma_engine_listener_set_world_up(
			engine_, index,
			transform.up.x,
			transform.up.y,
			transform.up.z);
		ma_engine_listener_set_velocity(
			engine_, index,
			transform.velocity.x,
			transform.velocity.y,
			transform.velocity.z);
		return true;
	}

	bool MiniaudioBackend::SetBusVolume(Bus bus, float volume)
	{
		std::scoped_lock lock(mutex_);
		ma_sound* group = ResolveBus(bus);
		if (!group)
			return false;
		volume = std::max(volume, 0.0f);
		busVolumes_[BusIndex(bus)] = volume;
		ma_sound_group_set_volume(group, volume);
		return true;
	}

	float MiniaudioBackend::GetBusVolume(Bus bus) const
	{
		std::scoped_lock lock(mutex_);
		return busVolumes_[BusIndex(bus)];
	}

	PlaybackState MiniaudioBackend::GetState(
		PlaybackHandle handle) const
	{
		std::scoped_lock lock(mutex_);
		const VoiceSlot* slot = Resolve(handle);
		if (!slot)
			return PlaybackState::Invalid;
		if (slot->paused)
			return PlaybackState::Paused;
		if (ma_sound_is_playing(slot->sound.get()))
			return PlaybackState::Playing;
		if (ma_sound_at_end(slot->sound.get()))
			return PlaybackState::Finished;
		return PlaybackState::Stopped;
	}

	BackendStats MiniaudioBackend::GetStats() const
	{
		std::scoped_lock lock(mutex_);
		BackendStats stats;
		stats.voiceCapacity = voices_.size();
		stats.activeVoices = std::count_if(
			voices_.begin(), voices_.end(),
			[](const VoiceSlot& slot)
			{
				return slot.allocated;
			});
		return stats;
	}

	MiniaudioBackend::VoiceSlot* MiniaudioBackend::Resolve(
		PlaybackHandle handle)
	{
		if (!handle.IsValid() || handle.index >= voices_.size())
			return nullptr;
		VoiceSlot& slot = voices_[handle.index];
		if (!slot.allocated || !slot.sound ||
			slot.generation != handle.generation)
			return nullptr;
		return &slot;
	}

	const MiniaudioBackend::VoiceSlot* MiniaudioBackend::Resolve(
		PlaybackHandle handle) const
	{
		if (!handle.IsValid() || handle.index >= voices_.size())
			return nullptr;
		const VoiceSlot& slot = voices_[handle.index];
		if (!slot.allocated || !slot.sound ||
			slot.generation != handle.generation)
			return nullptr;
		return &slot;
	}

	ma_sound* MiniaudioBackend::ResolveBus(Bus bus)
	{
		return buses_[BusIndex(bus)].get();
	}

	const ma_sound* MiniaudioBackend::ResolveBus(Bus bus) const
	{
		return buses_[BusIndex(bus)].get();
	}

	bool MiniaudioBackend::ApplySettings(
		ma_sound& sound,
		const VoiceSettings& settings)
	{
		ma_sound_set_volume(&sound, std::max(settings.volume, 0.0f));
		ma_sound_set_pitch(&sound, std::max(settings.pitch, 0.01f));
		ma_sound_set_pan(&sound, std::clamp(settings.pan, -1.0f, 1.0f));
		ma_sound_set_looping(&sound, settings.looping ? MA_TRUE : MA_FALSE);
		ma_sound_set_spatialization_enabled(
			&sound, settings.spatial.enabled ? MA_TRUE : MA_FALSE);
		ma_sound_set_attenuation_model(
			&sound, ToMiniaudio(settings.spatial.attenuation));
		ma_sound_set_min_distance(
			&sound, std::max(settings.spatial.minDistance, 0.0f));
		ma_sound_set_max_distance(
			&sound,
			std::max(
				settings.spatial.maxDistance,
				settings.spatial.minDistance));
		ma_sound_set_min_gain(
			&sound, std::max(settings.spatial.minGain, 0.0f));
		ma_sound_set_max_gain(
			&sound,
			std::max(
				settings.spatial.maxGain,
				settings.spatial.minGain));
		ma_sound_set_rolloff(
			&sound, std::max(settings.spatial.rolloff, 0.0f));
		ma_sound_set_doppler_factor(
			&sound, std::max(settings.spatial.dopplerFactor, 0.0f));
		ma_sound_set_cone(
			&sound,
			std::clamp(
				settings.spatial.cone.innerAngleDegrees,
				0.0f, 360.0f) * DegreesToRadians,
			std::clamp(
				settings.spatial.cone.outerAngleDegrees,
				0.0f, 360.0f) * DegreesToRadians,
			std::clamp(
				settings.spatial.cone.outerGain,
				0.0f, 1.0f));
		return true;
	}

	std::size_t MiniaudioBackend::BusIndex(Bus bus)
	{
		return static_cast<std::size_t>(bus);
	}
}
