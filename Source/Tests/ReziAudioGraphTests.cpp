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
		clip.inputs[0].defaultValue = std::string("math.wav");
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
}

int main()
{
	TestRegistry();
	TestDefaultGraphAndLiveParameters();
	TestMathLogicAndAudioNodes();
	TestCompilerDiagnostics();
	TestParameterHashAndIsolation();
	std::cout << "ReziAudio graph tests passed.\n";
	return 0;
}
