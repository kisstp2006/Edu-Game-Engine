#pragma once

#include "Component.h"
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
	bool Pause();
	bool Resume();
	void Stop();
	[[nodiscard]] EGE::ReziAudio::PlaybackState GetPlaybackState() const;
	[[nodiscard]] bool IsPlaying() const;

	bool SetSoundGraph(
		const EGE::ReziAudio::SoundGraphAsset& graph);
	void ClearSoundGraph();
	[[nodiscard]] bool HasSoundGraph() const;
	void SetRuntimeParameter(
		std::string_view name,
		const EGE::ReziAudio::ParameterValue& value);
	[[nodiscard]] const EGE::ReziAudio::ParameterValue*
		GetRuntimeParameter(std::string_view name) const;
	void ClearRuntimeParameters();

	EGE::ReziAudio::VoiceSettings settings;
	bool playOnStart = false;

private:
	void UpdateVoice();
	void ReleaseVoice();
	[[nodiscard]] EGE::ReziAudio::AudioTransform BuildTransform() const;

	UID clip_ = 0;
	EGE::ReziAudio::PlaybackHandle voice_;
	std::unordered_map<
		std::string,
		EGE::ReziAudio::ParameterValue> runtimeParameters_;
	std::unique_ptr<EGE::ReziAudio::SoundGraphInstance> graphInstance_;
	std::optional<EGE::ReziAudio::SoundGraphAsset> graphAsset_;
};
