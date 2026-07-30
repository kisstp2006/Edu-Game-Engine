#pragma once

#include "ReziAudioTypes.h"

#include <cstddef>
#include <cstdint>
#include <memory>

namespace EGE::ReziAudio
{
	struct BackendStats
	{
		std::size_t activeVoices = 0;
		std::size_t voiceCapacity = 0;
	};

	class IAudioStream
	{
	public:
		virtual ~IAudioStream() = default;

		[[nodiscard]] virtual std::uint32_t GetChannels() const = 0;
		[[nodiscard]] virtual std::uint32_t GetSampleRate() const = 0;
		[[nodiscard]] virtual std::uint64_t GetCursorFrames() const = 0;
		[[nodiscard]] virtual std::uint64_t GetLengthFrames() const = 0;
		virtual std::uint64_t ReadFrames(
			float* interleavedOutput,
			std::uint64_t frameCount) noexcept = 0;
		virtual bool SeekFrame(std::uint64_t frame) noexcept = 0;
		virtual void SetLooping(bool looping) noexcept = 0;
	};

	class IAudioBackend
	{
	public:
		virtual ~IAudioBackend() = default;

		[[nodiscard]] virtual bool IsReady() const = 0;
		[[nodiscard]] virtual PlaybackHandle CreateVoice(
			const VoiceCreateInfo& createInfo) = 0;
		[[nodiscard]] virtual PlaybackHandle CreateStreamVoice(
			std::shared_ptr<IAudioStream> stream,
			const VoiceSettings& settings,
			const AudioTransform& transform) = 0;
		virtual bool DestroyVoice(PlaybackHandle handle) = 0;
		virtual bool Play(PlaybackHandle handle) = 0;
		virtual bool Pause(PlaybackHandle handle) = 0;
		virtual bool Stop(PlaybackHandle handle) = 0;
		virtual bool FadeTo(
			PlaybackHandle,
			float,
			float)
		{
			return false;
		}
		virtual bool StopWithFade(PlaybackHandle, float)
		{
			return false;
		}
		virtual bool SeekSeconds(PlaybackHandle, float)
		{
			return false;
		}
		[[nodiscard]] virtual float GetPlaybackSeconds(
			PlaybackHandle) const
		{
			return 0.0f;
		}
		[[nodiscard]] virtual float GetPlaybackLengthSeconds(
			PlaybackHandle) const
		{
			return 0.0f;
		}
		virtual bool SetSettings(
			PlaybackHandle handle,
			const VoiceSettings& settings) = 0;
		virtual bool SetTransform(
			PlaybackHandle handle,
			const AudioTransform& transform) = 0;
		virtual bool SetListener(
			std::uint32_t index,
			const AudioTransform& transform,
			bool enabled) = 0;
		virtual bool SetBusVolume(Bus bus, float volume) = 0;
		[[nodiscard]] virtual float GetBusVolume(Bus bus) const = 0;
		[[nodiscard]] virtual PlaybackState GetState(
			PlaybackHandle handle) const = 0;
		[[nodiscard]] virtual BackendStats GetStats() const = 0;
	};
}
