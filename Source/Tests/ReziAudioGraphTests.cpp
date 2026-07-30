#include "../ReziAudioGraph.h"

#include <cassert>
#include <cmath>
#include <iostream>

using namespace EGE::ReziAudio;

namespace
{
	GraphNode& AddNode(
		SoundGraphAsset& graph,
		const NodeRegistry& registry,
		const char* type,
		std::uint64_t& nextNode,
		std::uint64_t& nextPin)
	{
		graph.nodes.push_back(
			registry.CreateNode(type, nextNode++, nextPin));
		return graph.nodes.back();
	}

	void Link(
		SoundGraphAsset& graph,
		std::uint64_t& nextLink,
		const GraphNode& outputNode,
		std::size_t output,
		const GraphNode& inputNode,
		std::size_t input)
	{
		graph.links.push_back({
			nextLink++,
			outputNode.outputs[output].id,
			inputNode.inputs[input].id});
	}

	void TestRegistry()
	{
		NodeRegistry registry;
		assert(registry.Descriptors().size() >= 55);
		assert(registry.Find("Math.Add"));
		assert(registry.Find("Audio.Output"));
		assert(registry.Find("Parameter.Vector3"));
		assert(registry.Find("Parameter.AudioClip"));
		assert(registry.Find("Parameter.AudioClipArray"));
		assert(registry.Find("Array.RandomAudioClip"));
		assert(registry.Find("Music.NoteToFrequency"));
		assert(registry.Find("Trigger.Counter"));
		assert(GetParameterTypeDescriptors().size() == 11);
		for (const ParameterTypeDescriptor& descriptor :
			GetParameterTypeDescriptors())
		{
			if (descriptor.type == ParameterValueType::Color ||
				descriptor.type == ParameterValueType::String)
			{
				assert(!descriptor.availableInAudioGraph);
			}
		}
	}

	void TestDefaultGraphAndLiveParameters()
	{
		NodeRegistry registry;
		SoundGraphAsset asset =
			CreateDefaultSoundGraph(registry, "test.wav");
		SoundGraphInstance instance;
		assert(instance.Load(asset, registry));
		assert(instance.SetParameter("Volume", 0.35f));
		assert(instance.SetParameter(
			HashAudioParameter("Pitch"), 1.5f));
		assert(instance.SetParameter(
			"Position", float3(4.0f, 2.0f, -1.0f)));
		assert(!instance.SetParameter("Volume", true));
		assert(!instance.SetParameter("Missing", 1.0f));

		const SoundGraphEvaluation result = instance.Evaluate();
		assert(result.succeeded);
		assert(result.voice.filePath == "test.wav");
		assert(std::abs(result.voice.settings.volume - 0.35f) < 0.0001f);
		assert(std::abs(result.voice.settings.pitch - 1.5f) < 0.0001f);
		assert(result.voice.settings.looping);
		assert(result.voice.transform.position.Equals(
			float3(4.0f, 2.0f, -1.0f), 0.0001f));
	}

