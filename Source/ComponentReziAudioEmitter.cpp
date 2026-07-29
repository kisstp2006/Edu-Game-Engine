#include "ComponentReziAudioEmitter.h"

#include "Application.h"
#include "GameObject.h"
#include "ModuleAudio.h"
#include "ModuleResources.h"
#include "ResourceAudio.h"

#include <algorithm>

ComponentReziAudioEmitter::ComponentReziAudioEmitter(GameObject* container)
	: Component(container, Types::ReziAudioEmitter)
{
	if (App && App->audio)
		App->audio->Register(this);
}

ComponentReziAudioEmitter::~ComponentReziAudioEmitter()
{
	ReleaseVoice();
	if (App && App->audio)
		App->audio->Unregister(this);
}

void ComponentReziAudioEmitter::OnSave(Config& config) const
{
	config.AddUID("Clip", clip_);
	config.AddBool("Play On Start", playOnStart);
	config.AddFloat("Volume", settings.volume);
	config.AddFloat("Pitch", settings.pitch);
	config.AddFloat("Pan", settings.pan);
	config.AddBool("Looping", settings.looping);
	config.AddBool("Streaming", settings.streaming);
	config.AddInt("Bus", static_cast<int>(settings.bus));
	config.AddBool("Spatial", settings.spatial.enabled);
	config.AddInt(
		"Attenuation",
		static_cast<int>(settings.spatial.attenuation));
	config.AddFloat("Min Distance", settings.spatial.minDistance);
	config.AddFloat("Max Distance", settings.spatial.maxDistance);
	config.AddFloat("Min Gain", settings.spatial.minGain);
	config.AddFloat("Max Gain", settings.spatial.maxGain);
	config.AddFloat("Rolloff", settings.spatial.rolloff);
	config.AddFloat("Doppler", settings.spatial.dopplerFactor);
	config.AddFloat(
		"Cone Inner", settings.spatial.cone.innerAngleDegrees);
	config.AddFloat(
		"Cone Outer", settings.spatial.cone.outerAngleDegrees);
	config.AddFloat(
		"Cone Outer Gain", settings.spatial.cone.outerGain);
}

void ComponentReziAudioEmitter::OnLoad(Config* config)
{
	if (!config)
		return;

	clip_ = config->GetUID("Clip", 0);
	playOnStart = config->GetBool("Play On Start", false);
	settings.volume = config->GetFloat("Volume", 1.0f);
	settings.pitch = config->GetFloat("Pitch", 1.0f);
	settings.pan = config->GetFloat("Pan", 0.0f);
	settings.looping = config->GetBool("Looping", false);
	settings.streaming = config->GetBool("Streaming", false);
	settings.bus = static_cast<EGE::ReziAudio::Bus>(
		std::clamp(config->GetInt("Bus", 2), 0, 4));
	settings.spatial.enabled = config->GetBool("Spatial", true);
	settings.spatial.attenuation =
		static_cast<EGE::ReziAudio::AttenuationModel>(
			std::clamp(config->GetInt("Attenuation", 1), 0, 3));
	settings.spatial.minDistance =
		std::max(config->GetFloat("Min Distance", 1.0f), 0.0f);
	settings.spatial.maxDistance = std::max(
		config->GetFloat("Max Distance", 100.0f),
		settings.spatial.minDistance);
	settings.spatial.minGain =
		std::max(config->GetFloat("Min Gain", 0.0f), 0.0f);
	settings.spatial.maxGain = std::max(
		config->GetFloat("Max Gain", 1.0f),
		settings.spatial.minGain);
	settings.spatial.rolloff =
		std::max(config->GetFloat("Rolloff", 1.0f), 0.0f);
	settings.spatial.dopplerFactor =
		std::max(config->GetFloat("Doppler", 1.0f), 0.0f);
	settings.spatial.cone.innerAngleDegrees =
		std::clamp(config->GetFloat("Cone Inner", 360.0f), 0.0f, 360.0f);
	settings.spatial.cone.outerAngleDegrees =
		std::clamp(config->GetFloat("Cone Outer", 360.0f), 0.0f, 360.0f);
	settings.spatial.cone.outerGain =
		std::clamp(config->GetFloat("Cone Outer Gain", 0.0f), 0.0f, 1.0f);
}

void ComponentReziAudioEmitter::OnStart()
{
	if (playOnStart)
		Play();
}

void ComponentReziAudioEmitter::OnPlay()
{
	if (playOnStart && !voice_.IsValid())
		Play();
}

void ComponentReziAudioEmitter::OnStop()
{
	ReleaseVoice();
}

void ComponentReziAudioEmitter::OnDeActivate()
{
	ReleaseVoice();
}

void ComponentReziAudioEmitter::OnUpdateTransform()
{
	UpdateVoice();
}

void ComponentReziAudioEmitter::GetBoundingBox(AABB& box) const
{
	if (settings.spatial.enabled)
	{
		box.Enclose(Sphere(
			float3::zero,
			std::max(settings.spatial.maxDistance, 0.0f)));
	}
}

bool ComponentReziAudioEmitter::SetClip(UID resource)
{
	if (resource == clip_)
		return true;
	if (resource != 0)
	{
		const Resource* candidate =
			App && App->resources ? App->resources->Get(resource) : nullptr;
		if (!candidate || candidate->GetType() != Resource::audio)
			return false;
	}

	ReleaseVoice();
	clip_ = resource;
	return true;
}

