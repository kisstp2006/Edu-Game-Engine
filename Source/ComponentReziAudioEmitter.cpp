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

	if (graphInstance_ && graphInstance_->IsValid())
	{
		ReleaseVoice();
		const EGE::ReziAudio::SoundGraphEvaluation evaluation =
			graphInstance_->Evaluate();
		if (!evaluation.succeeded)
			return false;
		EGE::ReziAudio::VoiceCreateInfo createInfo = evaluation.voice;
		createInfo.transform = BuildTransform();
		voice_ = App->audio->GetReziAudio().CreateVoice(createInfo);
		return voice_.IsValid() &&
			App->audio->GetReziAudio().Play(voice_);
	}

	if (!voice_.IsValid())
	{
		EGE::ReziAudio::VoiceCreateInfo createInfo;
		if (dspStream_)
		{
			voice_ = App->audio->GetReziAudio().CreateStreamVoice(
				dspStream_, settings, BuildTransform());
			return voice_.IsValid() &&
				App->audio->GetReziAudio().Play(voice_);
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

bool ComponentReziAudioEmitter::PlayWithFade(float durationSeconds)
{
	const float targetVolume = std::max(settings.volume, 0.0f);
	const float fadeDuration = std::max(durationSeconds, 0.0f);
	if (fadeDuration <= 0.0f)
		return Play();
	if (!Play())
		return false;

	auto& audio = App->audio->GetReziAudio();
	return audio.FadeTo(voice_, 0.0f, 0.0f) &&
		audio.FadeTo(voice_, targetVolume, fadeDuration);
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

bool ComponentReziAudioEmitter::StopWithFade(float durationSeconds)
{
	return voice_.IsValid() && App && App->audio &&
		App->audio->GetReziAudio().StopWithFade(
			voice_, std::max(durationSeconds, 0.0f));
}

bool ComponentReziAudioEmitter::FadeTo(
	float targetVolume,
	float durationSeconds)
{
	return voice_.IsValid() && App && App->audio &&
		App->audio->GetReziAudio().FadeTo(
			voice_,
			std::max(targetVolume, 0.0f),
			std::max(durationSeconds, 0.0f));
}

bool ComponentReziAudioEmitter::Seek(float seconds)
{
	return voice_.IsValid() && App && App->audio &&
		App->audio->GetReziAudio().SeekSeconds(
			voice_, std::max(seconds, 0.0f));
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

bool ComponentReziAudioEmitter::IsPaused() const
{
	return GetPlaybackState() ==
		EGE::ReziAudio::PlaybackState::Paused;
}

bool ComponentReziAudioEmitter::IsFinished() const
{
	return GetPlaybackState() ==
		EGE::ReziAudio::PlaybackState::Finished;
}

float ComponentReziAudioEmitter::GetPlaybackSeconds() const
{
	return voice_.IsValid() && App && App->audio
		? App->audio->GetReziAudio().GetPlaybackSeconds(voice_)
		: 0.0f;
}

float ComponentReziAudioEmitter::GetPlaybackLengthSeconds() const
{
	return voice_.IsValid() && App && App->audio
		? App->audio->GetReziAudio().GetPlaybackLengthSeconds(voice_)
		: 0.0f;
}

float ComponentReziAudioEmitter::GetPlaybackPercentage() const
{
	return voice_.IsValid() && App && App->audio
		? App->audio->GetReziAudio().GetPlaybackPercentage(voice_)
		: 0.0f;
}

bool ComponentReziAudioEmitter::SetSoundGraph(
	const EGE::ReziAudio::SoundGraphAsset& graph)
{
	EGE::ReziAudio::SoundGraphAsset resolvedGraph = graph;
	ResolveAudioClips(resolvedGraph);
	auto instance =
		std::make_unique<EGE::ReziAudio::SoundGraphInstance>();
	const EGE::ReziAudio::NodeRegistry registry;
	if (!instance->Load(resolvedGraph, registry))
		return false;
	for (const auto& [name, value] : runtimeParameters_)
		instance->SetParameter(name, value);
	ReleaseVoice();
	ClearDspGraph();
	graphAsset_ = std::move(resolvedGraph);
	graphInstance_ = std::move(instance);
	return true;
}

bool ComponentReziAudioEmitter::SetDspGraph(
	const EGE::ReziAudio::DspGraphAsset& graph)
{
	EGE::ReziAudio::DspGraphAsset resolvedGraph = graph;
	ResolveAudioClips(resolvedGraph);
	std::vector<EGE::ReziAudio::DspDiagnostic> diagnostics;
	std::shared_ptr<EGE::ReziAudio::DspGraphStream> stream =
		EGE::ReziAudio::DspGraphStream::Compile(
			resolvedGraph, diagnostics);
	if (!stream)
		return false;
	ReleaseVoice();
	graphInstance_.reset();
	graphAsset_.reset();
	dspAsset_ = std::move(resolvedGraph);
	dspStream_ = std::move(stream);
	for (const auto& [name, value] : runtimeParameters_)
		ApplyDspParameter(name, value);
	return true;
}

void ComponentReziAudioEmitter::ClearSoundGraph()
{
	ReleaseVoice();
	graphInstance_.reset();
	graphAsset_.reset();
}

void ComponentReziAudioEmitter::ClearDspGraph()
{
	ReleaseVoice();
	dspStream_.reset();
	dspAsset_.reset();
}

bool ComponentReziAudioEmitter::HasSoundGraph() const
{
	return graphInstance_ && graphInstance_->IsValid();
}

bool ComponentReziAudioEmitter::HasDspGraph() const
{
	return dspStream_ != nullptr;
}

void ComponentReziAudioEmitter::SetRuntimeParameter(
	std::string_view name,
	const EGE::ReziAudio::ParameterValue& value)
{
	if (!name.empty())
	{
		EGE::ReziAudio::ParameterValue resolvedValue = value;
		if (auto* clip =
				std::get_if<EGE::ReziAudio::AudioClipReference>(
					&resolvedValue);
			clip && clip->assetId != 0)
		{
			const auto resolved = ResolveAudioClip(
				static_cast<UID>(clip->assetId));
			if (resolved.IsValid())
				*clip = resolved;
		}
		if (auto* clips =
				std::get_if<EGE::ReziAudio::AudioClipArray>(
					&resolvedValue))
		{
			for (EGE::ReziAudio::AudioClipReference& clip : *clips)
			{
				if (clip.assetId == 0)
					continue;
				const auto resolved = ResolveAudioClip(
					static_cast<UID>(clip.assetId));
				if (resolved.IsValid())
					clip = resolved;
			}
		}
		runtimeParameters_[std::string(name)] = resolvedValue;
		if (graphInstance_)
			graphInstance_->SetParameter(name, resolvedValue);
		ApplyDspParameter(name, resolvedValue);
		UpdateVoice();
	}
}

bool ComponentReziAudioEmitter::SetRuntimeAudioClipParameter(
	std::string_view name,
	UID audioResource)
{
	const EGE::ReziAudio::AudioClipReference clip =
		ResolveAudioClip(audioResource);
	if (audioResource != 0 && !clip.IsValid())
		return false;
	SetRuntimeParameter(name, clip);
	return true;
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
	if (dspAsset_)
	{
		std::vector<EGE::ReziAudio::DspDiagnostic> diagnostics;
		dspStream_ = EGE::ReziAudio::DspGraphStream::Compile(
			*dspAsset_, diagnostics);
	}
	UpdateVoice();
}

bool ComponentReziAudioEmitter::ApplyDspParameter(
	std::string_view name,
	const EGE::ReziAudio::ParameterValue& value)
{
	if (!dspStream_ || !dspAsset_)
		return false;
	float scalar = 0.0f;
	if (const float* number = std::get_if<float>(&value))
		scalar = *number;
	else if (const int* number = std::get_if<int>(&value))
		scalar = static_cast<float>(*number);
	else if (const bool* enabled = std::get_if<bool>(&value))
		scalar = *enabled ? 1.0f : 0.0f;
	else
		return false;

	const std::size_t separator = name.find_last_of('.');
	const std::string_view nodeName = separator == std::string_view::npos
		? std::string_view()
		: name.substr(0, separator);
	const std::string_view parameter = separator == std::string_view::npos
		? name
		: name.substr(separator + 1);
	bool applied = false;
	for (const EGE::ReziAudio::DspNodeAsset& node : dspAsset_->nodes)
	{
		if (!nodeName.empty() &&
			nodeName != node.name &&
			nodeName != std::to_string(node.id))
		{
			continue;
		}
		if (!node.parameters.contains(std::string(parameter)))
			continue;
		applied |= dspStream_->SetParameter(
			node.id, parameter, scalar);
	}
	return applied;
}

EGE::ReziAudio::AudioClipReference
ComponentReziAudioEmitter::ResolveAudioClip(UID audioResource) const
{
	if (audioResource == 0)
		return {};
	const Resource* resource =
		App && App->resources
			? App->resources->Get(audioResource)
			: nullptr;
	if (!resource || resource->GetType() != Resource::audio)
		return {};
	const char* exported = resource->GetExportedFile();
	return {
		static_cast<EGE::ReziAudio::AudioAssetId>(audioResource),
		exported ? exported : std::string()};
}

void ComponentReziAudioEmitter::ResolveAudioClips(
	EGE::ReziAudio::SoundGraphAsset& graph) const
{
	const auto resolve = [this](
		EGE::ReziAudio::ParameterValue& value)
	{
		auto* clip =
			std::get_if<EGE::ReziAudio::AudioClipReference>(&value);
		if (clip && clip->assetId != 0)
		{
			const auto resolved = ResolveAudioClip(
				static_cast<UID>(clip->assetId));
			if (resolved.IsValid())
				*clip = resolved;
		}
		auto* clips =
			std::get_if<EGE::ReziAudio::AudioClipArray>(&value);
		if (!clips)
			return;
		for (EGE::ReziAudio::AudioClipReference& item : *clips)
		{
			if (item.assetId == 0)
				continue;
			const auto resolved = ResolveAudioClip(
				static_cast<UID>(item.assetId));
			if (resolved.IsValid())
				item = resolved;
		}
	};

	for (EGE::ReziAudio::NamedParameter& parameter : graph.parameters)
		resolve(parameter.defaultValue);
	for (EGE::ReziAudio::GraphNode& node : graph.nodes)
	{
		for (EGE::ReziAudio::GraphPin& pin : node.inputs)
			resolve(pin.defaultValue);
		for (auto& [name, value] : node.properties)
			resolve(value);
	}
}

void ComponentReziAudioEmitter::ResolveAudioClips(
	EGE::ReziAudio::DspGraphAsset& graph) const
{
	for (EGE::ReziAudio::DspNodeAsset& node : graph.nodes)
	{
		if (node.clip.assetId == 0)
			continue;
		const auto resolved = ResolveAudioClip(
			static_cast<UID>(node.clip.assetId));
		if (resolved.IsValid())
			node.clip = resolved;
	}
}

void ComponentReziAudioEmitter::UpdateVoice()
{
	if (!voice_.IsValid() || !App || !App->audio)
		return;

	auto& audio = App->audio->GetReziAudio();
	if (dspStream_)
		audio.SetSettings(voice_, settings);
	else if (graphInstance_ && graphInstance_->IsValid())
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
