#pragma once

#include "ReziAudioBackend.h"

#include <array>
#include <memory>
#include <mutex>
#include <vector>

struct ma_engine;
struct ma_sound;

namespace EGE::ReziAudio
{
	struct StreamDataSource;

	class MiniaudioBackend final : public IAudioBackend
	{
	public:
		MiniaudioBackend() = default;
		~MiniaudioBackend() override;

		MiniaudioBackend(const MiniaudioBackend&) = delete;
		MiniaudioBackend& operator=(const MiniaudioBackend&) = delete;

		bool Initialize(ma_engine& engine);
		void Shutdown();

		[[nodiscard]] bool IsReady() const override;
		[[nodiscard]] PlaybackHandle CreateVoice(
			const VoiceCreateInfo& createInfo) override;
		[[nodiscard]] PlaybackHandle CreateStreamVoice(
			std::shared_ptr<IAudioStream> stream,
			const VoiceSettings& settings,
			const AudioTransform& transform) override;
		bool DestroyVoice(PlaybackHandle handle) override;
		bool Play(PlaybackHandle handle) override;
		bool Pause(PlaybackHandle handle) override;
		bool Stop(PlaybackHandle handle) override;
		bool FadeTo(
			PlaybackHandle handle,
			float targetVolume,
			float durationSeconds) override;
		bool StopWithFade(
			PlaybackHandle handle,
			float durationSeconds) override;
		bool SeekSeconds(
			PlaybackHandle handle,
			float seconds) override;
		[[nodiscard]] float GetPlaybackSeconds(
			PlaybackHandle handle) const override;
		[[nodiscard]] float GetPlaybackLengthSeconds(
			PlaybackHandle handle) const override;
		bool SetSettings(
			PlaybackHandle handle,
			const VoiceSettings& settings) override;
		bool SetTransform(
			PlaybackHandle handle,
			const AudioTransform& transform) override;
		bool SetListener(
			std::uint32_t index,
			const AudioTransform& transform,
			bool enabled) override;
		bool SetBusVolume(Bus bus, float volume) override;
		[[nodiscard]] float GetBusVolume(Bus bus) const override;
		[[nodiscard]] PlaybackState GetState(
			PlaybackHandle handle) const override;
		[[nodiscard]] BackendStats GetStats() const override;

	private:
		struct VoiceSlot
		{
			VoiceSlot();
			~VoiceSlot();
			VoiceSlot(VoiceSlot&&) noexcept;
			VoiceSlot& operator=(VoiceSlot&&) noexcept;
			VoiceSlot(const VoiceSlot&) = delete;
			VoiceSlot& operator=(const VoiceSlot&) = delete;

			std::unique_ptr<ma_sound> sound;
			std::unique_ptr<StreamDataSource> streamDataSource;
			std::uint32_t generation = 0;
			bool paused = false;
			bool allocated = false;
		};

		[[nodiscard]] VoiceSlot* Resolve(PlaybackHandle handle);
		[[nodiscard]] const VoiceSlot* Resolve(
			PlaybackHandle handle) const;
		[[nodiscard]] ma_sound* ResolveBus(Bus bus);
		[[nodiscard]] const ma_sound* ResolveBus(Bus bus) const;
		bool ApplySettings(
			ma_sound& sound,
			const VoiceSettings& settings);
		static std::size_t BusIndex(Bus bus);

		mutable std::recursive_mutex mutex_;
		ma_engine* engine_ = nullptr;
		std::array<std::unique_ptr<ma_sound>, 5> buses_;
		std::array<bool, 5> busInitialized_{};
		std::array<float, 5> busVolumes_{1, 1, 1, 1, 1};
		std::vector<VoiceSlot> voices_;
	};
}
