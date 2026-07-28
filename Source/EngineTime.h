#pragma once

#include <cstdint>

namespace EGE
{
	class TimeService final
	{
	public:
		static constexpr float DefaultFixedDeltaTime = 1.0f / 60.0f;
		static constexpr float MaximumFrameDeltaTime = 0.25f;
		static constexpr std::uint32_t MaximumFixedStepsPerFrame = 8;
		static constexpr float MaximumTimeScale = 100.0f;

		void BeginPlay();
		void Stop();
		void Pause();
		void Resume();
		void BeginFrame(float unscaledDeltaTime);
		void DiscardPendingFixedSteps();
		void ResetForProjectChange();

		void SetTimeScale(float timeScale);

		[[nodiscard]] double GetTime() const;
		[[nodiscard]] double GetUnscaledTime() const;
		[[nodiscard]] float GetDeltaTime() const;
		[[nodiscard]] float GetUnscaledDeltaTime() const;
		[[nodiscard]] float GetFixedDeltaTime() const;
		[[nodiscard]] float GetTimeScale() const;
		[[nodiscard]] std::uint64_t GetFrameCount() const;
		[[nodiscard]] std::uint32_t GetFixedStepCount() const;
		[[nodiscard]] float GetFixedInterpolationAlpha() const;
		[[nodiscard]] bool IsPlaying() const;
		[[nodiscard]] bool IsPaused() const;

	private:
		double time_ = 0.0;
		double unscaledTime_ = 0.0;
		double fixedAccumulator_ = 0.0;
		float deltaTime_ = 0.0f;
		float unscaledDeltaTime_ = 0.0f;
		float fixedDeltaTime_ = DefaultFixedDeltaTime;
		float timeScale_ = 1.0f;
		std::uint64_t frameCount_ = 0;
		std::uint32_t fixedStepCount_ = 0;
		bool playing_ = false;
		bool paused_ = false;
	};
}