UID ComponentReziAudioEmitter::GetClip() const
{
	return clip_;
}

const ResourceAudio* ComponentReziAudioEmitter::GetClipResource() const
{
	const Resource* resource =
		App && App->resources ? App->resources->Get(clip_) : nullptr;
	return resource && resource->GetType() == Resource::audio
		? static_cast<const ResourceAudio*>(resource)
		: nullptr;
}

bool ComponentReziAudioEmitter::Play()
{
	if (!App || !App->audio || !App->audio->GetReziAudio().IsReady())
		return false;

	if (!voice_.IsValid())
	{
		EGE::ReziAudio::VoiceCreateInfo createInfo;
		if (graphInstance_ && graphInstance_->IsValid())
		{
			const EGE::ReziAudio::SoundGraphEvaluation evaluation =
				graphInstance_->Evaluate();
			if (!evaluation.succeeded)
				return false;
			createInfo = evaluation.voice;
		}
		else
		{
			const ResourceAudio* clip = GetClipResource();
			if (!clip || !clip->GetExportedFile())
				return false;
			createInfo.filePath = clip->GetExportedFile();
			createInfo.settings = settings;
		}
		createInfo.transform = BuildTransform();
		voice_ = App->audio->GetReziAudio().CreateVoice(createInfo);
	}
	return voice_.IsValid() &&
		App->audio->GetReziAudio().Play(voice_);
}

bool ComponentReziAudioEmitter::Pause()
{
	return voice_.IsValid() && App && App->audio &&
		App->audio->GetReziAudio().Pause(voice_);
}

bool ComponentReziAudioEmitter::Resume()
{
	return voice_.IsValid() && App && App->audio &&
		App->audio->GetReziAudio().Play(voice_);
}

void ComponentReziAudioEmitter::Stop()
{
	if (voice_.IsValid() && App && App->audio)
		App->audio->GetReziAudio().Stop(voice_);
}

EGE::ReziAudio::PlaybackState
ComponentReziAudioEmitter::GetPlaybackState() const
{
	return voice_.IsValid() && App && App->audio
		? App->audio->GetReziAudio().GetState(voice_)
		: EGE::ReziAudio::PlaybackState::Invalid;
}

bool ComponentReziAudioEmitter::IsPlaying() const
{
	return GetPlaybackState() ==
		EGE::ReziAudio::PlaybackState::Playing;
}

bool ComponentReziAudioEmitter::SetSoundGraph(
	const EGE::ReziAudio::SoundGraphAsset& graph)
{
	auto instance =
		std::make_unique<EGE::ReziAudio::SoundGraphInstance>();
	const EGE::ReziAudio::NodeRegistry registry;
	if (!instance->Load(graph, registry))
		return false;
	for (const auto& [name, value] : runtimeParameters_)
		instance->SetParameter(name, value);
	ReleaseVoice();
	graphAsset_ = graph;
	graphInstance_ = std::move(instance);
	return true;
}

void ComponentReziAudioEmitter::ClearSoundGraph()
{
	ReleaseVoice();
	graphInstance_.reset();
	graphAsset_.reset();
}

bool ComponentReziAudioEmitter::HasSoundGraph() const
{
	return graphInstance_ && graphInstance_->IsValid();
}

void ComponentReziAudioEmitter::SetRuntimeParameter(
	std::string_view name,
	const EGE::ReziAudio::ParameterValue& value)
{
	if (!name.empty())
	{
		runtimeParameters_[std::string(name)] = value;
		if (graphInstance_)
			graphInstance_->SetParameter(name, value);
		UpdateVoice();
	}
}

const EGE::ReziAudio::ParameterValue*
ComponentReziAudioEmitter::GetRuntimeParameter(
	std::string_view name) const
{
	const auto found = runtimeParameters_.find(std::string(name));
	if (found != runtimeParameters_.end())
		return &found->second;
	return graphInstance_ ? graphInstance_->GetParameter(name) : nullptr;
}

void ComponentReziAudioEmitter::ClearRuntimeParameters()
{
	runtimeParameters_.clear();
	if (graphAsset_)
	{
		const EGE::ReziAudio::NodeRegistry registry;
		graphInstance_->Load(*graphAsset_, registry);
	}
	UpdateVoice();
}

void ComponentReziAudioEmitter::UpdateVoice()
{
	if (!voice_.IsValid() || !App || !App->audio)
		return;

	auto& audio = App->audio->GetReziAudio();
	if (graphInstance_ && graphInstance_->IsValid())
	{
		const EGE::ReziAudio::SoundGraphEvaluation evaluation =
			graphInstance_->Evaluate();
		if (evaluation.succeeded)
			audio.SetSettings(voice_, evaluation.voice.settings);
	}
	else
		audio.SetSettings(voice_, settings);
	audio.SetTransform(voice_, BuildTransform());
}

void ComponentReziAudioEmitter::ReleaseVoice()
{
	if (voice_.IsValid() && App && App->audio)
		App->audio->GetReziAudio().DestroyVoice(voice_);
	else
		voice_ = {};
}

EGE::ReziAudio::AudioTransform
ComponentReziAudioEmitter::BuildTransform() const
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
