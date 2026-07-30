#include "ReziAudioGraph.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <unordered_set>

namespace EGE::ReziAudio
{
	namespace
	{
		NodePinDefinition Pin(
			std::string name,
			GraphPinType type,
			ParameterValue value)
		{
			return {std::move(name), type, std::move(value)};
		}

		NodeDescriptor Node(
			std::string type,
			std::string name,
			std::string category,
			std::vector<NodePinDefinition> inputs,
			std::vector<NodePinDefinition> outputs,
			std::map<std::string, ParameterValue> properties = {})
		{
			return {
				std::move(type),
				std::move(name),
				std::move(category),
				std::move(inputs),
				std::move(outputs),
				std::move(properties)};
		}

		float AsFloat(const ParameterValue& value, float fallback = 0.0f)
		{
			if (const float* result = std::get_if<float>(&value))
				return *result;
			if (const int* result = std::get_if<int>(&value))
				return static_cast<float>(*result);
			if (const bool* result = std::get_if<bool>(&value))
				return *result ? 1.0f : 0.0f;
			return fallback;
		}

		int AsInteger(const ParameterValue& value, int fallback = 0)
		{
			if (const int* result = std::get_if<int>(&value))
				return *result;
			if (const float* result = std::get_if<float>(&value))
				return static_cast<int>(*result);
			if (const bool* result = std::get_if<bool>(&value))
				return *result ? 1 : 0;
			return fallback;
		}

		bool AsBool(const ParameterValue& value, bool fallback = false)
		{
			if (const bool* result = std::get_if<bool>(&value))
				return *result;
			if (const float* result = std::get_if<float>(&value))
				return *result != 0.0f;
			if (const int* result = std::get_if<int>(&value))
				return *result != 0;
			return fallback;
		}

		float3 AsVector3(
			const ParameterValue& value,
			const float3& fallback = float3::zero)
		{
			if (const float3* result = std::get_if<float3>(&value))
				return *result;
			return fallback;
		}

		std::string AsString(
			const ParameterValue& value,
			std::string fallback = {})
		{
			if (const std::string* result = std::get_if<std::string>(&value))
				return *result;
			return fallback;
		}

		AudioClipReference AsAudioClip(
			const ParameterValue& value,
			AudioClipReference fallback = {})
		{
			if (const AudioClipReference* result =
					std::get_if<AudioClipReference>(&value))
			{
				return *result;
			}
			return fallback;
		}

		FloatArray AsFloatArray(
			const ParameterValue& value,
			FloatArray fallback = {})
		{
			if (const FloatArray* result =
					std::get_if<FloatArray>(&value))
				return *result;
			return fallback;
		}

		IntegerArray AsIntegerArray(
			const ParameterValue& value,
			IntegerArray fallback = {})
		{
			if (const IntegerArray* result =
					std::get_if<IntegerArray>(&value))
				return *result;
			return fallback;
		}

		AudioClipArray AsAudioClipArray(
			const ParameterValue& value,
			AudioClipArray fallback = {})
		{
			if (const AudioClipArray* result =
					std::get_if<AudioClipArray>(&value))
				return *result;
			return fallback;
		}

		bool Compatible(GraphPinType output, GraphPinType input)
		{
			if (output == input)
				return true;
			if ((output == GraphPinType::Integer &&
				 input == GraphPinType::Float) ||
				(output == GraphPinType::Float &&
				 input == GraphPinType::Integer))
			{
				return true;
			}
			return false;
		}

		const GraphPin* FindPin(
			const SoundGraphAsset& asset,
			std::uint64_t id,
			bool& isOutput,
			const GraphNode** owner = nullptr)
		{
			for (const GraphNode& node : asset.nodes)
			{
				for (const GraphPin& pin : node.inputs)
				{
					if (pin.id == id)
					{
						isOutput = false;
						if (owner)
							*owner = &node;
						return &pin;
					}
				}
				for (const GraphPin& pin : node.outputs)
				{
					if (pin.id == id)
					{
						isOutput = true;
						if (owner)
							*owner = &node;
						return &pin;
					}
				}
			}
			return nullptr;
		}

		const ParameterValue* Property(
			const GraphNode& node,
			std::string_view name)
		{
			const auto found = node.properties.find(std::string(name));
			return found == node.properties.end() ? nullptr : &found->second;
		}

		void AddDiagnostic(
			std::vector<GraphDiagnostic>& diagnostics,
			GraphDiagnosticSeverity severity,
			std::string message,
			std::uint64_t nodeId = 0,
			std::uint64_t pinId = 0)
		{
			diagnostics.push_back(
				{severity, nodeId, pinId, std::move(message)});
		}
	}

