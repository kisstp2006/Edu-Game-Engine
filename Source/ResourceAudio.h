#ifndef __RESOURCE_AUDIO_H__
#define __RESOURCE_AUDIO_H__

#include "Resource.h"

struct ma_sound;

class ResourceAudio : public Resource
{
	friend class ModuleMeshes;

public:
	enum Format
	{
		sample,
		stream,
		unknown
	};

public:
	ResourceAudio(UID id);
	virtual ~ResourceAudio();

	Format GetFormat() const;
	const char* GetFormatStr() const;
	bool GetLoop() const { return loop; }
	float GetVolume() const { return volume; }
	float GetPitch() const { return pitch; }
	bool GetSpatial() const { return spatial; }
	float GetMinimumDistance() const { return minimumDistance; }
	float GetMaximumDistance() const { return maximumDistance; }

	void SetLoop(bool value);
	void SetVolume(float value);
	void SetPitch(float value);
	void SetSpatial(bool value);
	void SetDistanceRange(float minimum, float maximum);

	bool LoadInMemory() override;
    void ReleaseFromMemory () override;

	void Save(Config& config) const override;
	void Load(const Config& config) override;

public:
	ma_sound* sound = nullptr;
	Format format = unknown;
	bool loop = false;
	float volume = 1.0f;
	float pitch = 1.0f;
	bool spatial = false;
	float minimumDistance = 1.0f;
	float maximumDistance = 100.0f;
};

#endif // __RESOURCE_AUDIO_H__
