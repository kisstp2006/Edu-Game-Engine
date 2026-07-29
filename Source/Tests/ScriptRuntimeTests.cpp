#include "../Scripting/ScriptRuntime.h"
#include "../Scripting/ScriptAsset.h"
#include "../Scripting/ScriptCoreHelpers.h"
#include "../Scripting/ScriptMath.h"
#include "../Scripting/ScriptResource.h"
#include "../Scripting/ScriptTime.h"
#include "../Project/VsCodeWorkspace.h"
#include "../EngineTime.h"
#include "../Config.h"
#include "../Reflection/PropertySerializer.h"

#include <cmath>
#include <chrono>
#include <cstdarg>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

void _log(const char[], int, const char*, ...)
{
}

namespace
{
	class TemporaryProject final
	{
	public:
		TemporaryProject()
		{
			const auto suffix =
				std::chrono::steady_clock::now().time_since_epoch().count();
			root_ = std::filesystem::temp_directory_path() /
				("ege-scripting-test-" + std::to_string(suffix));
			std::filesystem::create_directories(
				root_ / "Assets" / "Scripts");
		}

		~TemporaryProject()
		{
			std::error_code ignored;
			std::filesystem::remove_all(root_, ignored);
		}

		const std::filesystem::path& Root() const
		{
			return root_;
		}

		void WriteScript(const std::string& source) const
		{
			std::ofstream stream(
				root_ / "Assets" / "Scripts" / "Project.as",
				std::ios::binary | std::ios::trunc);
			stream << source;
		}

	private:
		std::filesystem::path root_;
	};

	bool Check(bool condition, const char* message)
	{
		if (!condition)
			std::cerr << message << '\n';
		return condition;
	}

	const EGE::PropertyDescriptor* FindProperty(
		const EGE::ReflectedScriptObject& object,
		const char* name)
	{
		return object.type ? object.type->FindProperty(name) : nullptr;
	}

	const EGE::PropertyState* FindState(
		const EGE::PropertyBag& properties,
		const char* name)
	{
		for (const EGE::PropertyState& property : properties)
		{
			if (property.name == name)
				return &property;
		}
		return nullptr;
	}

	double ReadNumber(
		const EGE::ReflectedScriptObject& object,
		const char* name)
	{
		const EGE::PropertyDescriptor* property =
			FindProperty(object, name);
		EGE::PropertyValue value;
		if (!property || !property->Read(object.object, value))
			return 0.0;
		if (const auto* number = std::get_if<double>(&value))
			return *number;
		if (const auto* number = std::get_if<std::int64_t>(&value))
			return static_cast<double>(*number);
		if (const auto* number = std::get_if<std::uint64_t>(&value))
			return static_cast<double>(*number);
		return 0.0;
	}

	struct NativeSettings
	{
		float exposure = 1.0f;
		std::string profile = "Default";
	};

	EGE::TimeService* testTimeService = nullptr;

	EGE::TimeService* ResolveTestTimeService()
	{
		return testTimeService;
	}
}