	NodeRegistry::NodeRegistry()
	{
		const auto floatIn = [](
			const char* name, float value = 0.0f)
		{
			return Pin(name, GraphPinType::Float, value);
		};
		const auto floatOut = [](
			const char* name = "Value")
		{
			return Pin(name, GraphPinType::Float, 0.0f);
		};
		const auto boolIn = [](
			const char* name, bool value = false)
		{
			return Pin(name, GraphPinType::Bool, value);
		};
		const auto boolOut = [](
			const char* name = "Value")
		{
			return Pin(name, GraphPinType::Bool, false);
		};
		const auto vectorIn = [](
			const char* name, float3 value = float3::zero)
		{
			return Pin(name, GraphPinType::Vector3, value);
		};
		const auto vectorOut = [](
			const char* name = "Value")
		{
			return Pin(name, GraphPinType::Vector3, float3::zero);
		};

		descriptors_ = {
			Node("Constant.Float", "Float", "Input",
				{floatIn("Value")}, {floatOut()}),
			Node("Constant.Integer", "Integer", "Input",
				{Pin("Value", GraphPinType::Integer, 0)},
				{Pin("Value", GraphPinType::Integer, 0)}),
			Node("Constant.Bool", "Bool", "Input",
				{boolIn("Value")}, {boolOut()}),
			Node("Constant.Vector3", "Vector 3", "Input",
				{vectorIn("Value")}, {vectorOut()}),
			Node("Constant.Vector2", "Vector 2", "Input",
				{Pin("Value", GraphPinType::Vector2, float2::zero)},
				{Pin("Value", GraphPinType::Vector2, float2::zero)}),
			Node("Constant.Color", "Color", "Input",
				{Pin("Value", GraphPinType::Color,
					float4(1.0f, 1.0f, 1.0f, 1.0f))},
				{Pin("Value", GraphPinType::Color,
					float4(1.0f, 1.0f, 1.0f, 1.0f))}),
			Node("Constant.Clip", "Audio Clip", "Input",
				{Pin("Asset", GraphPinType::AudioClip,
					AudioClipReference{})},
				{Pin("Clip", GraphPinType::AudioClip,
					AudioClipReference{})}),
			Node("Parameter.Float", "Float Parameter", "Parameters",
				{floatIn("Fallback")}, {floatOut()},
				{{"Name", std::string("Volume")}}),
			Node("Parameter.Integer", "Integer Parameter", "Parameters",
				{Pin("Fallback", GraphPinType::Integer, 0)},
				{Pin("Value", GraphPinType::Integer, 0)},
				{{"Name", std::string("Count")}}),
			Node("Parameter.Bool", "Bool Parameter", "Parameters",
				{boolIn("Fallback")}, {boolOut()},
				{{"Name", std::string("Enabled")}}),
			Node("Parameter.Vector3", "Vector 3 Parameter", "Parameters",
				{vectorIn("Fallback")}, {vectorOut()},
				{{"Name", std::string("Position")}}),
			Node("Parameter.Vector2", "Vector 2 Parameter", "Parameters",
				{Pin("Fallback", GraphPinType::Vector2, float2::zero)},
				{Pin("Value", GraphPinType::Vector2, float2::zero)},
				{{"Name", std::string("Vector2")}}),
			Node("Parameter.Color", "Color Parameter", "Parameters",
				{Pin("Fallback", GraphPinType::Color,
					float4(1.0f, 1.0f, 1.0f, 1.0f))},
				{Pin("Value", GraphPinType::Color,
					float4(1.0f, 1.0f, 1.0f, 1.0f))},
				{{"Name", std::string("Color")}}),
			Node("Parameter.String", "String Parameter", "Parameters",
				{Pin("Fallback", GraphPinType::String, std::string())},
				{Pin("Value", GraphPinType::String, std::string())},
				{{"Name", std::string("String")}}),
			Node("Parameter.AudioClip", "Audio Clip Parameter", "Parameters",
				{Pin("Fallback", GraphPinType::AudioClip,
					AudioClipReference{})},
				{Pin("Value", GraphPinType::AudioClip,
					AudioClipReference{})},
				{{"Name", std::string("Clip")}}),
			Node("Parameter.FloatArray", "Float Array Parameter", "Parameters",
				{Pin("Fallback", GraphPinType::FloatArray, FloatArray{})},
				{Pin("Value", GraphPinType::FloatArray, FloatArray{})},
				{{"Name", std::string("FloatArray")}}),
			Node("Parameter.IntegerArray", "Integer Array Parameter", "Parameters",
				{Pin("Fallback", GraphPinType::IntegerArray, IntegerArray{})},
				{Pin("Value", GraphPinType::IntegerArray, IntegerArray{})},
				{{"Name", std::string("IntArray")}}),
			Node("Parameter.AudioClipArray", "Audio Clip Array Parameter", "Parameters",
				{Pin("Fallback", GraphPinType::AudioClipArray,
					AudioClipArray{})},
				{Pin("Value", GraphPinType::AudioClipArray,
					AudioClipArray{})},
				{{"Name", std::string("ClipArray")}}),
			Node("Array.GetFloat", "Get (Float)", "Array",
				{
					Pin("Array", GraphPinType::FloatArray, FloatArray{}),
					Pin("Index", GraphPinType::Integer, 0)
				},
				{Pin("Element", GraphPinType::Float, 0.0f)}),
			Node("Array.GetInteger", "Get (Integer)", "Array",
				{
					Pin("Array", GraphPinType::IntegerArray, IntegerArray{}),
					Pin("Index", GraphPinType::Integer, 0)
				},
				{Pin("Element", GraphPinType::Integer, 0)}),
			Node("Array.GetAudioClip", "Get (Audio Clip)", "Array",
				{
					Pin("Array", GraphPinType::AudioClipArray,
						AudioClipArray{}),
					Pin("Index", GraphPinType::Integer, 0)
				},
				{Pin("Element", GraphPinType::AudioClip,
					AudioClipReference{})}),
			Node("Array.RandomFloat", "Get Random (Float)", "Array",
				{Pin("Array", GraphPinType::FloatArray, FloatArray{})},
				{Pin("Element", GraphPinType::Float, 0.0f)}),
			Node("Array.RandomInteger", "Get Random (Integer)", "Array",
				{Pin("Array", GraphPinType::IntegerArray, IntegerArray{})},
				{Pin("Element", GraphPinType::Integer, 0)}),
			Node("Array.RandomAudioClip", "Get Random (Audio Clip)", "Array",
				{Pin("Array", GraphPinType::AudioClipArray,
					AudioClipArray{})},
				{Pin("Element", GraphPinType::AudioClip,
					AudioClipReference{})}),
			Node("Math.Add", "Add", "Math",
				{floatIn("A"), floatIn("B")}, {floatOut()}),
			Node("Math.Subtract", "Subtract", "Math",
				{floatIn("A"), floatIn("B")}, {floatOut()}),
			Node("Math.Multiply", "Multiply", "Math",
				{floatIn("Value"), floatIn("Multiplier", 1.0f)},
				{floatOut()}),
			Node("Math.Divide", "Divide", "Math",
				{floatIn("Value"), floatIn("Denominator", 1.0f)},
				{floatOut()}),
			Node("Math.Logarithm", "Logarithm", "Math",
				{floatIn("Value", 1.0f), floatIn("Base", 10.0f)},
				{floatOut()}),
			Node("Math.Modulo", "Modulo", "Math",
				{
					Pin("Value", GraphPinType::Integer, 0),
					Pin("Modulo", GraphPinType::Integer, 1)
				},
				{Pin("Value", GraphPinType::Integer, 0)}),
			Node("Math.Min", "Minimum", "Math",
				{floatIn("A"), floatIn("B")}, {floatOut()}),
			Node("Math.Max", "Maximum", "Math",
				{floatIn("A"), floatIn("B")}, {floatOut()}),
			Node("Math.Clamp", "Clamp", "Math",
				{floatIn("Value"), floatIn("Min"), floatIn("Max", 1.0f)},
				{floatOut()}),
			Node("Math.Abs", "Absolute", "Math",
				{floatIn("Value")}, {floatOut()}),
			Node("Math.Negate", "Negate", "Math",
				{floatIn("Value")}, {floatOut()}),
			Node("Math.Lerp", "Lerp", "Math",
				{floatIn("A"), floatIn("B", 1.0f), floatIn("Alpha")},
				{floatOut()}),
			Node("Math.Remap", "Map Range", "Math",
				{floatIn("Value"), floatIn("In Min"),
				 floatIn("In Max", 1.0f), floatIn("Out Min"),
				 floatIn("Out Max", 1.0f)},
				{floatOut()}),
			Node("Math.Random", "Random Float", "Math",
				{floatIn("Min"), floatIn("Max", 1.0f)}, {floatOut()}),
			Node("Math.Sine", "Sine", "Math",
				{floatIn("Radians")}, {floatOut()}),
			Node("Math.Cosine", "Cosine", "Math",
				{floatIn("Radians")}, {floatOut()}),
			Node("Math.Power", "Power", "Math",
				{floatIn("Value"), floatIn("Exponent", 2.0f)},
				{floatOut()}),
			Node("Math.SquareRoot", "Square Root", "Math",
				{floatIn("Value")}, {floatOut()}),
			Node("Math.Floor", "Floor", "Math",
				{floatIn("Value")}, {floatOut()}),
			Node("Math.Ceil", "Ceil", "Math",
				{floatIn("Value")}, {floatOut()}),
			Node("Math.Round", "Round", "Math",
				{floatIn("Value")}, {floatOut()}),
			Node("Audio.LinearToLogFrequency",
				"Linear to Log Frequency", "Audio Utilities",
				{
					floatIn("Value", 0.5f),
					floatIn("Min"),
					floatIn("Max", 1.0f),
					floatIn("Min Frequency", 20.0f),
					floatIn("Max Frequency", 20000.0f)
				},
				{floatOut("Frequency")}),
			Node("Audio.FrequencyToLinear",
				"Log Frequency to Linear", "Audio Utilities",
				{
					floatIn("Frequency", 1000.0f),
					floatIn("Min Frequency", 20.0f),
					floatIn("Max Frequency", 20000.0f),
					floatIn("Min"),
					floatIn("Max", 1.0f)
				},
				{floatOut("Value")}),
			Node("Music.BPMToSeconds", "BPM to Seconds", "Music",
				{floatIn("BPM", 90.0f)}, {floatOut("Seconds")}),
			Node("Music.NoteToFrequency", "Note to Frequency", "Music",
				{floatIn("MIDI Note", 60.0f)}, {floatOut("Frequency")}),
			Node("Music.FrequencyToNote", "Frequency to Note", "Music",
				{floatIn("Frequency", 440.0f)}, {floatOut("MIDI Note")}),
			Node("Trigger.Counter", "Trigger Counter", "Trigger",
				{
					boolIn("Trigger", true),
					boolIn("Reset"),
					Pin("Start", GraphPinType::Integer, 0),
					Pin("Step", GraphPinType::Integer, 1),
					Pin("Reset Count", GraphPinType::Integer, 0)
				},
				{Pin("Value", GraphPinType::Integer, 0)}),
			Node("Logic.Greater", "Greater", "Logic",
				{floatIn("A"), floatIn("B")}, {boolOut()}),
			Node("Logic.Less", "Less", "Logic",
				{floatIn("A"), floatIn("B")}, {boolOut()}),
			Node("Logic.Equal", "Nearly Equal", "Logic",
				{floatIn("A"), floatIn("B"),
				 floatIn("Tolerance", 0.0001f)},
				{boolOut()}),
			Node("Logic.And", "And", "Logic",
				{boolIn("A"), boolIn("B")}, {boolOut()}),
			Node("Logic.Or", "Or", "Logic",
				{boolIn("A"), boolIn("B")}, {boolOut()}),
			Node("Logic.Not", "Not", "Logic",
				{boolIn("Value")}, {boolOut()}),
			Node("Logic.SelectFloat", "Select Float", "Logic",
				{boolIn("Condition"), floatIn("False"),
				 floatIn("True", 1.0f)},
				{floatOut()}),
			Node("Logic.SelectBool", "Select Bool", "Logic",
				{boolIn("Condition"), boolIn("False"), boolIn("True", true)},
				{boolOut()}),
			Node("Logic.SelectVector3", "Select Vector 3", "Logic",
				{boolIn("Condition"), vectorIn("False"),
				 vectorIn("True")},
				{vectorOut()}),
			Node("Vector.Compose", "Make Vector 3", "Vector",
				{floatIn("X"), floatIn("Y"), floatIn("Z")},
				{vectorOut()}),
			Node("Vector.X", "Vector X", "Vector",
				{vectorIn("Vector")}, {floatOut()}),
			Node("Vector.Y", "Vector Y", "Vector",
				{vectorIn("Vector")}, {floatOut()}),
			Node("Vector.Z", "Vector Z", "Vector",
				{vectorIn("Vector")}, {floatOut()}),
			Node("Vector.Length", "Vector Length", "Vector",
				{vectorIn("Vector")}, {floatOut("Length")}),
			Node("Vector.Distance", "Distance", "Vector",
				{vectorIn("A"), vectorIn("B")}, {floatOut()}),
			Node("Vector.Add", "Add Vectors", "Vector",
				{vectorIn("A"), vectorIn("B")}, {vectorOut()}),
			Node("Vector.Subtract", "Subtract Vectors", "Vector",
				{vectorIn("A"), vectorIn("B")}, {vectorOut()}),
			Node("Vector.Scale", "Scale Vector", "Vector",
				{vectorIn("Vector"), floatIn("Scale", 1.0f)},
				{vectorOut()}),
			Node("Vector.Normalize", "Normalize", "Vector",
				{vectorIn("Vector")}, {vectorOut()}),
			Node("Vector.Dot", "Dot Product", "Vector",
				{vectorIn("A"), vectorIn("B")}, {floatOut()}),
			Node("Vector.Cross", "Cross Product", "Vector",
				{vectorIn("A"), vectorIn("B")}, {vectorOut()}),
			Node("Audio.Gain", "Gain", "Audio",
				{floatIn("Value", 1.0f), floatIn("Gain", 1.0f)},
				{floatOut("Volume")}),
			Node("Audio.Pitch", "Pitch", "Audio",
				{floatIn("Semitones")}, {floatOut("Ratio")}),
			Node("Audio.Pan", "Stereo Pan", "Audio",
				{floatIn("Pan")}, {floatOut("Pan")}),
			Node("Audio.Attenuation", "Distance Attenuation", "Audio",
				{floatIn("Distance"), floatIn("Min Distance", 1.0f),
				 floatIn("Max Distance", 100.0f),
				 floatIn("Rolloff", 1.0f)},
				{floatOut("Gain")}),
			Node("Audio.RandomOneShot", "Random One Shot", "Audio",
				{
					Pin("Clip 1", GraphPinType::AudioClip,
						AudioClipReference{}),
					Pin("Clip 2", GraphPinType::AudioClip,
						AudioClipReference{})
				},
				{Pin("Clip", GraphPinType::AudioClip,
					AudioClipReference{})}),
			Node("Audio.Output", "Audio Output", "Output",
				{
					Pin("Clip", GraphPinType::AudioClip,
						AudioClipReference{}),
					floatIn("Volume", 1.0f),
					floatIn("Pitch", 1.0f),
					floatIn("Pan"),
					boolIn("Loop"),
					boolIn("Spatial", true),
					floatIn("Min Distance", 1.0f),
					floatIn("Max Distance", 100.0f),
					floatIn("Rolloff", 1.0f),
					floatIn("Doppler", 1.0f),
					vectorIn("Position"),
					vectorIn("Velocity")
				},
				{})
		};
	}

