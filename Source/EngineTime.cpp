#include "EngineTime.h"

#include <algorithm>
#include <cmath>

namespace EGE
{
	void TimeService::BeginPlay()
	{
		time_ = 0.0;
		unscaledTime_ = 0.0;
		fixedAccumulator_ = 0.0;
		deltaTime_ = 0.0f;
		unscaledDeltaTime_ = 0.0f;
		timeScale_ = 1.0f;
		frameCount_ = 0;
		fixedStepCount_ = 0;
		playing_ = true;
		paused_ = false;
	}

	void TimeService::Stop()
	{
		deltaTime_ = 0.0f;
		unscaledDeltaTime_ = 0.0f;
		fixedAccumulator_ = 0.0;
		fixedStepCount_ = 0;
		playing_ = false;
		paused_ = false;
	}

	void TimeService::Pause()
	{
		if (!playing_)
			return;

		deltaTime_ = 0.0f;
		fixedStepCount_ = 0;
		paused_ = true;
	}

	void TimeService::Resume()
	{
		if (!playing_)
			return;

		deltaTime_ = 0.0f;
		fixedStepCount_ = 0;
		paused_ = false;
	}

	void TimeService::BeginFrame(float unscaledDeltaTime)
	{
		fixedStepCount_ = 0;
		if (!playing_)
		{
			deltaTime_ = 0.0f;
			unscaledDeltaTime_ = 0.0f;
			return;
		}

		if (!std::isfinite(unscaledDeltaTime) || unscaledDeltaTime < 0.0f)
			unscaledDeltaTime = 0.0f;

		unscaledDeltaTime_ = unscaledDeltaTime;
		unscaledTime_ += static_cast<double>(unscaledDeltaTime);
		++frameCount_;

		if (paused_)
		{
			deltaTime_ = 0.0f;
			return;
		}

		const float simulationDelta = std::min(
			unscaledDeltaTime, MaximumFrameDeltaTime);
		deltaTime_ = simulationDelta * timeScale_;
		time_ += static_cast<double>(deltaTime_);

		const double maximumAccumulator =
			static_cast<double>(fixedDeltaTime_) *
			MaximumFixedStepsPerFrame;
		fixedAccumulator_ = std::min(
			fixedAccumulator_ + static_cast<double>(deltaTime_),
			maximumAccumulator);

		const double step = static_cast<double>(fixedDeltaTime_);
		fixedStepCount_ = static_cast<std::uint32_t>(
			std::min(
				std::floor((fixedAccumulator_ + step * 1.0e-6) / step),
				static_cast<double>(MaximumFixedStepsPerFrame)));
		fixedAccumulator_ -=
			static_cast<double>(fixedStepCount_) * step;
		if (fixedAccumulator_ < 0.0)
			fixedAccumulator_ = 0.0;
	}

	void TimeService::DiscardPendingFixedSteps()
	{
		fixedAccumulator_ = 0.0;
		fixedStepCount_ = 0;
	}

	void TimeService::ResetForProjectChange()
	{
		time_ = 0.0;
		unscaledTime_ = 0.0;
		deltaTime_ = 0.0f;
		unscaledDeltaTime_ = 0.0f;
		fixedAccumulator_ = 0.0;
		timeScale_ = 1.0f;
		frameCount_ = 0;
		fixedStepCount_ = 0;
		playing_ = false;
		paused_ = false;
	}

	void TimeService::SetTimeScale(float timeScale)
	{
		if (!std::isfinite(timeScale))
			return;
		timeScale_ = std::clamp(
			timeScale, 0.0f, MaximumTimeScale);
	}

	double TimeService::GetTime() const
	{
		return time_;
	}

	double TimeService::GetUnscaledTime() const
	{
		return unscaledTime_;
	}

	float TimeService::GetDeltaTime() const
	{
		return deltaTime_;
	}

	float TimeService::GetUnscaledDeltaTime() const
	{
		return unscaledDeltaTime_;
	}

	float TimeService::GetFixedDeltaTime() const
	{
		return fixedDeltaTime_;
	}

	float TimeService::GetTimeScale() const
	{
		return timeScale_;
	}

	std::uint64_t TimeService::GetFrameCount() const
	{
		return frameCount_;
	}

	std::uint32_t TimeService::GetFixedStepCount() const
	{
		return fixedStepCount_;
	}

	float TimeService::GetFixedInterpolationAlpha() const
	{
		if (fixedDeltaTime_ <= 0.0f)
			return 0.0f;
		return static_cast<float>(
			fixedAccumulator_ / static_cast<double>(fixedDeltaTime_));
	}

	bool TimeService::IsPlaying() const
	{
		return playing_;
	}

	bool TimeService::IsPaused() const
	{
		return paused_;
	}
}
