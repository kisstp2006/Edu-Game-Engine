#ifndef __MODULEAUDIO_H__
#define __MODULEAUDIO_H__

#include <vector>
#include "Module.h"
#include "miniaudio/miniaudio.h"

// miniaudio pulls in <windows.h> on Windows, which #defines CreateDirectory to
// CreateDirectoryA/W and breaks ModuleFileSystem::CreateDirectory() call sites
#ifdef CreateDirectory
#undef CreateDirectory
#endif

class GameObject;
class ComponentAudioListener;
class ComponentAudioSource;
class ResourceAudio;

class ModuleAudio : public Module
{
public:

	ModuleAudio(bool start_enabled = true);
	~ModuleAudio();

	bool Init(Config* config = nullptr) override;

	bool Start(Config* config = nullptr) override;
	update_status PostUpdate(float dt) override;
	bool CleanUp() override;

	void Save(Config* config) const override;
	void Load(Config* config) override;

	// Load audio assets
	bool Import(const char* file, std::string& output_file);
	bool Load(ResourceAudio* resource);
	void Unload(ma_sound* sound);

	float GetVolume() const;
	float GetMusicVolume() const;
	float GetFXVolume() const;

	void SetVolume(float new_volume);
	void SetMusicVolume(float new_music_volume);
	void SetFXVolume(float new_fx_volume);

private:

	void UpdateAudio();

	void UpdateListener(ComponentAudioListener* listener);
	void UpdateSource(ComponentAudioSource* source);

private:
	friend class ComponentAudioSource;
	friend class ComponentAudioListener;

	float volume = 1.0f;
	float music_volume = 1.0f;
	float fx_volume = 1.0f;
	std::vector<ComponentAudioSource*> sources;
	std::vector<ComponentAudioListener*> listeners;

	ma_engine engine;
	ma_sound_group music_group; // streams (music)
	ma_sound_group fx_group;    // samples (fx)
	bool engine_initialized = false;
};

#endif // __MODULEAUDIO_H__