int main()
{
	TemporaryProject helpersProject;
	helpersProject.WriteScript(
		"[ScriptComponent]\n"
		"class HelpersProbe : EGEBehaviour\n"
		"{\n"
		"    bool deterministicRandom = false;\n"
		"    bool randomRangeValid = false;\n"
		"    string builtText;\n"
		"    double durationSeconds = 0.0;\n"
		"    bool stopwatchValid = false;\n"
		"    bool guidRoundTrip = false;\n"
		"    string pathName;\n"
		"    string normalizedText;\n"
		"    bool stringChecks = false;\n"
		"    bool arraysValid = false;\n"
		"\n"
		"    void OnUpdate(float deltaTime)\n"
		"    {\n"
		"        Random first(1337);\n"
		"        Random second(1337);\n"
		"        deterministicRandom = first.Next() == second.Next();\n"
		"        int ranged = first.Next(10, 20);\n"
		"        Vector3 randomVector = first.NextVector3(-2, 2);\n"
		"        Color randomColor = first.NextColor();\n"
		"        randomRangeValid = ranged >= 10 && ranged < 20 &&\n"
		"            randomVector.x >= -2 && randomVector.x <= 2 &&\n"
		"            randomColor.a == 1;\n"
		"\n"
		"        StringBuilder builder(\"Value: \");\n"
		"        builder.Append(42).AppendLine().Append(true);\n"
		"        builtText = builder.ToString();\n"
		"\n"
		"        TimeSpan duration = TimeSpan::FromSeconds(90.5);\n"
		"        durationSeconds = duration.TotalSeconds;\n"
		"\n"
		"        Stopwatch stopwatch;\n"
		"        stopwatch.Start();\n"
		"        stopwatch.Stop();\n"
		"        Stopwatch started = Stopwatch::StartNew();\n"
		"        started.Stop();\n"
		"        stopwatchValid = !stopwatch.IsRunning &&\n"
		"            stopwatch.ElapsedMilliseconds >= 0 &&\n"
		"            stopwatch.Elapsed.TotalMilliseconds >= 0 &&\n"
		"            started.ElapsedMilliseconds >= 0;\n"
		"\n"
		"        Guid generated = Guid::NewGuid();\n"
		"        Guid parsed;\n"
		"        guidRoundTrip = !generated.IsEmpty &&\n"
		"            Guid::TryParse(generated.ToString(), parsed) &&\n"
		"            parsed == generated && Guid::Empty.IsEmpty;\n"
		"\n"
		"        pathName = Path::GetFileNameWithoutExtension(\n"
		"            Path::Combine(\"Assets/Scripts\", \"Player.as\"));\n"
		"        normalizedText = \"  Hello Engine  \".Trim().\n"
		"            ToUpper().Replace(\"ENGINE\", \"WORLD\");\n"
		"        stringChecks = normalizedText == \"HELLO WORLD\" &&\n"
		"            normalizedText.StartsWith(\"HELLO\") &&\n"
		"            normalizedText.EndsWith(\"WORLD\") &&\n"
		"            normalizedText.Contains(\"LO WO\") &&\n"
		"            normalizedText.Length == 11 &&\n"
		"            String::IsNullOrWhiteSpace(\"  \\t\");\n"
		"\n"
		"        array<int> values = { 1, 2, 3 };\n"
		"        values.insertLast(4);\n"
		"        arraysValid = values.length() == 4 &&\n"
		"            values[0] == 1 && values[3] == 4;\n"
		"    }\n"
		"}\n");

	EGE::ScriptRuntime helpersRuntime;
	std::string helpersRegistrationError;
	if (!Check(
			helpersRuntime.RegisterApi(
				"Engine.Math",
				EGE::RegisterMathApi,
				helpersRegistrationError),
			"Helper Math API registration was rejected") ||
		!Check(
			helpersRuntime.RegisterApi(
				"Engine.CoreHelpers",
				EGE::RegisterCoreHelpersApi,
				helpersRegistrationError),
			"Core helper API registration was rejected") ||
		!Check(
			helpersRuntime.Initialize(),
			"Core helper runtime initialization failed") ||
		!Check(
			helpersRuntime.SetProjectRoot(helpersProject.Root()),
			"Core helper script did not compile"))
	{
		std::cerr << helpersRegistrationError << '\n';
		for (const EGE::ScriptDiagnostic& diagnostic :
			helpersRuntime.GetDiagnostics())
		{
			std::cerr
				<< diagnostic.file.string() << ':'
				<< diagnostic.line << ':'
				<< diagnostic.column << ' '
				<< diagnostic.message << '\n';
		}
		return 1;
	}

	const EGE::ScriptInstanceHandle helpersInstance =
		helpersRuntime.CreateInstance("HelpersProbe");
	helpersRuntime.EnterPlayMode();
	helpersRuntime.StartInstance(helpersInstance);
	helpersRuntime.UpdateInstance(helpersInstance, 1.0f / 60.0f);
	const EGE::ReflectedScriptObject reflectedHelpers =
		helpersRuntime.GetReflectedInstance(helpersInstance);

	const auto ReadBool = [&reflectedHelpers](const char* name)
	{
		const EGE::PropertyDescriptor* property =
			FindProperty(reflectedHelpers, name);
		EGE::PropertyValue value;
		return property &&
			property->Read(reflectedHelpers.object, value) &&
			std::holds_alternative<bool>(value) &&
			std::get<bool>(value);
	};
	const auto ReadText = [&reflectedHelpers](const char* name)
	{
		const EGE::PropertyDescriptor* property =
			FindProperty(reflectedHelpers, name);
		EGE::PropertyValue value;
		if (!property ||
			!property->Read(reflectedHelpers.object, value) ||
			!std::holds_alternative<std::string>(value))
		{
			return std::string();
		}
		return std::get<std::string>(value);
	};

	const bool deterministicRandom =
		ReadBool("deterministicRandom");
	const bool randomRangeValid = ReadBool("randomRangeValid");
	const std::string builtText = ReadText("builtText");
	const double durationSeconds =
		ReadNumber(reflectedHelpers, "durationSeconds");
	const bool stopwatchValid = ReadBool("stopwatchValid");
	const bool guidRoundTrip = ReadBool("guidRoundTrip");
	const std::string pathName = ReadText("pathName");
	const std::string normalizedText = ReadText("normalizedText");
	const bool stringChecks = ReadBool("stringChecks");
	const bool arraysValid = ReadBool("arraysValid");
	const bool helpersValid =
		reflectedHelpers &&
		deterministicRandom &&
		randomRangeValid &&
		builtText == "Value: 42\nTrue" &&
		std::abs(durationSeconds - 90.5) < 0.0001 &&
		stopwatchValid &&
		guidRoundTrip &&
		pathName == "Player" &&
		normalizedText == "HELLO WORLD" &&
		stringChecks &&
		arraysValid;
	if (!helpersValid)
	{
		std::cerr
			<< "Helper results: deterministic=" << deterministicRandom
			<< " range=" << randomRangeValid
			<< " built=[" << builtText << ']'
			<< " duration=" << durationSeconds
			<< " stopwatch=" << stopwatchValid
			<< " guid=" << guidRoundTrip
			<< " path=[" << pathName << ']'
			<< " text=[" << normalizedText << ']'
			<< " string=" << stringChecks
			<< " arrays=" << arraysValid << '\n';
		for (const EGE::ScriptDiagnostic& diagnostic :
			helpersRuntime.GetDiagnostics())
		{
			std::cerr
				<< diagnostic.file.string() << ':'
				<< diagnostic.line << ':'
				<< diagnostic.column << ' '
				<< diagnostic.message << '\n';
		}
	}
	if (!Check(
			helpersValid,
			"One or more C#-style helpers returned an unexpected value"))
	{
		return 1;
	}

	std::ifstream helpersDefinitions(
		helpersProject.Root() / "as.predefined");
	const std::string helpersLanguageServerApi(
		(std::istreambuf_iterator<char>(helpersDefinitions)),
		std::istreambuf_iterator<char>());
	if (!Check(
			helpersLanguageServerApi.find("class Random") !=
				std::string::npos &&
			helpersLanguageServerApi.find("class StringBuilder") !=
				std::string::npos &&
			helpersLanguageServerApi.find("class TimeSpan") !=
				std::string::npos &&
			helpersLanguageServerApi.find("class Stopwatch") !=
				std::string::npos &&
			helpersLanguageServerApi.find("class Guid") !=
				std::string::npos &&
			helpersLanguageServerApi.find("class array<class T>") ==
				std::string::npos &&
			helpersLanguageServerApi.find(
				"namespace Stopwatch {\n"
				"    Stopwatch StartNew()") != std::string::npos &&
			helpersLanguageServerApi.find(
				"namespace Path {\n"
				"    string Combine(") != std::string::npos &&
			helpersLanguageServerApi.find(
				"bool Contains(const string&in value) const;") !=
				std::string::npos,
			"Language-server definitions do not include the core helpers"))
	{
		return 1;
	}
	helpersRuntime.LeavePlayMode();
	helpersRuntime.DestroyInstance(helpersInstance);
	helpersRuntime.Shutdown();

	TemporaryProject reflectionPolishProject;
	reflectionPolishProject.WriteScript(
		"enum MovementMode\n"
		"{\n"
		"    Idle = 0,\n"
		"    Run = 2\n"
		"}\n"
		"\n"
		"[ScriptComponent]\n"
		"class ReflectionPolishProbe : EGEBehaviour\n"
		"{\n"
		"    [Tooltip(\"Current movement mode\")]\n"
		"    MovementMode mode = Run;\n"
		"\n"
		"    [ReadOnly]\n"
		"    int buildNumber = 7;\n"
		"}\n");

	EGE::ScriptRuntime reflectionPolishRuntime;
	if (!Check(
			reflectionPolishRuntime.Initialize(),
			"Reflection polish runtime initialization failed") ||
		!Check(
			reflectionPolishRuntime.SetProjectRoot(
				reflectionPolishProject.Root()),
			"Reflection polish script did not compile"))
	{
		return 1;
	}

	const std::vector<EGE::ScriptClassInfo> reflectionPolishClasses =
		reflectionPolishRuntime.GetAvailableClasses();
	if (!Check(
			reflectionPolishClasses.size() == 1,
			"Reflection polish class discovery failed"))
	{
		return 1;
	}
	const std::string reflectionPolishAssetId =
		reflectionPolishClasses.front().assetId;
	const EGE::ScriptInstanceHandle reflectionPolishInstance =
		reflectionPolishRuntime.CreateInstance("ReflectionPolishProbe");
	const EGE::ReflectedScriptObject reflectionPolishObject =
		reflectionPolishRuntime.GetReflectedInstance(
			reflectionPolishInstance);
	const EGE::PropertyDescriptor* modeProperty =
		FindProperty(reflectionPolishObject, "mode");
	const EGE::PropertyDescriptor* buildNumberProperty =
		FindProperty(reflectionPolishObject, "buildNumber");
	if (!Check(
			modeProperty &&
				modeProperty->kind == EGE::PropertyKind::Enumeration &&
				modeProperty->attributes.tooltip ==
					"Current movement mode" &&
				modeProperty->enumValues.size() == 2 &&
				modeProperty->enumValues[1].name == "Run" &&
				modeProperty->enumValues[1].value == 2,
			"Enum choices or Tooltip metadata were not reflected") ||
		!Check(
			buildNumberProperty &&
				buildNumberProperty->attributes.readOnly,
			"ReadOnly metadata was not reflected"))
	{
		return 1;
	}

	reflectionPolishProject.WriteScript(
		"// EGE-ScriptId: " + reflectionPolishAssetId + "\n"
		"enum MovementMode { Idle = 0, Run = 2 }\n"
		"[ScriptComponent]\n"
		"class RenamedReflectionProbe : EGEBehaviour\n"
		"{\n"
		"    MovementMode mode = Idle;\n"
		"    int buildNumber = 0;\n"
		"}\n");
	if (!Check(
			reflectionPolishRuntime.ForceReload(),
			"Renamed script class did not hot reload") ||
		!Check(
			reflectionPolishRuntime.ResolveClass(
				reflectionPolishAssetId,
				"ReflectionPolishProbe") ==
				"RenamedReflectionProbe",
			"Script UUID did not resolve the renamed class") ||
		!Check(
			reflectionPolishRuntime.GetInstanceClassName(
				reflectionPolishInstance) ==
				"RenamedReflectionProbe",
			"Live script instance did not follow the renamed class") ||
		!Check(
			reflectionPolishRuntime.GetReflectedInstance(
				reflectionPolishInstance) &&
				ReadNumber(
					reflectionPolishRuntime.GetReflectedInstance(
						reflectionPolishInstance),
					"mode") == 2.0,
			"Renamed live script instance was not recreated with its state"))
	{
		return 1;
	}
	reflectionPolishRuntime.DestroyInstance(reflectionPolishInstance);
	reflectionPolishRuntime.Shutdown();

	EGE::TimeService engineTime;
	engineTime.BeginPlay();
	if (!Check(
			engineTime.IsPlaying() &&
				!engineTime.IsPaused() &&
				engineTime.GetTime() == 0.0 &&
				engineTime.GetUnscaledTime() == 0.0 &&
				engineTime.GetFrameCount() == 0,
			"Play did not reset the engine timeline"))
	{
		return 1;
	}

	engineTime.BeginFrame(0.01f);
	engineTime.SetTimeScale(0.5f);
	engineTime.BeginFrame(0.02f);
	if (!Check(
			std::abs(engineTime.GetDeltaTime() - 0.01f) < 0.0001f &&
				std::abs(engineTime.GetTime() - 0.02) < 0.0001 &&
				std::abs(engineTime.GetUnscaledTime() - 0.03) < 0.0001 &&
				engineTime.GetFrameCount() == 2,
			"Scaled and unscaled engine time diverged incorrectly"))
	{
		return 1;
	}

	const double scaledTimeBeforePause = engineTime.GetTime();
	engineTime.Pause();
	engineTime.BeginFrame(0.5f);
	if (!Check(
			engineTime.IsPaused() &&
				engineTime.GetDeltaTime() == 0.0f &&
				engineTime.GetFixedStepCount() == 0 &&
				engineTime.GetTime() == scaledTimeBeforePause &&
				engineTime.GetUnscaledTime() > 0.5 &&
				engineTime.GetFrameCount() == 3,
			"Pause did not stop scaled time while retaining real time"))
	{
		return 1;
	}

	engineTime.Resume();
	engineTime.SetTimeScale(1.0f);
	engineTime.DiscardPendingFixedSteps();
	engineTime.BeginFrame(1.0f);
	if (!Check(
			engineTime.GetFixedStepCount() ==
				EGE::TimeService::MaximumFixedStepsPerFrame &&
				engineTime.GetFixedInterpolationAlpha() >= 0.0f &&
				engineTime.GetFixedInterpolationAlpha() < 1.0f,
			"Fixed timestep catch-up was not bounded or stable"))
	{
		return 1;
	}
	engineTime.DiscardPendingFixedSteps();
	if (!Check(
			engineTime.GetFixedStepCount() == 0 &&
				engineTime.GetFixedInterpolationAlpha() == 0.0f,
			"Scene reset did not discard pending fixed steps"))
	{
		return 1;
	}

	engineTime.BeginPlay();
	if (!Check(
			engineTime.GetTime() == 0.0 &&
				engineTime.GetUnscaledTime() == 0.0 &&
				engineTime.GetFrameCount() == 0 &&
				engineTime.GetTimeScale() == 1.0f,
			"A subsequent Play did not start from a clean timeline"))
	{
		return 1;
	}

	TemporaryProject timeProject;
	timeProject.WriteScript(
		"[ScriptComponent]\n"
		"class TimeProbe : EGEBehaviour\n"
		"{\n"
		"    double scaledTime = -1.0;\n"
		"    double realTime = -1.0;\n"
		"    float delta = -1.0f;\n"
		"    float realDelta = -1.0f;\n"
		"    float fixedDelta = -1.0f;\n"
		"    uint64 frame = 0;\n"
		"    bool playing = false;\n"
		"\n"
		"    void OnUpdate(float deltaTime)\n"
		"    {\n"
		"        scaledTime = Time::time;\n"
		"        realTime = Time::unscaledTime;\n"
		"        delta = Time::deltaTime;\n"
		"        realDelta = Time::unscaledDeltaTime;\n"
		"        fixedDelta = Time::fixedDeltaTime;\n"
		"        frame = Time::frameCount;\n"
		"        playing = Time::isPlaying && !Time::isPaused;\n"
		"        Time::timeScale = 0.25f;\n"
		"    }\n"
		"}\n");

	testTimeService = &engineTime;
	EGE::SetTimeServiceProvider(ResolveTestTimeService);
	EGE::ScriptRuntime timeRuntime;
	std::string timeRegistrationError;
	if (!Check(
			timeRuntime.RegisterApi(
				"Engine.Time",
				EGE::RegisterTimeApi,
				timeRegistrationError),
			"Time API registration was rejected") ||
		!Check(
			timeRuntime.Initialize(),
			"Time runtime initialization failed"))
	{
		std::cerr << timeRegistrationError << '\n';
		return 1;
	}

	engineTime.BeginFrame(0.02f);
	if (!Check(
			timeRuntime.SetProjectRoot(timeProject.Root()),
			"Time API script did not compile"))
	{
		for (const EGE::ScriptDiagnostic& diagnostic :
			timeRuntime.GetDiagnostics())
		{
			std::cerr
				<< diagnostic.file.string() << ':'
				<< diagnostic.line << ':'
				<< diagnostic.column << ' '
				<< diagnostic.message << '\n';
		}
		return 1;
	}

	const EGE::ScriptInstanceHandle timeInstance =
		timeRuntime.CreateInstance("TimeProbe");
	timeRuntime.EnterPlayMode();
	timeRuntime.StartInstance(timeInstance);
	timeRuntime.UpdateInstance(
		timeInstance, engineTime.GetDeltaTime());
	const EGE::ReflectedScriptObject reflectedTime =
		timeRuntime.GetReflectedInstance(timeInstance);
	const EGE::PropertyDescriptor* playingProperty =
		FindProperty(reflectedTime, "playing");
	EGE::PropertyValue playingValue;
	if (!Check(
			reflectedTime &&
				std::abs(
					ReadNumber(reflectedTime, "delta") - 0.02) <
					0.0001 &&
				ReadNumber(reflectedTime, "frame") == 1.0 &&
				playingProperty &&
				playingProperty->Read(
					reflectedTime.object, playingValue) &&
				std::get<bool>(playingValue) &&
				engineTime.GetTimeScale() == 0.25f,
			"AngelScript Time properties do not match the engine timeline"))
	{
		return 1;
	}

	std::ifstream timeDefinitions(
		timeProject.Root() / "as.predefined");
	const std::string timeLanguageServerApi(
		(std::istreambuf_iterator<char>(timeDefinitions)),
		std::istreambuf_iterator<char>());
	if (!Check(
			timeLanguageServerApi.find(
				"namespace Time {\n"
				"    float get_deltaTime() property;") !=
				std::string::npos &&
			timeLanguageServerApi.find(
				"uint64 get_frameCount() property;") !=
				std::string::npos,
			"Language-server definitions do not include the Time API"))
	{
		return 1;
	}

	const double timeBeforeReload = engineTime.GetTime();
	const std::uint64_t frameBeforeReload =
		engineTime.GetFrameCount();
	if (!Check(
			timeRuntime.ForceReload(),
			"Time API script hot reload failed") ||
		!Check(
			engineTime.GetTime() == timeBeforeReload &&
				engineTime.GetFrameCount() == frameBeforeReload,
			"Script hot reload modified the engine timeline"))
	{
		return 1;
	}
	timeRuntime.LeavePlayMode();
	timeRuntime.DestroyInstance(timeInstance);
	timeRuntime.Shutdown();
	EGE::SetTimeServiceProvider(nullptr);
	testTimeService = nullptr;

	engineTime.ResetForProjectChange();
	if (!Check(
			!engineTime.IsPlaying() &&
				!engineTime.IsPaused() &&
				engineTime.GetTime() == 0.0 &&
				engineTime.GetUnscaledTime() == 0.0 &&
				engineTime.GetFrameCount() == 0 &&
				engineTime.GetFixedStepCount() == 0 &&
				engineTime.GetTimeScale() == 1.0f,
			"Project reset retained timeline state"))
	{
		return 1;
	}

	TemporaryProject createdAssetProject;
	std::string workspaceError;
	if (!Check(
			EGE::EnsureVsCodeWorkspace(
				createdAssetProject.Root(), workspaceError),
			"VS Code workspace configuration failed"))
	{
		std::cerr << workspaceError << '\n';
		return 1;
	}
	std::ifstream workspaceSettings(
		createdAssetProject.Root() / ".vscode" / "settings.json");
	const std::string workspaceSettingsText(
		(std::istreambuf_iterator<char>(workspaceSettings)),
		std::istreambuf_iterator<char>());
	if (!Check(
			workspaceSettingsText.find("Assets/**/*.as") != std::string::npos,
			"VS Code workspace does not include AngelScript assets"))
	{
		return 1;
	}

	const EGE::ScriptAssetCreationResult createdAsset =
		EGE::ScriptAsset::Create(
			createdAssetProject.Root(),
			"Assets/Gameplay/New Player Script.as",
			"New Player Script");
	if (!Check(
			static_cast<bool>(createdAsset),
			"AngelScript asset creation failed") ||
		!Check(
			createdAsset.className == "NewPlayerScript",
			"AngelScript class name generation failed") ||
		!Check(
			EGE::ScriptResource::IsAssetId(createdAsset.assetId),
			"AngelScript asset identifier generation failed"))
	{
		std::cerr << createdAsset.error << '\n';
		return 1;
	}

	EGE::ScriptRuntime createdAssetRuntime;
	if (!Check(
			createdAssetRuntime.Initialize(),
			"Created asset runtime initialization failed") ||
		!Check(
			createdAssetRuntime.SetProjectRoot(createdAssetProject.Root()),
			"Created AngelScript asset did not compile") ||
		!Check(
			createdAssetRuntime.HasClass("NewPlayerScript"),
			"Created AngelScript class was not discovered"))
	{
		return 1;
	}
	if (!Check(
			createdAssetRuntime.ResolveClass(createdAsset.assetId) ==
				"NewPlayerScript",
			"AngelScript asset identifier did not resolve its class"))
	{
		return 1;
	}
	createdAssetRuntime.Shutdown();

	TemporaryProject mathProject;
	mathProject.WriteScript(
		"[ScriptComponent]\n"
		"class MathProbe : EGEBehaviour\n"
		"{\n"
		"    [SerializeField]\n"
		"    [Range(-10, 10)]\n"
		"    Vector3 direction = Vector3(1, 2, 3);\n"
		"\n"
		"    [SerializeField]\n"
		"    Color tint = Color(0.25f, 0.5f, 0.75f, 1.0f);\n"
		"\n"
		"    private float result = 0.0f;\n"
		"\n"
		"    void OnUpdate(float deltaTime)\n"
		"    {\n"
		"        Vector3 sum = direction + Vector3(3, 2, 1);\n"
		"        Vector3 unit = Vector3(0, 3, 4).normalized;\n"
		"        direction = Math::Lerp(\n"
		"            direction, Math::Vector3Forward * 4.0f, 0.5f);\n"
		"        tint = Math::Lerp(tint, Math::ColorWhite, 0.5f);\n"
		"        result = Math::Dot(sum, Math::Vector3One) +\n"
		"            Math::Clamp(2.5f, 0.0f, 2.0f) + unit.length;\n"
		"    }\n"
		"}\n");

	EGE::ScriptRuntime mathRuntime;
	std::string mathRegistrationError;
	if (!Check(
			mathRuntime.RegisterApi(
				"Engine.Math",
				EGE::RegisterMathApi,
				mathRegistrationError),
			"Math API registration was rejected") ||
		!Check(
			mathRuntime.Initialize(),
			"Math runtime initialization failed") ||
		!Check(
			mathRuntime.SetProjectRoot(mathProject.Root()),
			"Math project root was rejected") ||
		!Check(
			mathRuntime.HasClass("MathProbe"),
			"Math script did not compile"))
	{
		std::cerr << mathRegistrationError << '\n';
		for (const EGE::ScriptDiagnostic& diagnostic :
			mathRuntime.GetDiagnostics())
		{
			std::cerr
				<< diagnostic.file.string() << ':'
				<< diagnostic.line << ':'
				<< diagnostic.column << ' '
				<< diagnostic.message << '\n';
		}
		return 1;
	}

	const EGE::ScriptInstanceHandle mathInstance =
		mathRuntime.CreateInstance("MathProbe");
	mathRuntime.EnterPlayMode();
	mathRuntime.StartInstance(mathInstance);
	mathRuntime.UpdateInstance(mathInstance, 1.0f / 60.0f);

	EGE::ReflectedScriptObject mathReflected =
		mathRuntime.GetReflectedInstance(mathInstance);
	const EGE::PropertyDescriptor* direction =
		FindProperty(mathReflected, "direction");
	const EGE::PropertyDescriptor* tint =
		FindProperty(mathReflected, "tint");
	EGE::PropertyValue directionValue;
	EGE::PropertyValue tintValue;
	const EGE::Vector3Value expectedDirection{0.5f, 1.0f, 3.5f};
	const EGE::ColorValue expectedTint{0.625f, 0.75f, 0.875f, 1.0f};
	const bool directionRead =
		direction &&
		direction->Read(mathReflected.object, directionValue);
	const bool tintRead =
		tint &&
		tint->Read(mathReflected.object, tintValue);
	if (directionRead &&
		std::holds_alternative<EGE::Vector3Value>(directionValue))
	{
		const auto actual = std::get<EGE::Vector3Value>(directionValue);
		if (actual != expectedDirection)
		{
			std::cerr
				<< "Actual Vector3: "
				<< actual.x << ", " << actual.y << ", " << actual.z << '\n';
		}
	}
	if (!directionRead ||
		!std::holds_alternative<EGE::Vector3Value>(directionValue))
	{
		std::cerr
			<< "Vector3 reflection state: object="
			<< static_cast<bool>(mathReflected)
			<< " property=" << (direction != nullptr)
			<< " kind="
			<< (direction
				? static_cast<int>(direction->kind)
				: -1)
			<< " type="
			<< (direction ? direction->typeName : std::string())
			<< " read=" << directionRead
			<< " value-index=" << directionValue.index()
			<< '\n';
	}
	if (!Check(
			direction &&
				direction->kind == EGE::PropertyKind::Vector3 &&
				directionRead &&
				std::get<EGE::Vector3Value>(directionValue) ==
					expectedDirection,
			"Vector3 operators or reflection returned an unexpected value") ||
		!Check(
			tint &&
				tint->kind == EGE::PropertyKind::Color &&
				tintRead &&
				std::get<EGE::ColorValue>(tintValue) == expectedTint,
			"Color math or reflection returned an unexpected value") ||
		!Check(
			std::abs(ReadNumber(mathReflected, "result") - 15.0) < 0.001,
			"Math scalar or Vector3 functions returned an unexpected value"))
	{
		return 1;
	}

	const EGE::PropertyBag mathState =
		mathRuntime.CaptureInstanceState(mathInstance, true);
	Config savedMathProperties;
	EGE::SavePropertyBag(
		savedMathProperties, "Properties", mathState);
	char* serializedMathJson = nullptr;
	savedMathProperties.Save(&serializedMathJson, nullptr);
	Config loadedMathProperties(serializedMathJson);
	delete[] serializedMathJson;
	const EGE::PropertyBag loadedMathState =
		EGE::LoadPropertyBag(loadedMathProperties, "Properties");
	const EGE::PropertyState* loadedDirection =
		FindState(loadedMathState, "direction");
	const EGE::PropertyState* loadedTint =
		FindState(loadedMathState, "tint");
	if (!Check(
			loadedDirection && loadedTint,
			"Vector3 and Color serialization did not retain both properties") ||
		!Check(
			std::get<EGE::Vector3Value>(loadedDirection->value) ==
				expectedDirection,
			"Vector3 JSON serialization did not round-trip") ||
		!Check(
			std::get<EGE::ColorValue>(loadedTint->value) ==
				expectedTint,
			"Color JSON serialization did not round-trip"))
	{
		return 1;
	}

	mathProject.WriteScript(
		"[ScriptComponent]\n"
		"class MathProbe : EGEBehaviour\n"
		"{\n"
		"    [SerializeField]\n"
		"    Vector3 direction = Vector3(9, 9, 9);\n"
		"    [SerializeField]\n"
		"    Color tint = Math::ColorBlack;\n"
		"    private float result = -1.0f;\n"
		"    void OnUpdate(float deltaTime) {}\n"
		"}\n");
	if (!Check(
			mathRuntime.ForceReload(),
			"Math script hot reload failed"))
	{
		return 1;
	}

	mathReflected = mathRuntime.GetReflectedInstance(mathInstance);
	direction = FindProperty(mathReflected, "direction");
	tint = FindProperty(mathReflected, "tint");
	directionValue = {};
	tintValue = {};
	if (!Check(
			direction &&
				direction->Read(mathReflected.object, directionValue) &&
				std::get<EGE::Vector3Value>(directionValue) ==
					expectedDirection,
			"Vector3 state was not preserved during hot reload") ||
		!Check(
			tint &&
				tint->Read(mathReflected.object, tintValue) &&
				std::get<EGE::ColorValue>(tintValue) == expectedTint,
			"Color state was not preserved during hot reload"))
	{
		return 1;
	}

	std::ifstream mathDefinitions(mathProject.Root() / "as.predefined");
	const std::string mathLanguageServerApi(
		(std::istreambuf_iterator<char>(mathDefinitions)),
		std::istreambuf_iterator<char>());
	if (!Check(
			mathLanguageServerApi.find(
				"namespace Math {\n"
				"    Vector3 Cross(") != std::string::npos &&
			mathLanguageServerApi.find(
				"namespace Math {\n"
				"    Color Lerp(") != std::string::npos &&
			mathLanguageServerApi.find(
				"float Math::Log(") == std::string::npos &&
			mathLanguageServerApi.find(
				"Vector3(float x = 0, float y = 0, "
				"float z = 0);") != std::string::npos &&
			mathLanguageServerApi.find(
				"Color(float r = 0, float g = 0, "
				"float b = 0, float a = 1);") != std::string::npos,
			"Language-server definitions do not include the Math API"))
	{
		return 1;
	}
	mathRuntime.LeavePlayMode();
	mathRuntime.DestroyInstance(mathInstance);
	mathRuntime.Shutdown();

	TemporaryProject ownerProject;
	ownerProject.WriteScript(
		"[ScriptComponent]\n"
		"class OwnerProbe : EGEBehaviour\n"
		"{\n"
		"    bool ownerBound = false;\n"
		"    void OnStart()\n"
		"    {\n"
		"        ownerBound = gameObject !is null && transform !is null;\n"
		"    }\n"
		"}\n");
	EGE::ScriptRuntime ownerRuntime;
	if (!Check(ownerRuntime.Initialize(), "Owner runtime initialization failed") ||
		!Check(
			ownerRuntime.SetProjectRoot(ownerProject.Root()),
			"Owner script did not compile"))
	{
		return 1;
	}
	const EGE::ScriptInstanceHandle ownerInstance =
		ownerRuntime.CreateInstance("OwnerProbe");
	ownerRuntime.SetInstanceOwnerId(ownerInstance, 1);
	ownerRuntime.StartInstance(ownerInstance);
	const EGE::ReflectedScriptObject ownerReflected =
		ownerRuntime.GetReflectedInstance(ownerInstance);
	EGE::PropertyValue ownerBoundValue;
	const EGE::PropertyDescriptor* ownerBound =
		FindProperty(ownerReflected, "ownerBound");
	if (!Check(
			ownerBound && ownerBound->Read(ownerReflected.object, ownerBoundValue) &&
				std::get<bool>(ownerBoundValue),
			"EGEBehaviour did not receive its GameObject and Transform handles") ||
		!Check(
			FindProperty(ownerReflected, "gameObject") == nullptr &&
				FindProperty(ownerReflected, "transform") == nullptr &&
				FindProperty(ownerReflected, "enabled") == nullptr,
			"EGEBehaviour runtime properties were exposed through reflection"))
	{
		return 1;
	}
	ownerRuntime.Shutdown();

	EGE::PropertyAttributes exposureAttributes;
	exposureAttributes.range = EGE::PropertyRange{0.0, 4.0};
	EGE::TypeDescriptor nativeDescriptor;
	nativeDescriptor.domain = "EngineTest";
	nativeDescriptor.id = "NativeSettings";
	nativeDescriptor.displayName = "Native Settings";
	nativeDescriptor.nativeType = typeid(NativeSettings);
	nativeDescriptor.properties.push_back(
		EGE::MakeMemberProperty(
			"exposure",
			&NativeSettings::exposure,
			exposureAttributes));
	nativeDescriptor.properties.push_back(
		EGE::MakeMemberProperty(
			"profile",
			&NativeSettings::profile));

	std::string registryError;
	if (!Check(
			EGE::TypeRegistry::Get().Register(
				std::move(nativeDescriptor), registryError),
			"C++ type registration failed"))
	{
		std::cerr << registryError << '\n';
		return 1;
	}

	NativeSettings nativeSettings;
	const auto reflectedNative =
		EGE::TypeRegistry::Get().Find(typeid(NativeSettings));
	const EGE::PropertyDescriptor* exposure =
		reflectedNative
			? reflectedNative->FindProperty("exposure")
			: nullptr;
	if (!Check(exposure != nullptr, "C++ property was not registered") ||
		!Check(
			exposure->Write(&nativeSettings, 12.0),
			"C++ reflected property write failed") ||
		!Check(
			std::abs(nativeSettings.exposure - 4.0f) < 0.001f,
			"C++ reflected range was not enforced"))
	{
		return 1;
	}

	TemporaryProject project;
	project.WriteScript(
		"void OnStart() { Log(\"started\"); }\n"
		"void OnUpdate(float deltaTime) {}\n"
		"void OnStop() { Log(\"stopped\"); }\n"
		"\n"
		"[ScriptComponent]\n"
		"class PlayerController\n"
		"{\n"
		"    [Header(\"Movement\")]\n"
		"    [Range(0, 10)]\n"
		"    float speed = 2.0f;\n"
		"\n"
		"    [SerializeField]\n"
		"    private int score = 7;\n"
		"\n"
		"    private int transientCounter = 3;\n"
		"\n"
		"    [HideInInspector]\n"
		"    [SerializeField]\n"
		"    string internalName = \"hidden\";\n"
		"\n"
		"    [SerializeField]\n"
		"    GameObject@ targetObject;\n"
		"\n"
		"    [SerializeField]\n"
		"    Component@ targetComponent;\n"
		"\n"
		"    void OnUpdate(float deltaTime)\n"
		"    {\n"
		"        score += 1;\n"
		"        transientCounter += 1;\n"
		"    }\n"
		"\n"
		"    void OnBeforeReload() { score += 10; }\n"
		"    void OnAfterReload() { score += 100; }\n"
		"}\n");

	EGE::ScriptRuntime runtime;
	if (!Check(runtime.Initialize(), "Runtime initialization failed") ||
		!Check(
			runtime.SetProjectRoot(project.Root()),
			"Initial script compilation failed") ||
		!Check(runtime.HasLoadedScripts(), "No script module was loaded"))
	{
		for (const EGE::ScriptDiagnostic& diagnostic :
			runtime.GetDiagnostics())
		{
			std::cerr
				<< diagnostic.file.string() << ':'
				<< diagnostic.line << ':'
				<< diagnostic.column << ' '
				<< diagnostic.message << '\n';
		}
		return 1;
	}

	const std::vector<EGE::ScriptClassInfo> classes =
		runtime.GetAvailableClasses();
	if (!Check(
			classes.size() == 1 &&
				classes.front().name == "PlayerController" &&
				EGE::ScriptResource::IsAssetId(classes.front().assetId),
			"Script class discovery returned an unexpected result"))
	{
		return 1;
	}

	const EGE::ScriptInstanceHandle instance =
		runtime.CreateInstance("PlayerController");
	EGE::ReflectedScriptObject reflected =
		runtime.GetReflectedInstance(instance);
	const EGE::PropertyDescriptor* speed =
		FindProperty(reflected, "speed");
	const EGE::PropertyDescriptor* score =
		FindProperty(reflected, "score");
	const EGE::PropertyDescriptor* transient =
		FindProperty(reflected, "transientCounter");
	const EGE::PropertyDescriptor* hidden =
		FindProperty(reflected, "internalName");
	const EGE::PropertyDescriptor* targetObject =
		FindProperty(reflected, "targetObject");
	const EGE::PropertyDescriptor* targetComponent =
		FindProperty(reflected, "targetComponent");
	if (!Check(instance != 0 && reflected, "Script instance was not created") ||
		!Check(
			speed && score && transient && hidden &&
				targetObject && targetComponent,
			"Script properties were not reflected") ||
		!Check(
			speed->attributes.header == "Movement" &&
				speed->attributes.range.has_value(),
			"Header or Range metadata was not reflected") ||
		!Check(
			score->attributes.serialized && score->attributes.visible,
			"SerializeField metadata was not applied") ||
		!Check(
			!transient->attributes.serialized &&
				!transient->attributes.visible,
			"Private script property visibility is incorrect") ||
		!Check(
			hidden->attributes.serialized &&
				!hidden->attributes.visible,
			"HideInInspector metadata was not applied") ||
		!Check(
			speed->Write(reflected.object, 50.0) &&
				std::abs(ReadNumber(reflected, "speed") - 10.0) < 0.001,
			"Script Range metadata did not clamp the property") ||
		!Check(
			targetObject->kind ==
				EGE::PropertyKind::GameObjectReference &&
				targetObject->Write(
					reflected.object,
					EGE::GameObjectReferenceValue{41}),
			"GameObject reference property was not writable") ||
		!Check(
			targetComponent->kind ==
				EGE::PropertyKind::ComponentReference &&
				targetComponent->Write(
					reflected.object,
					EGE::ComponentReferenceValue{41, 73}),
			"Component reference property was not writable"))
	{
		return 1;
	}

	const unsigned long long firstGeneration = runtime.GetGeneration();
	runtime.EnterPlayMode();
	runtime.StartInstance(instance);
	runtime.UpdateInstance(instance, 1.0f / 60.0f);
	runtime.Tick(1.0f / 60.0f);

	project.WriteScript("void OnStart( {");
	if (!Check(
			!runtime.ForceReload(),
			"Invalid script unexpectedly compiled") ||
		!Check(
			runtime.HasLoadedScripts(),
			"Failed reload discarded the running module") ||
		!Check(
			runtime.GetGeneration() == firstGeneration,
			"Failed reload changed the active generation") ||
		!Check(
			!runtime.GetDiagnostics().empty(),
			"Compiler diagnostics were not retained"))
	{
		return 1;
	}
	reflected = runtime.GetReflectedInstance(instance);
	if (!Check(
			reflected &&
				std::abs(ReadNumber(reflected, "score") - 8.0) < 0.001,
			"Failed reload did not preserve the live script instance"))
	{
		return 1;
	}

	project.WriteScript(
		"void OnStart() { Log(\"reloaded\"); }\n"
		"void OnUpdate(float deltaTime) {}\n"
		"void OnStop() {}\n"
		"\n"
		"[ScriptComponent]\n"
		"class PlayerController\n"
		"{\n"
		"    [Header(\"Movement\")]\n"
		"    [Range(0, 10)]\n"
		"    float speed = 1.0f;\n"
		"\n"
		"    [SerializeField]\n"
		"    private int score = 0;\n"
		"\n"
		"    private int transientCounter = 999;\n"
		"\n"
		"    [HideInInspector]\n"
		"    [SerializeField]\n"
		"    string internalName = \"changed\";\n"
		"\n"
		"    [SerializeField]\n"
		"    GameObject@ targetObject;\n"
		"\n"
		"    [SerializeField]\n"
		"    Component@ targetComponent;\n"
		"\n"
		"    void OnUpdate(float deltaTime) { score += 2; }\n"
		"    void OnAfterReload() { score += 100; }\n"
		"}\n");
	if (!Check(runtime.ForceReload(), "Corrected script did not reload") ||
		!Check(
			runtime.GetGeneration() > firstGeneration,
			"Successful reload did not advance the generation") ||
		!Check(
			std::filesystem::is_regular_file(
				project.Root() / "as.predefined"),
			"Language-server declarations were not generated"))
	{
		return 1;
	}

	reflected = runtime.GetReflectedInstance(instance);
	const EGE::PropertyBag serializedState =
		runtime.CaptureInstanceState(instance, true);
	if (serializedState.size() != 5)
	{
		std::cerr << "Serialized properties:";
		for (const EGE::PropertyState& property : serializedState)
			std::cerr << ' ' << property.name;
		std::cerr << '\n';
		for (const EGE::PropertyDescriptor& property :
			reflected.type->properties)
		{
			EGE::PropertyValue value;
			std::cerr
				<< property.name
				<< " kind=" << static_cast<int>(property.kind)
				<< " type=" << property.typeName
				<< " serialized=" << property.attributes.serialized
				<< " visible=" << property.attributes.visible
				<< " readable="
				<< property.Read(reflected.object, value)
				<< '\n';
		}
	}
	EGE::PropertyValue targetObjectValue;
	EGE::PropertyValue targetComponentValue;
	const EGE::PropertyDescriptor* reloadedTargetObject =
		FindProperty(reflected, "targetObject");
	const EGE::PropertyDescriptor* reloadedTargetComponent =
		FindProperty(reflected, "targetComponent");
	if (!Check(
			static_cast<bool>(reflected),
			"Script instance was not rebound after reload") ||
		!Check(
			std::abs(ReadNumber(reflected, "speed") - 10.0) < 0.001,
			"Edited script property was not preserved during reload") ||
		!Check(
			std::abs(ReadNumber(reflected, "score") - 118.0) < 0.001,
			"Script lifecycle state was not preserved during reload") ||
		!Check(
			std::abs(ReadNumber(reflected, "transientCounter") - 4.0) < 0.001,
			"Non-serialized runtime state was not preserved during reload") ||
		!Check(
			serializedState.size() == 5,
			"Serialized script property filtering is incorrect") ||
		!Check(
			reloadedTargetObject &&
				reloadedTargetObject->Read(
					reflected.object, targetObjectValue) &&
				std::get<EGE::GameObjectReferenceValue>(
					targetObjectValue).objectId == 41,
			"GameObject reference was not preserved during reload") ||
		!Check(
			reloadedTargetComponent &&
				reloadedTargetComponent->Read(
					reflected.object, targetComponentValue) &&
				std::get<EGE::ComponentReferenceValue>(
					targetComponentValue).objectId == 41 &&
				std::get<EGE::ComponentReferenceValue>(
					targetComponentValue).componentId == 73,
			"Component reference was not preserved during reload"))
	{
		return 1;
	}

	Config savedProperties;
	EGE::SavePropertyBag(
		savedProperties, "Properties", serializedState);
	char* serializedJson = nullptr;
	savedProperties.Save(&serializedJson, nullptr);
	Config loadedProperties(serializedJson);
	delete[] serializedJson;
	const EGE::PropertyBag loadedState =
		EGE::LoadPropertyBag(loadedProperties, "Properties");
	const EGE::ScriptInstanceHandle restoredInstance =
		runtime.CreateInstance("PlayerController", loadedState);
	const EGE::ReflectedScriptObject restored =
		runtime.GetReflectedInstance(restoredInstance);
	EGE::PropertyValue restoredObjectValue;
	EGE::PropertyValue restoredComponentValue;
	const EGE::PropertyDescriptor* restoredTargetObject =
		FindProperty(restored, "targetObject");
	const EGE::PropertyDescriptor* restoredTargetComponent =
		FindProperty(restored, "targetComponent");
	if (!Check(
			loadedState.size() == serializedState.size(),
			"Serialized script property JSON did not round-trip") ||
		!Check(
			restored &&
				std::abs(ReadNumber(restored, "speed") - 10.0) < 0.001 &&
				std::abs(ReadNumber(restored, "score") - 118.0) < 0.001,
			"Saved script properties were not restored into an instance") ||
		!Check(
			restoredTargetObject &&
				restoredTargetObject->Read(
					restored.object, restoredObjectValue) &&
				std::get<EGE::GameObjectReferenceValue>(
					restoredObjectValue).objectId == 41,
			"Serialized GameObject reference did not round-trip") ||
		!Check(
			restoredTargetComponent &&
				restoredTargetComponent->Read(
					restored.object, restoredComponentValue) &&
				std::get<EGE::ComponentReferenceValue>(
					restoredComponentValue).objectId == 41 &&
				std::get<EGE::ComponentReferenceValue>(
					restoredComponentValue).componentId == 73,
			"Serialized Component reference did not round-trip"))
	{
		return 1;
	}
	runtime.DestroyInstance(restoredInstance);

	std::ifstream definitions(project.Root() / "as.predefined");
	const std::string languageServerApi(
		(std::istreambuf_iterator<char>(definitions)),
		std::istreambuf_iterator<char>());
	if (!Check(
			languageServerApi.find(
				"void Log(const string&in message);") != std::string::npos,
			"Language-server API does not match the runtime binding"))
	{
		return 1;
	}

	std::ifstream checkedInDefinitions(
		std::filesystem::path(EGE_SOURCE_ROOT) / "as.predefined");
	const std::string checkedInLanguageServerApi(
		(std::istreambuf_iterator<char>(checkedInDefinitions)),
		std::istreambuf_iterator<char>());
	if (checkedInLanguageServerApi != languageServerApi)
	{
		std::cerr
			<< "The checked-in VS Code API differs from the runtime API\n"
			<< "Generated:\n"
			<< languageServerApi
			<< "Checked in:\n"
			<< checkedInLanguageServerApi;
		return 1;
	}

	runtime.LeavePlayMode();
	runtime.DestroyInstance(instance);
	runtime.Shutdown();
	EGE::TypeRegistry::Get().ClearDomain("EngineTest");

	TemporaryProject brokenProject;
	brokenProject.WriteScript("void OnStart( {");
	EGE::ScriptRuntime brokenRuntime;
	if (!Check(
			brokenRuntime.Initialize(),
			"Runtime initialization for broken project failed") ||
		!Check(
			brokenRuntime.SetProjectRoot(brokenProject.Root()),
			"A script compiler error rejected the entire project") ||
		!Check(
			!brokenRuntime.HasLoadedScripts(),
			"Broken project unexpectedly created an active module") ||
		!Check(
			!brokenRuntime.WasLastReloadSuccessful(),
			"Broken project's compilation status was not retained"))
	{
		return 1;
	}
	brokenRuntime.Shutdown();
	return 0;
}
