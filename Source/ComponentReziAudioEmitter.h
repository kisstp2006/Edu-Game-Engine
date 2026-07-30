#pragma once

#include "Component.h"
#include "ReziAudioDspGraph.h"
#include "ReziAudioGraph.h"

#include <memory>
#include <optional>
#include <string_view>
#include <unordered_map>

class ResourceAudio;

class ComponentReziAudioEmitter final : public Component
{
	friend class ModuleAudio;

public:
	explicit ComponentReziAudioEmitter(GameObject* container);
	~ComponentReziAudioEmitter() override;

	void OnSave(Config& config) const override;
	void OnLoad(Config* config) override;
	void OnStart() override;
	void OnPlay() override;
	void OnStop() override;
	void OnDeActivate() override;
	void OnUpdateTransform() override;
	void GetBoundingBox(AABB& box) const override;

	bool SetClip(UID resource);
	[[nodiscard]] UID GetClip() const;
	[[nodiscard]] const ResourceAudio* GetClipResource() const;

	bool Play();
	bool PlayWithFade(float durationSeconds);
	bool Pause();
	bool Resume();
	void Stop();
	bool StopWithFade(float durationSeconds);
	bool FadeTo(float targetVolume, float durationSeconds);
	bool Seek(float seconds);
	[[nodiscard]] EGE::ReziAudio::PlaybackState GetPlaybackState() const;
	[[nodiscard]] bool IsPlaying() const;
	[[nodiscard]] bool IsPaused() const;
	[[nodiscard]] bool IsFinished() const;
	[[nodiscard]] float GetPlaybackSeconds() const;
	[[nodiscard]] float GetPlaybackLengthSeconds() const;
	[[nodiscard]] float GetPlaybackPercentage() const;

	bool SetSoundGraph(
		const EGE::ReziAudio::SoundGraphAsset& graph);
	bool SetDspGraph(
		const EGE::ReziAudio::DspGraphAsset& graph);
	void ClearSoundGraph();
	void ClearDspGraph();
	[[nodiscard]] bool HasSoundGraph() const;
	[[nodiscard]] bool HasDspGraph() const;
	void SetRuntimeParameter(
		std::string_view name,
		const EGE::ReziAudio::ParameterValue& value);
	bool SetRuntimeAudioClipParameter(
		std::string_view name,
		UID audioResource);
	[[nodiscard]] const EGE::ReziAudio::ParameterValue*
		GetRuntimeParameter(std::string_view name) const;
	void ClearRuntimeParameters();

	EGE::ReziAudio::VoiceSettings settings;
	bool playOnStart = false;

private:
	void UpdateVoice();
	void ReleaseVoice();
	bool ApplyDspParameter(
		std::string_view name,
		const EGE::ReziAudio::ParameterValue& value);
	[[nodiscard]] EGE::ReziAudio::AudioClipReference
		ResolveAudioClip(UID audioResource) const;
	void ResolveAudioClips(
		EGE::ReziAudio::SoundGraphAsset& graph) const;
	void ResolveAudioClips(
		EGE::ReziAudio::DspGraphAsset& graph) const;
	[[nodiscard]] EGE::ReziAudio::AudioTransform BuildTransform() const;

	UID clip_ = 0;
	EGE::ReziAudio::PlaybackHandle voice_;
	std::unordered_map<
		std::string,
		EGE::ReziAudio::ParameterValue> runtimeParameters_;
	std::unique_ptr<EGE::ReziAudio::SoundGraphInstance> graphInstance_;
	std::optional<EGE::ReziAudio::SoundGraphAsset> graphAsset_;
	std::shared_ptr<EGE::ReziAudio::DspGraphStream> dspStream_;
	std::optional<EGE::ReziAudio::DspGraphAsset> dspAsset_;
};
