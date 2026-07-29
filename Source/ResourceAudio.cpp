#include "ResourceAudio.h"
#include "Application.h"
#include "ModuleAudio.h"
#include "Config.h"

#include "Leaks.h"

#include <algorithm>

// ---------------------------------------------------------
ResourceAudio::ResourceAudio(UID uid) : Resource(uid, Resource::Type::audio)
{}

// ---------------------------------------------------------
ResourceAudio::~ResourceAudio()
{
}

// ---------------------------------------------------------
ResourceAudio::Format ResourceAudio::GetFormat() const
{
	return format;
}

// ---------------------------------------------------------
const char * ResourceAudio::GetFormatStr() const
{
	const char* formats[] = { "Sample", "Stream", "Unknown" };
	return formats[format];
}

void ResourceAudio::SetLoop(bool value)
{
	loop = value;
	if (sound)
		ma_sound_set_looping(sound, value ? MA_TRUE : MA_FALSE);
}

void ResourceAudio::SetVolume(float value)
{
	volume = std::max(value, 0.0f);
	if (sound)
		ma_sound_set_volume(sound, volume);
}

void ResourceAudio::SetPitch(float value)
{
	pitch = std::max(value, 0.01f);
	if (sound)
		ma_sound_set_pitch(sound, pitch);
}

void ResourceAudio::SetSpatial(bool value)
{
	spatial = value;
	if (sound)
	{
		ma_sound_set_spatialization_enabled(
			sound, value ? MA_TRUE : MA_FALSE);
	}
}

void ResourceAudio::SetDistanceRange(
	float minimum,
	float maximum)
{
	minimumDistance = std::max(minimum, 0.0f);
	maximumDistance = std::max(maximum, minimumDistance);
	if (sound)
	{
		ma_sound_set_min_distance(sound, minimumDistance);
		ma_sound_set_max_distance(sound, maximumDistance);
	}
}

// ---------------------------------------------------------
bool ResourceAudio::LoadInMemory()
{
	return App->audio->Load(this);
}

// ---------------------------------------------------------
void ResourceAudio::ReleaseFromMemory ()
{
    // \todo:
}

// ---------------------------------------------------------
void ResourceAudio::Save(Config & config) const
{
	Resource::Save(config);
	config.AddInt("Format", format);
	config.AddBool("Loop", loop);
	config.AddFloat("Volume", volume);
	config.AddFloat("Pitch", pitch);
	config.AddBool("Spatial", spatial);
	config.AddFloat("Minimum Distance", minimumDistance);
	config.AddFloat("Maximum Distance", maximumDistance);
}

// ---------------------------------------------------------
void ResourceAudio::Load(const Config & config)
{
	Resource::Load(config);
	format = (Format) config.GetInt("Format", unknown);
	loop = config.GetBool("Loop", false);
	volume = config.GetFloat("Volume", 1.0f);
	pitch = config.GetFloat("Pitch", 1.0f);
	spatial = config.GetBool("Spatial", false);
	minimumDistance = config.GetFloat("Minimum Distance", 1.0f);
	maximumDistance = config.GetFloat("Maximum Distance", 100.0f);
}
