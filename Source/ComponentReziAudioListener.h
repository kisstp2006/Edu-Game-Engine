#pragma once

#include "Component.h"
#include "ReziAudioTypes.h"

class ComponentReziAudioListener final : public Component
{
	friend class ModuleAudio;

public:
	explicit ComponentReziAudioListener(GameObject* container);
	~ComponentReziAudioListener() override;

	void OnSave(Config& config) const override;
	void OnLoad(Config* config) override;
	void OnUpdateTransform() override;
	void OnDeActivate() override;

	float gain = 1.0f;

private:
	void UpdateListener() const;
	[[nodiscard]] EGE::ReziAudio::AudioTransform BuildTransform() const;
};