	const NodeDescriptor* NodeRegistry::Find(std::string_view type) const
	{
		const auto found = std::find_if(
			descriptors_.begin(),
			descriptors_.end(),
			[type](const NodeDescriptor& descriptor)
			{
				return descriptor.type == type;
			});
		return found == descriptors_.end() ? nullptr : &*found;
	}

	const std::vector<NodeDescriptor>& NodeRegistry::Descriptors() const
	{
		return descriptors_;
	}

	GraphNode NodeRegistry::CreateNode(
		std::string_view type,
		std::uint64_t nodeId,
		std::uint64_t& nextPinId) const
	{
		GraphNode node;
		const NodeDescriptor* descriptor = Find(type);
		if (!descriptor)
			return node;
		node.id = nodeId;
		node.type = descriptor->type;
		node.displayName = descriptor->displayName;
		node.properties = descriptor->defaultProperties;
		for (const NodePinDefinition& definition : descriptor->inputs)
		{
			node.inputs.push_back(
				{nextPinId++, definition.name, definition.type,
				 definition.defaultValue});
		}
		for (const NodePinDefinition& definition : descriptor->outputs)
		{
			node.outputs.push_back(
				{nextPinId++, definition.name, definition.type,
				 definition.defaultValue});
		}
		return node;
	}

	bool CompiledSoundGraph::IsValid() const
	{
		return std::none_of(
			diagnostics.begin(),
			diagnostics.end(),
			[](const GraphDiagnostic& diagnostic)
			{
				return diagnostic.severity ==
					GraphDiagnosticSeverity::Error;
			});
	}

