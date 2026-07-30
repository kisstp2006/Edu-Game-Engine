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

using namespace EGE::ReziAudio;

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

	DspNodeAsset MakeNode(
		std::uint64_t id,
		DspNodeType type,
		std::vector<std::uint64_t> inputs = {})
	{
		DspNodeAsset node;
		node.id = id;
		node.type = type;
		node.name = FindDspNodeDescriptor(type)->displayName;
		node.inputs = std::move(inputs);
		node.parameters = CreateDspParameterDefaults(type);
		return node;
	}

	bool TestHazelNodeDescriptors()
	{
		const auto descriptors = GetDspNodeDescriptors();
		if (!Check(descriptors.size() == 23, "all DSP node descriptors"))
			return false;
		for (const DspNodeDescriptor& descriptor : descriptors)
		{
			if (!Check(
					FindDspNodeDescriptor(descriptor.type) != nullptr,
					"DSP descriptor lookup"))
			{
				return false;
			}
			if (!Check(
					descriptor.minimumInputs <= descriptor.maximumInputs,
					"DSP descriptor input range"))
			{
				return false;
			}
		}
		return Check(
			GetDspParameterDescriptors(DspNodeType::Reverb).size() == 6,
			"reverb exposes Hazel-style predelay and width");
	}

	bool TestGeneratorsEnvelopeAndAudioMath()
	{
		DspGraphAsset graphAsset;
		graphAsset.name = "Generators and audio math";
		graphAsset.nodes = {
			MakeNode(1, DspNodeType::SineOscillator),
			MakeNode(2, DspNodeType::RepeatTrigger),
			MakeNode(3, DspNodeType::ADEnvelope, {2}),
			MakeNode(4, DspNodeType::AudioMultiply, {1, 3}),
			MakeNode(5, DspNodeType::AudioClamp, {4}),
			MakeNode(6, DspNodeType::AudioMapRange, {5}),
			MakeNode(7, DspNodeType::Output, {6})};
		graphAsset.outputNode = 7;
		graphAsset.nodes[0].parameters["Frequency"] = 220.0f;
		graphAsset.nodes[1].parameters["Period"] = 0.02f;
		graphAsset.nodes[2].parameters["Attack"] = 0.005f;
		graphAsset.nodes[2].parameters["Decay"] = 0.01f;
		graphAsset.nodes[5].parameters["OutputMinimum"] = -0.5f;
		graphAsset.nodes[5].parameters["OutputMaximum"] = 0.5f;

		std::vector<DspDiagnostic> diagnostics;
		auto graph = DspGraphStream::Compile(graphAsset, diagnostics);
		if (!Check(graph != nullptr, "generator graph compilation") ||
			!Check(diagnostics.empty(), "generator graph diagnostics"))
		{
			return false;
		}

		std::vector<float> samples(4096 * 2);
		AllocationCount.store(0);
		TrackAllocations.store(true);
		const std::uint64_t read =
			graph->ReadFrames(samples.data(), 4096);
		TrackAllocations.store(false);
		if (!Check(read == 4096, "generator callback read") ||
			!Check(
				AllocationCount.load() == 0,
				"generator callback is allocation-free") ||
			!Check(
				IsFiniteAndAudible(samples),
				"sine/envelope/math output is audible"))
		{
			return false;
		}
		float peak = 0.0f;
		for (const float sample : samples)
			peak = std::max(peak, std::abs(sample));
		if (!Check(peak <= 0.5001f, "audio map range bounds output"))
			return false;

		for (int noiseType = 0; noiseType < 3; ++noiseType)
		{
			DspGraphAsset noiseAsset;
			noiseAsset.name = "Noise";
			noiseAsset.nodes = {
				MakeNode(1, DspNodeType::NoiseGenerator),
				MakeNode(2, DspNodeType::Output, {1})};
			noiseAsset.nodes[0].parameters["Seed"] = 42.0f;
			noiseAsset.nodes[0].parameters["Type"] =
				static_cast<float>(noiseType);
			noiseAsset.outputNode = 2;
			diagnostics.clear();
			auto noise =
				DspGraphStream::Compile(noiseAsset, diagnostics);
			if (!Check(noise != nullptr, "noise graph compilation"))
				return false;
			std::fill(samples.begin(), samples.end(), 0.0f);
			if (!Check(
					noise->ReadFrames(samples.data(), 1024) == 1024,
					"noise callback read") ||
				!Check(
					IsFiniteAndAudible(samples),
					"white/pink/brown noise is finite and audible"))
			{
				return false;
			}
		}
		return true;
	}

	bool TestDelayedTriggerAndCounter()
	{
		DspGraphAsset delayedAsset;
		delayedAsset.name = "Delayed trigger";
		delayedAsset.nodes = {
			MakeNode(1, DspNodeType::DelayedTrigger),
			MakeNode(2, DspNodeType::ADEnvelope, {1}),
			MakeNode(3, DspNodeType::Output, {2})};
		delayedAsset.outputNode = 3;
		delayedAsset.nodes[0].parameters["Delay"] = 0.01f;
		delayedAsset.nodes[1].parameters["Attack"] = 0.001f;
		delayedAsset.nodes[1].parameters["Decay"] = 0.01f;
		std::vector<DspDiagnostic> diagnostics;
		auto delayed =
			DspGraphStream::Compile(delayedAsset, diagnostics);
		if (!Check(delayed != nullptr, "delayed trigger compilation"))
			return false;

		std::vector<float> samples(800 * 2);
		if (!Check(
				delayed->ReadFrames(samples.data(), 800) == 800,
				"delayed trigger processing"))
			return false;
		float earlyPeak = 0.0f;
		float latePeak = 0.0f;
		for (std::size_t frame = 0; frame < 800; ++frame)
		{
			const float value = std::abs(samples[frame * 2]);
			if (frame < 470)
				earlyPeak = std::max(earlyPeak, value);
			else
				latePeak = std::max(latePeak, value);
		}
		if (!Check(earlyPeak < 0.00001f, "delayed trigger stays silent") ||
			!Check(latePeak > 0.01f, "delayed trigger fires envelope"))
		{
			return false;
		}

		DspGraphAsset counterAsset;
		counterAsset.name = "Trigger counter";
		counterAsset.nodes = {
			MakeNode(1, DspNodeType::RepeatTrigger),
			MakeNode(2, DspNodeType::TriggerCounter, {1}),
			MakeNode(3, DspNodeType::Output, {2})};
		counterAsset.outputNode = 3;
		counterAsset.nodes[0].parameters["Period"] = 0.002f;
		diagnostics.clear();
		auto counter =
			DspGraphStream::Compile(counterAsset, diagnostics);
		if (!Check(counter != nullptr, "trigger counter compilation"))
			return false;
		std::fill(samples.begin(), samples.end(), 0.0f);
		counter->ReadFrames(samples.data(), 800);
		float maximum = 0.0f;
		for (const float sample : samples)
			maximum = std::max(maximum, sample);
		return Check(maximum >= 8.0f, "trigger counter follows repeat events");
	}

	bool TestEveryAudioMathNodeCompiles()
	{
		const std::array mathTypes{
			DspNodeType::AudioAdd,
			DspNodeType::AudioSubtract,
			DspNodeType::AudioMultiply,
			DspNodeType::AudioMinimum,
			DspNodeType::AudioMaximum};
		for (const DspNodeType type : mathTypes)
		{
			DspGraphAsset asset;
			asset.nodes = {
				MakeNode(1, DspNodeType::SineOscillator),
				MakeNode(2, DspNodeType::NoiseGenerator),
				MakeNode(3, type, {1, 2}),
				MakeNode(4, DspNodeType::Output, {3})};
			asset.outputNode = 4;
			std::vector<DspDiagnostic> diagnostics;
			auto graph = DspGraphStream::Compile(asset, diagnostics);
			if (!Check(graph != nullptr, "audio math graph compilation"))
				return false;
			std::vector<float> output(256 * 2);
			if (!Check(
					graph->ReadFrames(output.data(), 256) == 256,
					"audio math graph processing"))
			{
				return false;
			}
			for (const float sample : output)
			{
				if (!Check(std::isfinite(sample), "audio math finite output"))
					return false;
			}
		}
		DspGraphAsset offsetAsset;
		offsetAsset.nodes = {
			MakeNode(1, DspNodeType::SineOscillator),
			MakeNode(2, DspNodeType::AudioOffset, {1}),
			MakeNode(3, DspNodeType::Output, {2})};
		offsetAsset.nodes[1].parameters["Value"] = 0.25f;
		offsetAsset.outputNode = 3;
		std::vector<DspDiagnostic> diagnostics;
		auto offset =
			DspGraphStream::Compile(offsetAsset, diagnostics);
		if (!Check(offset != nullptr, "audio plus float compilation"))
			return false;
		std::vector<float> output(64 * 2);
		offset->ReadFrames(output.data(), 64);
		return Check(
			std::abs(output.front() - 0.25f) < 0.0001f,
			"audio plus float processing");
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
	success &= TestHazelNodeDescriptors();
	success &= TestGeneratorsEnvelopeAndAudioMath();
	success &= TestDelayedTriggerAndCounter();
	success &= TestEveryAudioMathNodeCompiles();
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
	for (std::size_t sample = 0; sample < 512 * 2; ++sample)
		silentPeak = std::max(silentPeak, std::abs(output[sample]));
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
