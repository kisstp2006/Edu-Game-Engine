#include "Globals.h"
#include "Application.h"
#include "ModuleAudio.h"
#include "ModuleFileSystem.h"
#include "ModuleLevelManager.h"
#include "ModuleResources.h"
#include "ResourceAudio.h"
#include "GameObject.h"
#include "ComponentAudioListener.h"
#include "ComponentAudioSource.h"
#include "Config.h"

#include "Leaks.h"

using namespace std;

ModuleAudio::ModuleAudio( bool start_enabled) : Module("Audio", start_enabled)
{}

// Destructor
ModuleAudio::~ModuleAudio()
{}

// Called before render is available
bool ModuleAudio::Init(Config* config)
{
	bool ret = true;
	LOG("Loading Audio Mixer");

	ma_engine_config engine_config = ma_engine_config_init();
	engine_config.pResourceManagerVFS = (ma_vfs*) App->fs->GetAudioVFS();
	engine_config.listenerCount = 1;

	if (ma_engine_init(&engine_config, &engine) != MA_SUCCESS)
	{
		LOG("ma_engine_init() error");
		ret = false;
	}
	else
	{
		engine_initialized = true;
		LOG("Using miniaudio %s", ma_version_string());

		const ma_device* device = ma_engine_get_device(&engine);
		if (device != nullptr)
			LOG("Audio device in use: %s", device->playback.name);

		if (ma_sound_group_init(&engine, 0, nullptr, &music_group) != MA_SUCCESS)
			LOG("Could not create the music sound group");

		if (ma_sound_group_init(&engine, 0, nullptr, &fx_group) != MA_SUCCESS)
			LOG("Could not create the fx sound group");
	}

	// Settings
	if (config != nullptr && config->IsValid() == true)
	{
		SetVolume(config->GetFloat("Volume", 1.0f));
		SetMusicVolume(config->GetFloat("Music Volume", 1.0f));
		SetFXVolume(config->GetFloat("Fx Volume", 1.0f));
	}

	return ret;
}

bool ModuleAudio::Start(Config * config)
{
	return true;
}

update_status ModuleAudio::PostUpdate(float dt)
{
	UpdateAudio();

	return UPDATE_CONTINUE;
}

// Called before quitting
bool ModuleAudio::CleanUp()
{
	LOG("Freeing sound FX, closing Mixer and Audio subsystem");

	if (engine_initialized == true)
	{
		ma_sound_group_uninit(&fx_group);
		ma_sound_group_uninit(&music_group);
		ma_engine_uninit(&engine);
		engine_initialized = false;
	}

	return true;
}


void ModuleAudio::Save(Config * config) const
{
	config->AddFloat("Volume", GetVolume());
	config->AddFloat("Music Volume", GetMusicVolume());
	config->AddFloat("FX Volume", GetFXVolume());
}

void ModuleAudio::Load(Config * config)
{
	SetVolume(config->GetFloat("Volume", 1.0f));
	SetMusicVolume(config->GetFloat("Music Volume", 1.0f));
	SetFXVolume(config->GetFloat("Fx Volume", 1.0f));

}

bool ModuleAudio::Import(const char * full_path, string& output_file)
{
	// Try to load and free immediately to check if the resource is valid
	bool ret = false;
	string extension;

	if (full_path != nullptr)
	{
		App->fs->SplitFilePath(full_path, nullptr, nullptr, &extension);

		if (extension == "ogg" || extension == "wav")
		{
			ma_decoder_config decoder_config = ma_decoder_config_init_default();
			ma_decoder decoder;

			if (ma_decoder_init_vfs((ma_vfs*) App->fs->GetAudioVFS(), full_path, &decoder_config, &decoder) == MA_SUCCESS)
			{
				ma_decoder_uninit(&decoder);
				ret = true;
			}
			else
				LOG("miniaudio could not open [%s] to validate the import", full_path);
		}
	}

	// Just copy the file for now
	// TODO: decode and re-encode to ogg to save space
	if (ret == true)
	{
		char result[250];
		sprintf_s(result, 250, "%s%s_%llu.%s", LIBRARY_AUDIO_FOLDER, "audio", App->resources->GenerateNewUID(), extension.c_str());
		App->fs->NormalizePath(result);
		if (ret = App->fs->Copy(full_path, result))
			output_file = result;
	}

	return ret;
}

bool ModuleAudio::Load(ResourceAudio * resource)
{
	bool ret = false;

	if (resource != nullptr && resource->GetExportedFile())
	{
		string extension;
		App->fs->SplitFilePath(resource->GetExportedFile(), nullptr, nullptr, &extension);

		ma_sound_group* group = nullptr;
		bool should_loop = false;

		if (extension == "ogg")
		{
			// OGG files will be streams
			resource->format = ResourceAudio::stream;
			group = &music_group;
			should_loop = true;
		}
		else if (extension == "wav")
		{
			// WAV for samples
			resource->format = ResourceAudio::sample;
			group = &fx_group;
			should_loop = false;
		}

		if (group != nullptr)
		{
			ma_uint32 flags = (resource->format == ResourceAudio::stream) ? MA_SOUND_FLAG_STREAM : MA_SOUND_FLAG_DECODE;
			ma_sound* sound = new ma_sound;

			if (ma_sound_init_from_file(&engine, resource->GetExportedFile(), flags, group, nullptr, sound) == MA_SUCCESS)
			{
				ma_sound_set_looping(sound, should_loop ? MA_TRUE : MA_FALSE);
				resource->sound = sound;
				ret = true;
			}
			else
			{
				LOG("miniaudio could not load sound [%s]", resource->GetExportedFile());
				delete sound;
			}
		}
	}

	return ret;
}