	CompiledSoundGraph SoundGraphCompiler::Compile(
		const SoundGraphAsset& asset,
		const NodeRegistry& registry)
	{
		CompiledSoundGraph result;
		result.asset = asset;
		std::unordered_map<std::uint64_t, const GraphNode*> nodes;
		std::unordered_map<std::uint64_t, std::size_t> indegree;
		std::unordered_map<std::uint64_t, std::vector<std::uint64_t>> edges;
		std::unordered_set<std::uint64_t> pinIds;
		std::size_t outputCount = 0;

		for (const GraphNode& node : asset.nodes)
		{
			if (node.id == 0 || !nodes.emplace(node.id, &node).second)
			{
				AddDiagnostic(
					result.diagnostics,
					GraphDiagnosticSeverity::Error,
					"Every node must have a unique non-zero ID.",
					node.id);
				continue;
			}
			indegree[node.id] = 0;
			if (!registry.Find(node.type))
			{
				AddDiagnostic(
					result.diagnostics,
					GraphDiagnosticSeverity::Error,
					"Unknown node type: " + node.type,
					node.id);
			}
			if (node.type == "Audio.Output")
				++outputCount;
			for (const GraphPin& pin : node.inputs)
			{
				if (pin.id == 0 || !pinIds.emplace(pin.id).second)
					AddDiagnostic(
						result.diagnostics,
						GraphDiagnosticSeverity::Error,
						"Every pin must have a unique non-zero ID.",
						node.id,
						pin.id);
			}
			for (const GraphPin& pin : node.outputs)
			{
				if (pin.id == 0 || !pinIds.emplace(pin.id).second)
					AddDiagnostic(
						result.diagnostics,
						GraphDiagnosticSeverity::Error,
						"Every pin must have a unique non-zero ID.",
						node.id,
						pin.id);
			}
		}

		if (outputCount != 1)
		{
			AddDiagnostic(
				result.diagnostics,
				GraphDiagnosticSeverity::Error,
				"A sound graph requires exactly one Audio Output node.");
		}

		std::unordered_set<std::uint64_t> linkIds;
		for (const GraphLink& link : asset.links)
		{
			if (link.id == 0 || !linkIds.emplace(link.id).second)
			{
				AddDiagnostic(
					result.diagnostics,
					GraphDiagnosticSeverity::Error,
					"Every link must have a unique non-zero ID.");
				continue;
			}
			bool sourceIsOutput = false;
			bool targetIsOutput = false;
			const GraphNode* sourceNode = nullptr;
			const GraphNode* targetNode = nullptr;
			const GraphPin* source = FindPin(
				asset, link.outputPin, sourceIsOutput, &sourceNode);
			const GraphPin* target = FindPin(
				asset, link.inputPin, targetIsOutput, &targetNode);
			if (!source || !target || !sourceIsOutput || targetIsOutput)
			{
				AddDiagnostic(
					result.diagnostics,
					GraphDiagnosticSeverity::Error,
					"Link endpoints must connect an output pin to an input pin.");
				continue;
			}
			if (!Compatible(source->type, target->type))
			{
				AddDiagnostic(
					result.diagnostics,
					GraphDiagnosticSeverity::Error,
					"Incompatible pin types.",
					targetNode->id,
					target->id);
				continue;
			}
			if (!result.inputSources.emplace(
					target->id, source->id).second)
			{
				AddDiagnostic(
					result.diagnostics,
					GraphDiagnosticSeverity::Error,
					"An input pin can only have one source.",
					targetNode->id,
					target->id);
				continue;
			}
			edges[sourceNode->id].push_back(targetNode->id);
			++indegree[targetNode->id];
		}

		std::queue<std::uint64_t> ready;
		for (const auto& [id, degree] : indegree)
		{
			if (degree == 0)
				ready.push(id);
		}
		while (!ready.empty())
		{
			const std::uint64_t id = ready.front();
			ready.pop();
			result.evaluationOrder.push_back(id);
			for (const std::uint64_t target : edges[id])
			{
				if (--indegree[target] == 0)
					ready.push(target);
			}
		}
		if (result.evaluationOrder.size() != asset.nodes.size())
		{
			AddDiagnostic(
				result.diagnostics,
				GraphDiagnosticSeverity::Error,
				"The sound graph contains a cycle.");
		}
		return result;
	}

	void RuntimeParameterSet::Reset(
		const std::vector<NamedParameter>& parameters)
	{
		definitions_ = parameters;
		values_.clear();
		names_.clear();
		for (NamedParameter& parameter : definitions_)
		{
			if (parameter.id == 0)
				parameter.id = HashAudioParameter(parameter.name);
			names_[parameter.name] = parameter.id;
			values_[parameter.id] = parameter.defaultValue;
		}
	}

	bool RuntimeParameterSet::Set(
		std::string_view name,
		const ParameterValue& value)
	{
		const auto found = names_.find(std::string(name));
		return found != names_.end() && Set(found->second, value);
	}

	bool RuntimeParameterSet::Set(
		AudioParameterId id,
		const ParameterValue& value)
	{
		const auto found = values_.find(id);
		if (found == values_.end() ||
			found->second.index() != value.index())
		{
			return false;
		}
		found->second = value;
		return true;
	}

	const ParameterValue* RuntimeParameterSet::Find(
		std::string_view name) const
	{
		const auto found = names_.find(std::string(name));
		return found == names_.end() ? nullptr : Find(found->second);
	}

	const ParameterValue* RuntimeParameterSet::Find(
		AudioParameterId id) const
	{
		const auto found = values_.find(id);
		return found == values_.end() ? nullptr : &found->second;
	}

	const std::vector<NamedParameter>&
	RuntimeParameterSet::Definitions() const
	{
		return definitions_;
	}

	bool SoundGraphInstance::Load(
		const SoundGraphAsset& asset,
		const NodeRegistry& registry)
	{
		prototype_ = SoundGraphCompiler::Compile(asset, registry);
		parameters_.Reset(asset.parameters);
		randomOneShotHistory_.clear();
		randomArrayHistory_.clear();
		triggerCounters_.clear();
		return prototype_.IsValid();
	}

	bool SoundGraphInstance::IsValid() const
	{
		return prototype_.IsValid() && !prototype_.asset.nodes.empty();
	}

	bool SoundGraphInstance::SetParameter(
		std::string_view name,
		const ParameterValue& value)
	{
		return parameters_.Set(name, value);
	}

	bool SoundGraphInstance::SetParameter(
		AudioParameterId id,
		const ParameterValue& value)
	{
		return parameters_.Set(id, value);
	}

	const ParameterValue* SoundGraphInstance::GetParameter(
		std::string_view name) const
	{
		return parameters_.Find(name);
	}

	RuntimeParameterSet& SoundGraphInstance::Parameters()
	{
		return parameters_;
	}

	const RuntimeParameterSet& SoundGraphInstance::Parameters() const
	{
		return parameters_;
	}

	const CompiledSoundGraph& SoundGraphInstance::Prototype() const
	{
		return prototype_;
	}

