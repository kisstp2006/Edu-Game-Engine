#include "ResourceAudio.h"
#include "Application.h"
#include "ModuleAudio.h"
#include "Config.h"

#include "Leaks.h"

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
	const char* formats[] = { "Stream", "Sample", "Unknown" };
	return formats[format];
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