void ModuleAudio::Unload(ma_sound* sound)
{
	if (sound != nullptr)
	{
		ma_sound_uninit(sound);
		delete sound;
	}
}

float ModuleAudio::GetVolume() const
{
	return volume;
}

float ModuleAudio::GetMusicVolume() const
{
	return music_volume;
}

float ModuleAudio::GetFXVolume() const
{
	return fx_volume;
}

void ModuleAudio::SetVolume(float new_volume)
{
	volume = new_volume;
	CAP(volume);

	if (engine_initialized == true)
		ma_engine_set_volume(&engine, volume);
}

void ModuleAudio::SetMusicVolume(float new_music_volume)
{
	music_volume = new_music_volume;
	CAP(music_volume);

	if (engine_initialized == true)
		ma_sound_set_volume(&music_group, music_volume);
}

void ModuleAudio::SetFXVolume(float new_fx_volume)
{
	fx_volume = new_fx_volume;
	CAP(fx_volume);

	if (engine_initialized == true)
		ma_sound_set_volume(&fx_group, fx_volume);
}

void ModuleAudio::UpdateAudio() const
{
	for (ComponentAudioListener* listener : listeners)
	{
		if (listener->IsActive())
			UpdateListener(listener);
	}

	for (ComponentAudioSource* source : sources)
	{
		if (source->IsActive())
			UpdateSource(source);
	}
}

void ModuleAudio::UpdateListener(ComponentAudioListener * listener) const
{
	// Update position and orientation
	const GameObject* go = listener->GetGameObject();
	float3 pos = go->GetGlobalTransformation().TranslatePart();
	float3 front = go->GetGlobalTransformation().WorldZ();
	float3 up = go->GetGlobalTransformation().WorldY();

	ma_engine_listener_set_position(&engine, 0, pos.x, pos.y, pos.z);
	ma_engine_listener_set_direction(&engine, 0, front.x, front.y, front.z);
	ma_engine_listener_set_world_up(&engine, 0, up.x, up.y, up.z);

	// miniaudio applies rolloff/doppler per-sound rather than globally like BASS did,
	// so propagate the listener's factors onto every active source to keep the same behaviour
	for (ComponentAudioSource* source : sources)
	{
		const ResourceAudio* resource = (const ResourceAudio*) source->GetResource();
		if (resource != nullptr && resource->sound != nullptr)
		{
			ma_sound_set_rolloff(resource->sound, listener->roll_off);
			ma_sound_set_doppler_factor(resource->sound, listener->doppler);
		}
	}
}

void ModuleAudio::UpdateSource(ComponentAudioSource* source) const
{
	if (source == nullptr)
		return;

	const ResourceAudio* resource = (const ResourceAudio*) source->GetResource();

	if (resource == nullptr || resource->sound == nullptr)
		return;

	ma_sound* sound = resource->sound;

	switch (source->current_state)
	{
		case ComponentAudioSource::state::playing:
		{
			// Setup 3D attributes for this gameobject
			ma_sound_set_spatialization_enabled(sound, source->is_2d ? MA_FALSE : MA_TRUE);
			ma_sound_set_min_distance(sound, source->min_distance);
			ma_sound_set_max_distance(sound, source->max_distance);
			ma_sound_set_cone(sound,
				source->cone_angle_in * (MA_PI / 180.0f),
				source->cone_angle_out * (MA_PI / 180.0f),
				source->out_cone_vol);

			// Update 3D position
			const GameObject* go = source->GetGameObject();
			float3 pos = go->GetGlobalPosition();
			float3 front = go->GetGlobalTransformation().WorldZ();
			ma_sound_set_position(sound, pos.x, pos.y, pos.z);
			ma_sound_set_direction(sound, front.x, front.y, front.z);
		} break;

		case ComponentAudioSource::state::waiting_to_play:
		{
			if (ma_sound_start(sound) != MA_SUCCESS)
				LOG("miniaudio could not start the sound");
			else
			{
				ma_sound_set_fade_in_milliseconds(sound, 0.0f, 1.0f, (ma_uint64) (source->fade_in * 1000.0f));
				source->current_state = ComponentAudioSource::state::playing;
			}
		} break;

		case ComponentAudioSource::state::waiting_to_stop:
		{
			if (ma_sound_stop_with_fade_in_milliseconds(sound, (ma_uint64) (source->fade_out * 1000.0f)) != MA_SUCCESS)
				LOG("miniaudio could not stop the sound");

			source->current_state = ComponentAudioSource::state::stopped;
		} break;

		case ComponentAudioSource::state::waiting_to_pause:
		{
			if (ma_sound_stop(sound) != MA_SUCCESS)
				LOG("miniaudio could not pause the sound");
			else
				source->current_state = ComponentAudioSource::state::paused;
		} break;

		case ComponentAudioSource::state::waiting_to_unpause:
		{
			if (ma_sound_start(sound) != MA_SUCCESS)
				LOG("miniaudio could not resume the sound");
			else
				source->current_state = ComponentAudioSource::state::playing;
		} break;
	}
}