	SoundGraphEvaluation SoundGraphInstance::Evaluate()
	{
		SoundGraphEvaluation result;
		result.diagnostics = prototype_.diagnostics;
		if (!IsValid())
			return result;

		std::unordered_map<std::uint64_t, const GraphNode*> nodes;
		std::unordered_map<std::uint64_t, const GraphNode*> outputOwners;
		for (const GraphNode& node : prototype_.asset.nodes)
		{
			nodes[node.id] = &node;
			for (const GraphPin& pin : node.outputs)
				outputOwners[pin.id] = &node;
		}
		std::unordered_map<std::uint64_t, ParameterValue> values;

		const auto input = [&](
			const GraphPin& pin) -> ParameterValue
		{
			const auto linked = prototype_.inputSources.find(pin.id);
			if (linked == prototype_.inputSources.end())
				return pin.defaultValue;
			const auto found = values.find(linked->second);
			return found == values.end() ? pin.defaultValue : found->second;
		};
		const auto write = [&values](
			const GraphNode& node,
			std::size_t index,
			ParameterValue value)
		{
			if (index < node.outputs.size())
				values[node.outputs[index].id] = std::move(value);
		};
		const auto randomArrayIndex = [this](
			std::uint64_t nodeId,
			std::size_t count)
		{
			if (count <= 1)
			{
				randomArrayHistory_[nodeId] = 0;
				return std::size_t(0);
			}
			std::uniform_int_distribution<std::size_t>
				distribution(0, count - 2);
			std::size_t selected = distribution(random_);
			const auto previous = randomArrayHistory_.find(nodeId);
			if (previous != randomArrayHistory_.end() &&
				selected >= previous->second &&
				previous->second < count)
			{
				++selected;
			}
			randomArrayHistory_[nodeId] = selected;
			return selected;
		};

		for (const std::uint64_t id : prototype_.evaluationOrder)
		{
			const GraphNode& node = *nodes[id];
			std::vector<ParameterValue> in;
			in.reserve(node.inputs.size());
			for (const GraphPin& pin : node.inputs)
				in.push_back(input(pin));
			const auto f = [&in](std::size_t index, float fallback = 0.0f)
			{
				return index < in.size()
					? AsFloat(in[index], fallback)
					: fallback;
			};
			const auto b = [&in](std::size_t index, bool fallback = false)
			{
				return index < in.size()
					? AsBool(in[index], fallback)
					: fallback;
			};
			const auto v = [&in](
				std::size_t index,
				const float3& fallback = float3::zero)
			{
				return index < in.size()
					? AsVector3(in[index], fallback)
					: fallback;
			};

			if (node.type.rfind("Constant.", 0) == 0)
			{
				if (!in.empty())
					write(node, 0, in[0]);
			}
			else if (node.type.rfind("Parameter.", 0) == 0)
			{
				const ParameterValue* name = Property(node, "Name");
				const ParameterValue* value = name
					? parameters_.Find(AsString(*name))
					: nullptr;
				write(node, 0, value
					? *value
					: (in.empty() ? ParameterValue(0.0f) : in[0]));
			}
			else if (node.type == "Array.GetFloat")
			{
				const FloatArray array = in.empty()
					? FloatArray{}
					: AsFloatArray(in[0]);
				const int index = in.size() > 1
					? AsInteger(in[1])
					: 0;
				write(
					node,
					0,
					index >= 0 &&
						static_cast<std::size_t>(index) < array.size()
						? array[static_cast<std::size_t>(index)]
						: 0.0f);
			}
			else if (node.type == "Array.GetInteger")
			{
				const IntegerArray array = in.empty()
					? IntegerArray{}
					: AsIntegerArray(in[0]);
				const int index = in.size() > 1
					? AsInteger(in[1])
					: 0;
				write(
					node,
					0,
					index >= 0 &&
						static_cast<std::size_t>(index) < array.size()
						? array[static_cast<std::size_t>(index)]
						: 0);
			}
			else if (node.type == "Array.GetAudioClip")
			{
				const AudioClipArray array = in.empty()
					? AudioClipArray{}
					: AsAudioClipArray(in[0]);
				const int index = in.size() > 1
					? AsInteger(in[1])
					: 0;
				write(
					node,
					0,
					index >= 0 &&
						static_cast<std::size_t>(index) < array.size()
						? array[static_cast<std::size_t>(index)]
						: AudioClipReference{});
			}
			else if (node.type == "Array.RandomFloat")
			{
				const FloatArray array = in.empty()
					? FloatArray{}
					: AsFloatArray(in[0]);
				write(
					node,
					0,
					array.empty()
						? 0.0f
						: array[randomArrayIndex(node.id, array.size())]);
			}
			else if (node.type == "Array.RandomInteger")
			{
				const IntegerArray array = in.empty()
					? IntegerArray{}
					: AsIntegerArray(in[0]);
				write(
					node,
					0,
					array.empty()
						? 0
						: array[randomArrayIndex(node.id, array.size())]);
			}
			else if (node.type == "Array.RandomAudioClip")
			{
				const AudioClipArray array = in.empty()
					? AudioClipArray{}
					: AsAudioClipArray(in[0]);
				write(
					node,
					0,
					array.empty()
						? AudioClipReference{}
						: array[randomArrayIndex(node.id, array.size())]);
			}
			else if (node.type == "Math.Add")
				write(node, 0, f(0) + f(1));
			else if (node.type == "Math.Subtract")
				write(node, 0, f(0) - f(1));
			else if (node.type == "Math.Multiply")
				write(node, 0, f(0) * f(1));
			else if (node.type == "Math.Divide")
			{
				const float denominator = f(1, 1.0f);
				if (std::abs(denominator) <=
					std::numeric_limits<float>::epsilon())
				{
					AddDiagnostic(
						result.diagnostics,
						GraphDiagnosticSeverity::Warning,
						"Division by zero produced 0.",
						node.id);
					write(node, 0, 0.0f);
				}
				else
					write(node, 0, f(0) / denominator);
			}
			else if (node.type == "Math.Logarithm")
			{
				const float value = f(0, 1.0f);
				const float base = f(1, 10.0f);
				if (value <= 0.0f || base <= 0.0f ||
					std::abs(base - 1.0f) <= 0.000001f)
				{
					AddDiagnostic(
						result.diagnostics,
						GraphDiagnosticSeverity::Warning,
						"Logarithm requires a positive value and "
						"a positive base other than one.",
						node.id);
					write(node, 0, 0.0f);
				}
				else
					write(node, 0, std::log(value) / std::log(base));
			}
			else if (node.type == "Math.Modulo")
			{
				const int divisor = in.size() > 1
					? AsInteger(in[1], 1)
					: 1;
				write(
					node,
					0,
					divisor == 0 ? 0 : AsInteger(in[0]) % divisor);
			}
			else if (node.type == "Math.Min")
				write(node, 0, std::min(f(0), f(1)));
			else if (node.type == "Math.Max")
				write(node, 0, std::max(f(0), f(1)));
			else if (node.type == "Math.Clamp")
			{
				const float minimum = std::min(f(1), f(2, 1.0f));
				const float maximum = std::max(f(1), f(2, 1.0f));
				write(node, 0, std::clamp(f(0), minimum, maximum));
			}
			else if (node.type == "Math.Abs")
				write(node, 0, std::abs(f(0)));
			else if (node.type == "Math.Negate")
				write(node, 0, -f(0));
			else if (node.type == "Math.Lerp")
				write(node, 0, f(0) + (f(1) - f(0)) * f(2));
			else if (node.type == "Math.Remap")
			{
				const float range = f(2, 1.0f) - f(1);
				const float alpha = std::abs(range) > 0.000001f
					? (f(0) - f(1)) / range
					: 0.0f;
				write(node, 0, f(3) + (f(4, 1.0f) - f(3)) * alpha);
			}
			else if (node.type == "Math.Random")
			{
				std::uniform_real_distribution<float> distribution(
					std::min(f(0), f(1, 1.0f)),
					std::max(f(0), f(1, 1.0f)));
				write(node, 0, distribution(random_));
			}
			else if (node.type == "Math.Sine")
				write(node, 0, std::sin(f(0)));
			else if (node.type == "Math.Cosine")
				write(node, 0, std::cos(f(0)));
			else if (node.type == "Math.Power")
				write(node, 0, std::pow(f(0), f(1, 2.0f)));
			else if (node.type == "Math.SquareRoot")
				write(node, 0, std::sqrt(std::max(0.0f, f(0))));
			else if (node.type == "Math.Floor")
				write(node, 0, std::floor(f(0)));
			else if (node.type == "Math.Ceil")
				write(node, 0, std::ceil(f(0)));
			else if (node.type == "Math.Round")
				write(node, 0, std::round(f(0)));
			else if (node.type == "Audio.LinearToLogFrequency")
			{
				const float minimum = f(1);
				const float maximum = f(2, 1.0f);
				const float minimumFrequency =
					std::max(0.001f, f(3, 20.0f));
				const float maximumFrequency =
					std::max(minimumFrequency, f(4, 20000.0f));
				const float range = maximum - minimum;
				const float alpha = std::abs(range) > 0.000001f
					? std::clamp((f(0, 0.5f) - minimum) / range,
						0.0f, 1.0f)
					: 0.0f;
				write(
					node,
					0,
					minimumFrequency * std::pow(
						maximumFrequency / minimumFrequency,
						alpha));
			}
			else if (node.type == "Audio.FrequencyToLinear")
			{
				const float minimumFrequency =
					std::max(0.001f, f(1, 20.0f));
				const float maximumFrequency =
					std::max(minimumFrequency, f(2, 20000.0f));
				const float frequency = std::clamp(
					f(0, 1000.0f),
					minimumFrequency,
					maximumFrequency);
				const float denominator = std::log(
					maximumFrequency / minimumFrequency);
				const float alpha = denominator > 0.000001f
					? std::log(frequency / minimumFrequency) /
						denominator
					: 0.0f;
				write(node, 0, f(3) + (f(4, 1.0f) - f(3)) * alpha);
			}
			else if (node.type == "Music.BPMToSeconds")
				write(node, 0, 60.0f / std::max(f(0, 90.0f), 0.001f));
			else if (node.type == "Music.NoteToFrequency")
				write(
					node,
					0,
					440.0f * std::pow(
						2.0f, (f(0, 60.0f) - 69.0f) / 12.0f));
			else if (node.type == "Music.FrequencyToNote")
				write(
					node,
					0,
					69.0f + 12.0f * std::log2(
						std::max(f(0, 440.0f), 0.001f) / 440.0f));
			else if (node.type == "Trigger.Counter")
			{
				const int start =
					in.size() > 2 ? AsInteger(in[2]) : 0;
				const auto counter =
					triggerCounters_.try_emplace(node.id, start).first;
				int& count = counter->second;
				if (b(1))
					count = in.size() > 4 ? AsInteger(in[4]) : 0;
				if (b(0, true))
				count += in.size() > 3 ? AsInteger(in[3], 1) : 1;
				write(node, 0, count);
			}
			else if (node.type == "Logic.Greater")
				write(node, 0, f(0) > f(1));
			else if (node.type == "Logic.Less")
				write(node, 0, f(0) < f(1));
			else if (node.type == "Logic.Equal")
				write(node, 0, std::abs(f(0) - f(1)) <= f(2, 0.0001f));
			else if (node.type == "Logic.And")
				write(node, 0, b(0) && b(1));
			else if (node.type == "Logic.Or")
				write(node, 0, b(0) || b(1));
			else if (node.type == "Logic.Not")
				write(node, 0, !b(0));
			else if (node.type == "Logic.SelectFloat")
				write(node, 0, b(0) ? f(2, 1.0f) : f(1));
			else if (node.type == "Logic.SelectBool")
				write(node, 0, b(0) ? b(2, true) : b(1));
			else if (node.type == "Logic.SelectVector3")
				write(node, 0, b(0) ? v(2) : v(1));
			else if (node.type == "Vector.Compose")
				write(node, 0, float3(f(0), f(1), f(2)));
			else if (node.type == "Vector.X")
				write(node, 0, v(0).x);
			else if (node.type == "Vector.Y")
				write(node, 0, v(0).y);
			else if (node.type == "Vector.Z")
				write(node, 0, v(0).z);
			else if (node.type == "Vector.Length")
				write(node, 0, v(0).Length());
			else if (node.type == "Vector.Distance")
				write(node, 0, v(0).Distance(v(1)));
			else if (node.type == "Vector.Add")
				write(node, 0, v(0) + v(1));
			else if (node.type == "Vector.Subtract")
				write(node, 0, v(0) - v(1));
			else if (node.type == "Vector.Scale")
				write(node, 0, v(0) * f(1, 1.0f));
			else if (node.type == "Vector.Normalize")
			{
				const float3 value = v(0);
				write(node, 0, value.LengthSq() > 0.000001f
					? value.Normalized()
					: float3::zero);
			}
			else if (node.type == "Vector.Dot")
				write(node, 0, v(0).Dot(v(1)));
			else if (node.type == "Vector.Cross")
				write(node, 0, v(0).Cross(v(1)));
			else if (node.type == "Audio.Gain")
				write(node, 0, std::max(0.0f, f(0, 1.0f) * f(1, 1.0f)));
			else if (node.type == "Audio.Pitch")
				write(node, 0, std::pow(2.0f, f(0) / 12.0f));
			else if (node.type == "Audio.Pan")
				write(node, 0, std::clamp(f(0), -1.0f, 1.0f));
			else if (node.type == "Audio.Attenuation")
			{
				const float minimum = std::max(0.001f, f(1, 1.0f));
				const float maximum = std::max(minimum, f(2, 100.0f));
				const float distance = std::clamp(f(0), minimum, maximum);
				write(node, 0, std::pow(
					minimum / distance,
					std::max(0.0f, f(3, 1.0f))));
			}
			else if (node.type == "Audio.RandomOneShot")
			{
				std::vector<AudioClipReference> clips;
				clips.reserve(in.size());
				for (const ParameterValue& value : in)
				{
					const AudioClipReference clip = AsAudioClip(value);
					if (!clip.IsValid() ||
						std::find(
							clips.begin(),
							clips.end(),
							clip) != clips.end())
					{
						continue;
					}
					clips.push_back(clip);
				}

				if (clips.empty())
				{
					AddDiagnostic(
						result.diagnostics,
						GraphDiagnosticSeverity::Warning,
						"Random One Shot has no valid clips.",
						node.id);
					write(node, 0, AudioClipReference{});
				}
				else
				{
					std::size_t selected = 0;
					if (clips.size() > 1)
					{
						const auto history =
							randomOneShotHistory_.find(node.id);
						const AudioClipReference previous =
							history !=
								randomOneShotHistory_.end()
								? history->second
								: AudioClipReference{};
						std::uniform_int_distribution<std::size_t>
							distribution(0, clips.size() - 1);
						selected = distribution(random_);
						if (clips[selected] == previous)
						{
							std::uniform_int_distribution<std::size_t>
								alternate(0, clips.size() - 2);
							selected = alternate(random_);
							if (clips[selected] == previous)
								selected = clips.size() - 1;
						}
					}
					randomOneShotHistory_[node.id] =
						clips[selected];
					write(node, 0, clips[selected]);
				}
			}
			else if (node.type == "Audio.Output")
			{
				const AudioClipReference clip = in.empty()
					? AudioClipReference{}
					: AsAudioClip(in[0]);
				result.voice.filePath = clip.resolvedSource;
				result.voice.settings.volume =
					std::max(0.0f, f(1, 1.0f));
				result.voice.settings.pitch =
					std::max(0.01f, f(2, 1.0f));
				result.voice.settings.pan =
					std::clamp(f(3), -1.0f, 1.0f);
				bool forceOneShot = false;
				if (!node.inputs.empty())
				{
					const auto source =
						prototype_.inputSources.find(
							node.inputs.front().id);
					if (source !=
						prototype_.inputSources.end())
					{
						const auto owner =
							outputOwners.find(source->second);
						forceOneShot =
							owner != outputOwners.end() &&
							owner->second->type ==
								"Audio.RandomOneShot";
					}
				}
				result.voice.settings.looping =
					forceOneShot ? false : b(4);
				result.voice.settings.spatial.enabled = b(5, true);
				result.voice.settings.spatial.minDistance =
					std::max(0.001f, f(6, 1.0f));
				result.voice.settings.spatial.maxDistance = std::max(
					result.voice.settings.spatial.minDistance,
					f(7, 100.0f));
				result.voice.settings.spatial.rolloff =
					std::max(0.0f, f(8, 1.0f));
				result.voice.settings.spatial.dopplerFactor =
					std::max(0.0f, f(9, 1.0f));
				result.voice.transform.position = v(10);
				result.voice.transform.velocity = v(11);
				result.succeeded = !result.voice.filePath.empty();
				if (!result.succeeded)
				{
					AddDiagnostic(
						result.diagnostics,
						GraphDiagnosticSeverity::Error,
						"Audio Output requires a resolved Audio Clip asset.",
						node.id);
				}
			}
		}
		return result;
	}