	void TestMathLogicAndAudioNodes()
	{
		NodeRegistry registry;
		SoundGraphAsset graph;
		graph.name = "Math";
		graph.nodes.reserve(8);
		std::uint64_t nodeId = 1;
		std::uint64_t pinId = 100;
		std::uint64_t linkId = 1000;

		GraphNode& clip =
			AddNode(graph, registry, "Constant.Clip", nodeId, pinId);
		clip.inputs[0].defaultValue =
			AudioClipReference{101, "math.wav"};
		GraphNode& a =
			AddNode(graph, registry, "Constant.Float", nodeId, pinId);
		a.inputs[0].defaultValue = 2.0f;
		GraphNode& b =
			AddNode(graph, registry, "Constant.Float", nodeId, pinId);
		b.inputs[0].defaultValue = 3.0f;
		GraphNode& multiply =
			AddNode(graph, registry, "Math.Multiply", nodeId, pinId);
		GraphNode& greater =
			AddNode(graph, registry, "Logic.Greater", nodeId, pinId);
		GraphNode& select =
			AddNode(graph, registry, "Logic.SelectFloat", nodeId, pinId);
		select.inputs[1].defaultValue = 0.1f;
		select.inputs[2].defaultValue = 0.75f;
		GraphNode& output =
			AddNode(graph, registry, "Audio.Output", nodeId, pinId);

		Link(graph, linkId, clip, 0, output, 0);
		Link(graph, linkId, a, 0, multiply, 0);
		Link(graph, linkId, b, 0, multiply, 1);
		Link(graph, linkId, multiply, 0, greater, 0);
		Link(graph, linkId, b, 0, greater, 1);
		Link(graph, linkId, greater, 0, select, 0);
		Link(graph, linkId, select, 0, output, 1);

		SoundGraphInstance instance;
		assert(instance.Load(graph, registry));
		const SoundGraphEvaluation result = instance.Evaluate();
		assert(result.succeeded);
		assert(std::abs(result.voice.settings.volume - 0.75f) < 0.0001f);
	}

	void TestCompilerDiagnostics()
	{
		NodeRegistry registry;
		std::uint64_t nodeId = 1;
		std::uint64_t pinId = 100;
		std::uint64_t linkId = 1000;
		SoundGraphAsset graph;
		graph.nodes.reserve(3);
		GraphNode& a =
			AddNode(graph, registry, "Math.Add", nodeId, pinId);
		GraphNode& b =
			AddNode(graph, registry, "Math.Add", nodeId, pinId);
		GraphNode& output =
			AddNode(graph, registry, "Audio.Output", nodeId, pinId);
		Link(graph, linkId, a, 0, b, 0);
		Link(graph, linkId, b, 0, a, 0);
		(void)output;
		const CompiledSoundGraph compiled =
			SoundGraphCompiler::Compile(graph, registry);
		assert(!compiled.IsValid());
		assert(!compiled.diagnostics.empty());
	}

	void TestParameterHashAndIsolation()
	{
		assert(HashAudioParameter("Volume") ==
			HashAudioParameter("Volume"));
		assert(HashAudioParameter("Volume") !=
			HashAudioParameter("Pitch"));

		NodeRegistry registry;
		const SoundGraphAsset asset =
			CreateDefaultSoundGraph(registry, "voice.wav");
		SoundGraphInstance first;
		SoundGraphInstance second;
		assert(first.Load(asset, registry));
		assert(second.Load(asset, registry));
		assert(first.SetParameter("Volume", 0.2f));
		assert(second.SetParameter("Volume", 1.8f));
		assert(std::abs(
			first.Evaluate().voice.settings.volume - 0.2f) < 0.0001f);
		assert(std::abs(
			second.Evaluate().voice.settings.volume - 1.8f) < 0.0001f);
	}

	void TestGraphEditingOperations()
	{
		NodeRegistry registry;
		SoundGraphAsset graph;
		std::uint64_t nodeId = 1;
		std::uint64_t pinId = 100;
		GraphNode& add =
			AddNode(graph, registry, "Math.Add", nodeId, pinId);
		const std::uint64_t inputA = add.inputs[0].id;
		const std::uint64_t inputB = add.inputs[1].id;

		assert(CanPromoteInputToParameter(graph, inputA));
		const auto promoted = PromoteInputToParameter(
			graph,
			registry,
			inputA,
			float2(-250.0f, 0.0f));
		assert(promoted);
		assert(promoted->parameterName == "A");
		assert(graph.parameters.size() == 1);
		assert(graph.nodes.size() == 2);
		assert(graph.links.size() == 1);
		assert(IsGraphPinConnected(graph, inputA));
		assert(!CanPromoteInputToParameter(graph, inputA));

		assert(CanConnectParameterToInput(
			graph, inputB, promoted->parameterName));
		const auto connected = ConnectParameterToInput(
			graph,
			registry,
			inputB,
			promoted->parameterName,
			float2(-250.0f, 120.0f));
		assert(connected);
		assert(graph.nodes.size() == 3);
		assert(graph.links.size() == 2);
		assert(IsGraphPinConnected(graph, inputB));

		assert(DisconnectGraphPin(
			graph, promoted->outputPinId) == 1);
		assert(!IsGraphPinConnected(
			graph, promoted->outputPinId));
		assert(graph.links.size() == 1);
		assert(DisconnectGraphPin(graph, inputB) == 1);
		assert(graph.links.empty());

		nodeId = NextGraphNodeId(graph);
		pinId = NextGraphPinId(graph);
		GraphNode& clip = AddNode(
			graph, registry, "Constant.Clip", nodeId, pinId);
		assert(CanPromoteInputToParameter(
			graph, clip.inputs.front().id));
	}

