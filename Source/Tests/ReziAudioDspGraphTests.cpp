#include "../ReziAudioDspGraph.h"
#include "../ReziAudioSystem.h"

#include <miniaudio.h>

#include <atomic>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <new>
#include <vector>

namespace
{
	std::atomic<bool> TrackAllocations{false};
	std::atomic<std::uint64_t> AllocationCount{0};

	bool Check(bool condition, const char* message)
	{
		if (condition)
			return true;
		std::cerr << "ReziAudio DSP test failed: " << message << '\n';
		return false;
	}

	bool WriteWave(const std::filesystem::path& path)
	{
		constexpr std::uint32_t sampleRate = 48000;
		constexpr std::uint16_t channels = 2;
		constexpr std::uint16_t bits = 16;
		constexpr std::uint32_t frames = 12000;
		constexpr std::uint32_t dataSize =
			frames * channels * bits / 8;
		constexpr std::uint32_t riffSize = 36 + dataSize;
		constexpr std::uint32_t byteRate =
			sampleRate * channels * bits / 8;
		constexpr std::uint16_t blockAlign = channels * bits / 8;
		std::ofstream output(path, std::ios::binary);
		if (!output)
			return false;
		const auto write = [&output](const auto& value)
		{
			output.write(
				reinterpret_cast<const char*>(&value), sizeof(value));
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
		write(bits);
		output.write("data", 4);
		write(dataSize);
		for (std::uint32_t frame = 0; frame < frames; ++frame)
		{
			const float phase =
				static_cast<float>(frame) / sampleRate *
				330.0f * 6.28318530717958647692f;
			const std::int16_t left = static_cast<std::int16_t>(
				std::sin(phase) * 9000.0f);
			const std::int16_t right = static_cast<std::int16_t>(
				std::sin(phase * 1.01f) * 7000.0f);
			write(left);
			write(right);
		}
		return output.good();
	}

	bool IsFiniteAndAudible(const std::vector<float>& samples)
	{
		float peak = 0.0f;
		for (const float sample : samples)
		{
			if (!std::isfinite(sample))
				return false;
			peak = std::max(peak, std::abs(sample));
		}
		return peak > 0.0001f;
	}
}

void* operator new(std::size_t size)
{
	if (TrackAllocations.load(std::memory_order_relaxed))
		AllocationCount.fetch_add(1, std::memory_order_relaxed);
	if (void* memory = std::malloc(size))
		return memory;
	throw std::bad_alloc();
}

void operator delete(void* memory) noexcept
{
	std::free(memory);
}

void operator delete(void* memory, std::size_t) noexcept
{
	std::free(memory);
}

int main()
{
	using namespace EGE::ReziAudio;
	const std::filesystem::path wave =
		std::filesystem::current_path() / "ege_reziaudio_dsp_test.wav";
	if (!WriteWave(wave))
		return EXIT_FAILURE;

	bool success = true;
	std::vector<DspDiagnostic> diagnostics;
	DspGraphAsset asset = CreateDefaultDspGraph(wave.string());
	std::shared_ptr<DspGraphStream> graph =
		DspGraphStream::Compile(asset, diagnostics);
	success &= Check(graph != nullptr, "default graph compilation");
	success &= Check(diagnostics.empty(), "default graph diagnostics");
	if (!graph)
		return EXIT_FAILURE;

	std::vector<float> output(2048 * 2);
	AllocationCount.store(0);
	TrackAllocations.store(true);
	const std::uint64_t read =
		graph->ReadFrames(output.data(), 2048);
	TrackAllocations.store(false);
	success &= Check(read == 2048, "full callback read");
	success &= Check(
		AllocationCount.load() == 0,
		"no allocation in DSP callback");
	success &= Check(
		IsFiniteAndAudible(output),
		"filter/delay/reverb output is finite and audible");
	success &= Check(
		graph->GetStats().processedBlocks >= 4,
		"block processing statistics");

	graph->SeekFrame(0);
	success &= Check(
		graph->SetParameter(2, "Gain", 0.0f),
		"lock-free gain command");
	std::fill(output.begin(), output.end(), 1.0f);
	graph->ReadFrames(output.data(), 512);
	float silentPeak = 0.0f;
	for (const float sample : output)
		silentPeak = std::max(silentPeak, std::abs(sample));
	success &= Check(
		silentPeak < 0.00001f,
		"runtime parameter reaches the callback");

	std::size_t rejected = 0;
	for (int index = 0; index < 400; ++index)
	{
		rejected += !graph->SetParameter(
			2, "Gain", static_cast<float>(index) / 400.0f);
	}
	success &= Check(rejected != 0, "bounded command queue overflow");
	success &= Check(
		graph->GetStats().droppedParameterCommands != 0,
		"command overflow diagnostics");

	DspGraphAsset cyclic = asset;
	cyclic.nodes[0].inputs = {7};
	std::vector<DspDiagnostic> invalidDiagnostics;
	success &= Check(
		!DspGraphStream::Compile(cyclic, invalidDiagnostics),
		"cycle rejection");
	success &= Check(
		!invalidDiagnostics.empty(),
		"cycle diagnostic");

	std::vector<DspDiagnostic> backendDiagnostics;
	std::shared_ptr<DspGraphStream> backendGraph =
		DspGraphStream::Compile(asset, backendDiagnostics);
	ma_engine_config engineConfig = ma_engine_config_init();
	engineConfig.noDevice = MA_TRUE;
	engineConfig.channels = 2;
	engineConfig.sampleRate = 48000;
	ma_engine engine{};
	success &= Check(
		ma_engine_init(&engineConfig, &engine) == MA_SUCCESS,
		"no-device engine initialization");
	System audio;
	success &= Check(audio.Initialize(engine), "audio system initialization");
	VoiceSettings settings;
	settings.spatial.enabled = false;
	const PlaybackHandle voice =
		audio.CreateStreamVoice(backendGraph, settings);
	success &= Check(voice.IsValid(), "custom data-source voice creation");
	success &= Check(audio.Play(voice), "custom data-source playback");
	std::vector<float> engineOutput(1024 * 2);
	ma_uint64 engineFrames = 0;
	success &= Check(
		ma_engine_read_pcm_frames(
			&engine,
			engineOutput.data(),
			1024,
			&engineFrames) == MA_SUCCESS,
		"miniaudio callback processing");
	success &= Check(engineFrames == 1024, "miniaudio frame count");
	success &= Check(
		backendGraph->GetStats().processedFrames >= 1024,
		"DSP graph executed through miniaudio");
	PlaybackHandle mutableVoice = voice;
	audio.DestroyVoice(mutableVoice);
	audio.Shutdown();
	ma_engine_uninit(&engine);

	std::error_code error;
	std::filesystem::remove(wave, error);
	return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