	AudioParameterId HashAudioParameter(std::string_view name)
	{
		std::uint32_t hash = 2166136261u;
		for (const unsigned char character : name)
		{
			hash ^= character;
			hash *= 16777619u;
		}
		return hash == 0 ? 1 : hash;
	}

	SoundGraphAsset CreateDefaultSoundGraph(
		const NodeRegistry& registry,
		const AudioClipReference& clipReference)
	{
		SoundGraphAsset asset;
		asset.id = 1;
		asset.name = "Interactive ReziAudio Graph";
		asset.parameters = {
			{"Volume", HashAudioParameter("Volume"), 1.0f},
			{"Pitch", HashAudioParameter("Pitch"), 1.0f},
			{"Loop", HashAudioParameter("Loop"), true},
			{"Position", HashAudioParameter("Position"),
			 float3(3.0f, 0.0f, 0.0f)}
		};
		std::uint64_t nextNode = 1;
		std::uint64_t nextPin = 100;
		const auto add = [&](std::string_view type, float2 position)
		{
			GraphNode node =
				registry.CreateNode(type, nextNode++, nextPin);
			node.editorPosition = position;
			asset.nodes.push_back(std::move(node));
			return asset.nodes.size() - 1;
		};
		const std::size_t clip = add("Constant.Clip", float2(30, 100));
		asset.nodes[clip].inputs[0].defaultValue = clipReference;
		const std::size_t volume =
			add("Parameter.Float", float2(30, 260));
		asset.nodes[volume].properties["Name"] = std::string("Volume");
		const std::size_t pitch =
			add("Parameter.Float", float2(30, 380));
		asset.nodes[pitch].properties["Name"] = std::string("Pitch");
		const std::size_t loop =
			add("Parameter.Bool", float2(30, 500));
		asset.nodes[loop].properties["Name"] = std::string("Loop");
		const std::size_t position =
			add("Parameter.Vector3", float2(30, 620));
		asset.nodes[position].properties["Name"] =
			std::string("Position");
		const std::size_t output =
			add("Audio.Output", float2(430, 250));

		std::uint64_t nextLink = 1000;
		const auto connect = [&](std::size_t from, std::size_t out,
			std::size_t to, std::size_t in)
		{
			asset.links.push_back({
				nextLink++,
				asset.nodes[from].outputs[out].id,
				asset.nodes[to].inputs[in].id});
		};
		connect(clip, 0, output, 0);
		connect(volume, 0, output, 1);
		connect(pitch, 0, output, 2);
		connect(loop, 0, output, 4);
		connect(position, 0, output, 10);
		return asset;
	}

