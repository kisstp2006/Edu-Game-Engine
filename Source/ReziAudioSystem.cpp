#include "ReziAudioSystem.h"

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
