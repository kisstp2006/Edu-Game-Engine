#include "ReziAudioSystem.h"

#include <algorithm>

namespace EGE::ReziAudio
{
	bool System::Initialize(ma_engine& engine)
	{
		return backend_.Initialize(engine);
	}

	void System::Shutdown()
	{
		backend_.Shutdown();
	}

	bool System::IsReady() const
	{
		return backend_.IsReady();
	}

	PlaybackHandle System::CreateVoice(
		const VoiceCreateInfo& createInfo)
	{
		return backend_.CreateVoice(createInfo);
	}

	PlaybackHandle System::CreateStreamVoice(
		std::shared_ptr<IAudioStream> stream,
		const VoiceSettings& settings,
		const AudioTransform& transform)
	{
		return backend_.CreateStreamVoice(
			std::move(stream), settings, transform);
	}

	bool System::DestroyVoice(PlaybackHandle& handle)
	{
		const bool destroyed = backend_.DestroyVoice(handle);
		if (destroyed)
			handle = {};
		return destroyed;
	}

	bool System::Play(PlaybackHandle handle)
	{
		return backend_.Play(handle);
	}

	bool System::Pause(PlaybackHandle handle)
	{
		return backend_.Pause(handle);
	}

	bool System::Stop(PlaybackHandle handle)
	{
		return backend_.Stop(handle);
	}

	bool System::FadeTo(
		PlaybackHandle handle,
		float targetVolume,
		float durationSeconds)
	{
		return backend_.FadeTo(
			handle, targetVolume, durationSeconds);
	}

	bool System::StopWithFade(
		PlaybackHandle handle,
		float durationSeconds)
	{
		return backend_.StopWithFade(handle, durationSeconds);
	}

	bool System::SeekSeconds(
		PlaybackHandle handle,
		float seconds)
	{
		return backend_.SeekSeconds(handle, seconds);
	}

	float System::GetPlaybackSeconds(
		PlaybackHandle handle) const
	{
		return backend_.GetPlaybackSeconds(handle);
	}

	float System::GetPlaybackLengthSeconds(
		PlaybackHandle handle) const
	{
		return backend_.GetPlaybackLengthSeconds(handle);
	}

	float System::GetPlaybackPercentage(
		PlaybackHandle handle) const
	{
		const float length = GetPlaybackLengthSeconds(handle);
		return length > 0.0f
			? std::clamp(
				GetPlaybackSeconds(handle) / length,
				0.0f, 1.0f)
			: 0.0f;
	}

	bool System::SetSettings(
		PlaybackHandle handle,
		const VoiceSettings& settings)
	{
		return backend_.SetSettings(handle, settings);
	}

	bool System::SetTransform(
		PlaybackHandle handle,
		const AudioTransform& transform)
	{
		return backend_.SetTransform(handle, transform);
	}

	bool System::SetListener(
		const AudioTransform& transform,
		bool enabled)
	{
		return backend_.SetListener(0, transform, enabled);
	}

	bool System::SetBusVolume(Bus bus, float volume)
	{
		return backend_.SetBusVolume(bus, volume);
	}

	float System::GetBusVolume(Bus bus) const
	{
		return backend_.GetBusVolume(bus);
	}

	PlaybackState System::GetState(PlaybackHandle handle) const
	{
		return backend_.GetState(handle);
	}

	BackendStats System::GetStats() const
	{
		return backend_.GetStats();
	}
}