	SoundGraphAsset CreateDefaultSoundGraph(
		const NodeRegistry& registry,
		const std::string& clipPath)
	{
		return CreateDefaultSoundGraph(
			registry,
			AudioClipReference{0, clipPath});
	}

	bool IsGraphPinConnected(
		const SoundGraphAsset& graph,
		std::uint64_t pinId)
	{
		return std::any_of(
			graph.links.begin(),
			graph.links.end(),
			[pinId](const GraphLink& link)
			{
				return link.outputPin == pinId || link.inputPin == pinId;
			});
	}

	std::size_t DisconnectGraphPin(
		SoundGraphAsset& graph,
		std::uint64_t pinId)
	{
		const std::size_t previousSize = graph.links.size();
		std::erase_if(
			graph.links,
			[pinId](const GraphLink& link)
			{
				return link.outputPin == pinId || link.inputPin == pinId;
			});
		return previousSize - graph.links.size();
	}

	namespace
	{
		struct InputPinLocation
		{
			const GraphNode* node = nullptr;
			const GraphPin* pin = nullptr;
		};

		InputPinLocation FindInputPin(
			const SoundGraphAsset& graph,
			std::uint64_t pinId)
		{
			for (const GraphNode& node : graph.nodes)
			{
				const auto found = std::find_if(
					node.inputs.begin(),
					node.inputs.end(),
					[pinId](const GraphPin& pin)
					{
						return pin.id == pinId;
					});
				if (found != node.inputs.end())
					return {&node, &*found};
			}
			return {};
		}

		const char* ParameterNodeType(GraphPinType type)
		{
			switch (type)
			{
			case GraphPinType::Bool:
				return "Parameter.Bool";
			case GraphPinType::Integer:
				return "Parameter.Integer";
			case GraphPinType::Float:
				return "Parameter.Float";
			case GraphPinType::Vector2:
				return "Parameter.Vector2";
			case GraphPinType::Vector3:
				return "Parameter.Vector3";
			case GraphPinType::Color:
				return "Parameter.Color";
			case GraphPinType::String:
				return "Parameter.String";
			case GraphPinType::AudioClip:
				return "Parameter.AudioClip";
			case GraphPinType::FloatArray:
				return "Parameter.FloatArray";
			case GraphPinType::IntegerArray:
				return "Parameter.IntegerArray";
			case GraphPinType::AudioClipArray:
				return "Parameter.AudioClipArray";
			default:
				return nullptr;
			}
		}

		std::optional<GraphPinType> ParameterType(
			const ParameterValue& value)
		{
			if (std::holds_alternative<bool>(value))
				return GraphPinType::Bool;
			if (std::holds_alternative<int>(value))
				return GraphPinType::Integer;
			if (std::holds_alternative<float>(value))
				return GraphPinType::Float;
			if (std::holds_alternative<float2>(value))
				return GraphPinType::Vector2;
			if (std::holds_alternative<float3>(value))
				return GraphPinType::Vector3;
			if (std::holds_alternative<float4>(value))
				return GraphPinType::Color;
			if (std::holds_alternative<std::string>(value))
				return GraphPinType::String;
			if (std::holds_alternative<AudioClipReference>(value))
				return GraphPinType::AudioClip;
			if (std::holds_alternative<FloatArray>(value))
				return GraphPinType::FloatArray;
			if (std::holds_alternative<IntegerArray>(value))
				return GraphPinType::IntegerArray;
			if (std::holds_alternative<AudioClipArray>(value))
				return GraphPinType::AudioClipArray;
			return std::nullopt;
		}

