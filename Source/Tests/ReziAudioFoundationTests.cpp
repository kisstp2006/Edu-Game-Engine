#include "../ReziAudioSystem.h"

#include <miniaudio.h>

#include <array>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace
{
	bool WriteTestWave(const std::filesystem::path& path)
	{
		constexpr std::uint32_t sampleRate = 48000;
		constexpr std::uint16_t channels = 1;
		constexpr std::uint16_t bitsPerSample = 16;
		constexpr std::uint32_t frameCount = 4800;
		constexpr std::uint32_t dataSize =
			frameCount * channels * bitsPerSample / 8;
		constexpr std::uint32_t riffSize = 36 + dataSize;
		constexpr std::uint32_t byteRate =
			sampleRate * channels * bitsPerSample / 8;
		constexpr std::uint16_t blockAlign =
			channels * bitsPerSample / 8;

		std::ofstream output(path, std::ios::binary);
		if (!output)
			return false;

		const auto write = [&output](const auto& value)
		{
			output.write(
				reinterpret_cast<const char*>(&value),
				sizeof(value));
		};

		output.write("RIFF", 4);
		write(riffSize);
		output.write("WAVEfmt ", 8);
		constexpr std::uint32_t formatSize = 16;
		constexpr std::uint16_t pcm = 1;
		write(formatSize);
		write(pcm);
		write(channels);
		write(sampleRate);
		write(byteRate);
		write(blockAlign);
		write(bitsPerSample);
		output.write("data", 4);
		write(dataSize);

		for (std::uint32_t frame = 0; frame < frameCount; ++frame)
		{
			const float phase =
				static_cast<float>(frame) / sampleRate *
				440.0f * 6.28318530717958647692f;
			const std::int16_t sample = static_cast<std::int16_t>(
				std::sin(phase) * 5000.0f);
			write(sample);
		}
		return output.good();
	}

	bool Check(bool condition, const char* message)
	{
		if (condition)
			return true;
		std::cerr << "ReziAudio test failed: " << message << '\n';
		return false;
	}
}

int main()
{
	const std::filesystem::path testWave =
		std::filesystem::current_path() /
		"ege_reziaudio_foundation_test.wav";
	if (!WriteTestWave(testWave))
	{
		std::cerr << "Could not write test wave: "
			<< testWave.string() << '\n';
		return EXIT_FAILURE;
	}

	ma_engine_config engineConfig = ma_engine_config_init();
	engineConfig.noDevice = MA_TRUE;
	engineConfig.channels = 2;
	engineConfig.sampleRate = 48000;
	engineConfig.listenerCount = 1;
	ma_engine engine{};
	if (ma_engine_init(&engineConfig, &engine) != MA_SUCCESS)
	{
		std::cerr << "Could not initialize no-device miniaudio engine.\n";
		std::error_code error;
		std::filesystem::remove(testWave, error);
		return EXIT_FAILURE;
	}

	bool success = true;
	EGE::ReziAudio::System audio;
	success &= Check(audio.Initialize(engine), "backend initialization");
	success &= Check(audio.IsReady(), "backend readiness");

	EGE::ReziAudio::VoiceCreateInfo createInfo;
	createInfo.filePath = testWave.string();
	createInfo.settings.spatial.enabled = true;
	createInfo.settings.spatial.minDistance = 2.0f;
	createInfo.settings.spatial.maxDistance = 25.0f;
	createInfo.settings.looping = true;
	createInfo.transform.position = float3(5.0f, 0.0f, 0.0f);

	EGE::ReziAudio::PlaybackHandle first =
		audio.CreateVoice(createInfo);
	success &= Check(first.IsValid(), "voice creation");
	success &= Check(audio.Play(first), "voice playback");
	success &= Check(
		audio.GetState(first) ==
			EGE::ReziAudio::PlaybackState::Playing,
		"playing state");
	success &= Check(
		audio.GetPlaybackLengthSeconds(first) > 0.09f,
		"playback length query");
	success &= Check(
		audio.SeekSeconds(first, 0.05f),
		"seek in seconds");
	success &= Check(
		std::abs(audio.GetPlaybackSeconds(first) - 0.05f) < 0.002f,
		"playback cursor query");
	success &= Check(
		std::abs(audio.GetPlaybackPercentage(first) - 0.5f) < 0.03f,
		"playback percentage query");
	success &= Check(
		audio.FadeTo(first, 0.6f, 0.01f),
		"sample-accurate volume fade");

	createInfo.settings.volume = 0.35f;
	createInfo.settings.pitch = 1.25f;
	createInfo.settings.spatial.attenuation =
		EGE::ReziAudio::AttenuationModel::Linear;
	success &= Check(
		audio.SetSettings(first, createInfo.settings),
		"live voice settings");

	createInfo.transform.position = float3(-3.0f, 2.0f, 1.0f);
	createInfo.transform.velocity = float3(1.0f, 0.0f, 0.0f);
	success &= Check(
		audio.SetTransform(first, createInfo.transform),
		"live 3D transform");

	EGE::ReziAudio::AudioTransform listener;
	listener.position = float3::zero;
	success &= Check(
		audio.SetListener(listener),
		"listener transform");
	success &= Check(
		audio.SetBusVolume(
			EGE::ReziAudio::Bus::SoundEffects, 0.5f),
		"bus volume");
	success &= Check(
		std::abs(
			audio.GetBusVolume(
				EGE::ReziAudio::Bus::SoundEffects) - 0.5f) <
			0.0001f,
		"bus volume query");

	success &= Check(audio.Pause(first), "pause");
	success &= Check(
		audio.GetState(first) ==
			EGE::ReziAudio::PlaybackState::Paused,
		"paused state");
	success &= Check(audio.Play(first), "resume");
	success &= Check(
		audio.StopWithFade(first, 0.01f),
		"scheduled fade-out stop");

	const EGE::ReziAudio::PlaybackHandle stale = first;
	success &= Check(audio.DestroyVoice(first), "voice destruction");
	success &= Check(!first.IsValid(), "handle invalidation");
	success &= Check(
		audio.GetState(stale) ==
			EGE::ReziAudio::PlaybackState::Invalid,
		"stale generation rejection");

	EGE::ReziAudio::PlaybackHandle second =
		audio.CreateVoice(createInfo);
	success &= Check(second.IsValid(), "slot reuse");
	success &= Check(
		second.index != stale.index ||
			second.generation != stale.generation,
		"generation changes after slot reuse");
	audio.DestroyVoice(second);

	const EGE::ReziAudio::BackendStats stats = audio.GetStats();
	success &= Check(stats.activeVoices == 0, "voice cleanup");
	audio.Shutdown();
	ma_engine_uninit(&engine);

	std::error_code error;
	std::filesystem::remove(testWave, error);
	return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