	void TestRandomOneShotNode()
	{
		NodeRegistry registry;
		assert(registry.Find("Audio.RandomOneShot"));

		SoundGraphAsset graph;
		graph.nodes.reserve(5);
		std::uint64_t nodeId = 1;
		std::uint64_t pinId = 100;
		std::uint64_t linkId = 1000;
		GraphNode& clipA =
			AddNode(graph, registry, "Constant.Clip", nodeId, pinId);
		clipA.inputs[0].defaultValue =
			AudioClipReference{1, "a.wav"};
		GraphNode& clipB =
			AddNode(graph, registry, "Constant.Clip", nodeId, pinId);
		clipB.inputs[0].defaultValue =
			AudioClipReference{2, "b.wav"};
		GraphNode& clipC =
			AddNode(graph, registry, "Constant.Clip", nodeId, pinId);
		clipC.inputs[0].defaultValue =
			AudioClipReference{3, "c.wav"};
		GraphNode& random = AddNode(
			graph,
			registry,
			"Audio.RandomOneShot",
			nodeId,
			pinId);
		random.inputs.push_back(
			{pinId++, "Clip 3", GraphPinType::AudioClip,
			 AudioClipReference{}});
		GraphNode& output =
			AddNode(graph, registry, "Audio.Output", nodeId, pinId);
		output.inputs[4].defaultValue = true;

		Link(graph, linkId, clipA, 0, random, 0);
		Link(graph, linkId, clipB, 0, random, 1);
		Link(graph, linkId, clipC, 0, random, 2);
		Link(graph, linkId, random, 0, output, 0);

		SoundGraphInstance instance;
		assert(instance.Load(graph, registry));
		std::string previous;
		std::vector<std::string> seen;
		for (int run = 0; run < 24; ++run)
		{
			const SoundGraphEvaluation result = instance.Evaluate();
			assert(result.succeeded);
			assert(!result.voice.settings.looping);
			assert(
				result.voice.filePath == "a.wav" ||
				result.voice.filePath == "b.wav" ||
				result.voice.filePath == "c.wav");
			assert(
				previous.empty() ||
				result.voice.filePath != previous);
			if (std::find(
					seen.begin(),
					seen.end(),
					result.voice.filePath) == seen.end())
			{
				seen.push_back(result.voice.filePath);
			}
			previous = result.voice.filePath;
		}
		assert(seen.size() >= 2);
	}

	void TestAudioClipParameter()
	{
		NodeRegistry registry;
		SoundGraphAsset graph =
			CreateDefaultSoundGraph(
				registry,
				AudioClipReference{10, "default.wav"});
		graph.parameters.push_back({
			"RuntimeClip",
			HashAudioParameter("RuntimeClip"),
			AudioClipReference{10, "default.wav"}});

		SoundGraphInstance instance;
		assert(instance.Load(graph, registry));
		assert(instance.SetParameter(
			"RuntimeClip",
			AudioClipReference{20, "changed.wav"}));
		assert(!instance.SetParameter(
			"RuntimeClip",
			std::string("not-an-audio-asset")));
		const auto* value = instance.GetParameter("RuntimeClip");
		const auto* clip =
			value ? std::get_if<AudioClipReference>(value) : nullptr;
		assert(clip && clip->assetId == 20);
		assert(clip->resolvedSource == "changed.wav");
	}

