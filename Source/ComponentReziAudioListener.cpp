#include "ComponentReziAudioListener.h"

#include "Application.h"
#include "GameObject.h"
#include "ModuleAudio.h"

#include <algorithm>

ComponentReziAudioListener::ComponentReziAudioListener(
	GameObject* container)
	: Component(container, Types::ReziAudioListener)
{
	if (App && App->audio)
		App->audio->Register(this);
}

ComponentReziAudioListener::~ComponentReziAudioListener()
{
	if (App && App->audio)
		App->audio->Unregister(this);
}

void ComponentReziAudioListener::OnSave(Config& config) const
{
	config.AddFloat("Gain", gain);
}

void ComponentReziAudioListener::OnLoad(Config* config)
{
	gain = config
		? std::max(config->GetFloat("Gain", 1.0f), 0.0f)
		: 1.0f;
}

void ComponentReziAudioListener::OnUpdateTransform()
{
	UpdateListener();
}

void ComponentReziAudioListener::OnDeActivate()
{
	if (App && App->audio)
		App->audio->GetReziAudio().SetListener(BuildTransform(), false);
}

void ComponentReziAudioListener::UpdateListener() const
{
	if (App && App->audio)
	{
		App->audio->GetReziAudio().SetBusVolume(
			EGE::ReziAudio::Bus::Master,
			std::max(gain, 0.0f));
		App->audio->GetReziAudio().SetListener(BuildTransform(), true);
	}
}

EGE::ReziAudio::AudioTransform
ComponentReziAudioListener::BuildTransform() const
{
	EGE::ReziAudio::AudioTransform transform;
	if (!game_object)
		return transform;

	transform.position = game_object->GetGlobalPosition();
	transform.forward =
		game_object->GetGlobalTransformation().WorldZ().Normalized();
	transform.up =
		game_object->GetGlobalTransformation().WorldY().Normalized();
	transform.velocity = game_object->GetVelocity();
	return transform;
}