		bool CompatibleParameterType(
			GraphPinType parameter,
			GraphPinType input)
		{
			if (parameter == input)
				return true;
			const bool parameterNumeric =
				parameter == GraphPinType::Integer ||
				parameter == GraphPinType::Float;
			const bool inputNumeric =
				input == GraphPinType::Integer ||
				input == GraphPinType::Float;
			return parameterNumeric && inputNumeric;
		}

		std::uint64_t CalculateNextNodeId(
			const SoundGraphAsset& graph)
		{
			std::uint64_t result = 1;
			for (const GraphNode& node : graph.nodes)
				result = std::max(result, node.id + 1);
			return result;
		}

		std::uint64_t CalculateNextPinId(
			const SoundGraphAsset& graph)
		{
			std::uint64_t result = 1;
			for (const GraphNode& node : graph.nodes)
			{
				for (const GraphPin& pin : node.inputs)
					result = std::max(result, pin.id + 1);
				for (const GraphPin& pin : node.outputs)
					result = std::max(result, pin.id + 1);
			}
			return result;
		}

		std::uint64_t CalculateNextLinkId(
			const SoundGraphAsset& graph)
		{
			std::uint64_t result = 1;
			for (const GraphLink& link : graph.links)
				result = std::max(result, link.id + 1);
			return result;
		}

		std::string UniqueParameterName(
			const SoundGraphAsset& graph,
			std::string baseName)
		{
			if (baseName.empty())
				baseName = "Parameter";
			std::string candidate = baseName;
			std::size_t suffix = 2;
			const auto isUsed = [&graph](const std::string& name)
			{
				const AudioParameterId id = HashAudioParameter(name);
				return std::any_of(
					graph.parameters.begin(),
					graph.parameters.end(),
					[&name, id](const NamedParameter& parameter)
					{
						return parameter.name == name ||
							parameter.id == id;
					});
			};
			while (isUsed(candidate))
				candidate =
					baseName + " " + std::to_string(suffix++);
			return candidate;
		}

		std::optional<GraphParameterNodeResult>
			AddParameterGetterAndLink(
				SoundGraphAsset& graph,
				const NodeRegistry& registry,
				std::uint64_t inputPinId,
				const NamedParameter& parameter,
				GraphPinType inputType,
				const float2& nodePosition)
		{
			const std::optional<GraphPinType> parameterType =
				ParameterType(parameter.defaultValue);
			if (!parameterType ||
				!CompatibleParameterType(*parameterType, inputType))
			{
				return std::nullopt;
			}

			const char* nodeType = ParameterNodeType(*parameterType);
			if (!nodeType || !registry.Find(nodeType))
				return std::nullopt;

			const std::uint64_t nodeId =
				CalculateNextNodeId(graph);
			std::uint64_t nextPinId =
				CalculateNextPinId(graph);
			GraphNode node =
				registry.CreateNode(nodeType, nodeId, nextPinId);
			if (node.outputs.empty())
				return std::nullopt;
			node.editorPosition = nodePosition;
			node.properties["Name"] = parameter.name;
			if (!node.inputs.empty())
				node.inputs.front().defaultValue =
					parameter.defaultValue;

			const std::uint64_t outputPinId =
				node.outputs.front().id;
			const std::uint64_t linkId =
				CalculateNextLinkId(graph);
			graph.nodes.push_back(std::move(node));
			graph.links.push_back(
				{linkId, outputPinId, inputPinId});
			return GraphParameterNodeResult{
				nodeId,
				outputPinId,
				linkId,
				parameter.name};
		}
	}

	std::uint64_t NextGraphNodeId(
		const SoundGraphAsset& graph)
	{
		return CalculateNextNodeId(graph);
	}

	std::uint64_t NextGraphPinId(
		const SoundGraphAsset& graph)
	{
		return CalculateNextPinId(graph);
	}

	std::uint64_t NextGraphLinkId(
		const SoundGraphAsset& graph)
	{
		return CalculateNextLinkId(graph);
	}

	bool CanPromoteInputToParameter(
		const SoundGraphAsset& graph,
		std::uint64_t inputPinId)
	{
		const InputPinLocation location =
			FindInputPin(graph, inputPinId);
		return location.pin &&
			ParameterNodeType(location.pin->type) &&
			!IsGraphPinConnected(graph, inputPinId);
	}

	bool CanConnectParameterToInput(
		const SoundGraphAsset& graph,
		std::uint64_t inputPinId,
		std::string_view parameterName)
	{
		const InputPinLocation location =
			FindInputPin(graph, inputPinId);
		if (!location.pin ||
			IsGraphPinConnected(graph, inputPinId))
		{
			return false;
		}
		const auto parameter = std::find_if(
			graph.parameters.begin(),
			graph.parameters.end(),
			[parameterName](const NamedParameter& candidate)
			{
				return candidate.name == parameterName;
			});
		if (parameter == graph.parameters.end())
			return false;
		const std::optional<GraphPinType> type =
			ParameterType(parameter->defaultValue);
		return type &&
			CompatibleParameterType(*type, location.pin->type);
	}

	std::optional<GraphParameterNodeResult>
		PromoteInputToParameter(
			SoundGraphAsset& graph,
			const NodeRegistry& registry,
			std::uint64_t inputPinId,
			const float2& nodePosition)
	{
		const InputPinLocation location =
			FindInputPin(graph, inputPinId);
		if (!location.node ||
			!location.pin ||
			!CanPromoteInputToParameter(graph, inputPinId))
		{
			return std::nullopt;
		}

		std::string baseName = location.pin->name;
		if (baseName.empty() ||
			baseName == "Value" ||
			baseName == "Fallback" ||
			baseName == "Input")
		{
			baseName =
				location.node->displayName + " " +
				location.pin->name;
		}
		NamedParameter parameter{
			UniqueParameterName(graph, std::move(baseName)),
			0,
			location.pin->defaultValue};
		parameter.id = HashAudioParameter(parameter.name);
		graph.parameters.push_back(parameter);

		const auto result = AddParameterGetterAndLink(
			graph,
			registry,
			inputPinId,
			parameter,
			location.pin->type,
			nodePosition);
		if (!result)
			graph.parameters.pop_back();
		return result;
	}

	std::optional<GraphParameterNodeResult>
		ConnectParameterToInput(
			SoundGraphAsset& graph,
			const NodeRegistry& registry,
			std::uint64_t inputPinId,
			std::string_view parameterName,
			const float2& nodePosition)
	{
		const InputPinLocation location =
			FindInputPin(graph, inputPinId);
		if (!location.pin ||
			!CanConnectParameterToInput(
				graph, inputPinId, parameterName))
		{
			return std::nullopt;
		}
		const auto parameter = std::find_if(
			graph.parameters.begin(),
			graph.parameters.end(),
			[parameterName](const NamedParameter& candidate)
			{
				return candidate.name == parameterName;
			});
		return AddParameterGetterAndLink(
			graph,
			registry,
			inputPinId,
			*parameter,
			location.pin->type,
			nodePosition);
	}
}