	void TestTypedArraysAndHazelUtilityNodes()
	{
		NodeRegistry registry;
		SoundGraphAsset graph;
		graph.nodes.reserve(5);
		std::uint64_t nodeId = 1;
		std::uint64_t pinId = 100;
		std::uint64_t linkId = 1000;

		GraphNode& clips = AddNode(
			graph,
			registry,
			"Parameter.AudioClipArray",
			nodeId,
			pinId);
		clips.properties["Name"] = std::string("Variations");
		GraphNode& random = AddNode(
			graph,
			registry,
			"Array.RandomAudioClip",
			nodeId,
			pinId);
		GraphNode& bpm = AddNode(
			graph,
			registry,
			"Music.BPMToSeconds",
			nodeId,
			pinId);
		bpm.inputs[0].defaultValue = 120.0f;
		GraphNode& output =
			AddNode(graph, registry, "Audio.Output", nodeId, pinId);
		graph.parameters.push_back({
			"Variations",
			HashAudioParameter("Variations"),
			AudioClipArray{
				AudioClipReference{1, "one.wav"},
				AudioClipReference{2, "two.wav"},
				AudioClipReference{3, "three.wav"}}});

		Link(graph, linkId, clips, 0, random, 0);
		Link(graph, linkId, random, 0, output, 0);
		Link(graph, linkId, bpm, 0, output, 1);

		SoundGraphInstance instance;
		assert(instance.Load(graph, registry));
		std::string previous;
		for (int run = 0; run < 12; ++run)
		{
			const SoundGraphEvaluation result = instance.Evaluate();
			assert(result.succeeded);
			assert(
				result.voice.filePath == "one.wav" ||
				result.voice.filePath == "two.wav" ||
				result.voice.filePath == "three.wav");
			assert(
				previous.empty() ||
				result.voice.filePath != previous);
			assert(std::abs(
				result.voice.settings.volume - 0.5f) < 0.0001f);
			previous = result.voice.filePath;
		}
		assert(instance.SetParameter(
			"Variations",
			AudioClipArray{
				AudioClipReference{4, "changed.wav"}}));
		assert(
			instance.Evaluate().voice.filePath == "changed.wav");
	}

	void TestTriggerCounter()
	{
		NodeRegistry registry;
		SoundGraphAsset graph;
		graph.nodes.reserve(3);
		std::uint64_t nodeId = 1;
		std::uint64_t pinId = 100;
		std::uint64_t linkId = 1000;
		GraphNode& clip =
			AddNode(graph, registry, "Constant.Clip", nodeId, pinId);
		clip.inputs[0].defaultValue =
			AudioClipReference{1, "counter.wav"};
		GraphNode& counter =
			AddNode(graph, registry, "Trigger.Counter", nodeId, pinId);
		counter.inputs[2].defaultValue = 0;
		counter.inputs[3].defaultValue = 1;
		GraphNode& output =
			AddNode(graph, registry, "Audio.Output", nodeId, pinId);
		Link(graph, linkId, clip, 0, output, 0);
		Link(graph, linkId, counter, 0, output, 1);

		SoundGraphInstance instance;
		assert(instance.Load(graph, registry));
		assert(std::abs(
			instance.Evaluate().voice.settings.volume - 1.0f) <
			0.0001f);
		assert(std::abs(
			instance.Evaluate().voice.settings.volume - 2.0f) <
			0.0001f);
	}
}

int main()
{
	TestRegistry();
	TestDefaultGraphAndLiveParameters();
	TestMathLogicAndAudioNodes();
	TestCompilerDiagnostics();
	TestParameterHashAndIsolation();
	TestGraphEditingOperations();
	TestRandomOneShotNode();
	TestAudioClipParameter();
	TestTypedArraysAndHazelUtilityNodes();
	TestTriggerCounter();
	std::cout << "ReziAudio graph tests passed.\n";
	return 0;
}
