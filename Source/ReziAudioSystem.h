#pragma once

#include "ReziAudioMiniaudioBackend.h"

struct ma_engine;

namespace EGE::ReziAudio
{
	class System final
	{
	public:
		bool Initialize(ma_engine& engine);
		void Shutdown();

		[[nodiscard]] bool IsReady() const;
		[[nodiscard]] PlaybackHandle CreateVoice(
			const VoiceCreateInfo& createInfo);
		[[nodiscard]] PlaybackHandle CreateStreamVoice(
			std::shared_ptr<IAudioStream> stream,
			const VoiceSettings& settings = {},
			const AudioTransform& transform = {});
		bool DestroyVoice(PlaybackHandle& handle);
		bool Play(PlaybackHandle handle);
		bool Pause(PlaybackHandle handle);
		bool Stop(PlaybackHandle handle);
		bool SetSettings(
			PlaybackHandle handle,
			const VoiceSettings& settings);
		bool SetTransform(
			PlaybackHandle handle,
			const AudioTransform& transform);
		bool SetListener(
			const AudioTransform& transform,
			bool enabled = true);
		bool SetBusVolume(Bus bus, float volume);
		[[nodiscard]] float GetBusVolume(Bus bus) const;
		[[nodiscard]] PlaybackState GetState(
			PlaybackHandle handle) const;
		[[nodiscard]] BackendStats GetStats() const;

	private:
		MiniaudioBackend backend_;
	};
}
