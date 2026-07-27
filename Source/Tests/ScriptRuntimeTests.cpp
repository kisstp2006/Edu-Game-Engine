#include "../Scripting/ScriptRuntime.h"
#include "../Scripting/ScriptAsset.h"
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
}

int main()
{
	TemporaryProject createdAssetProject;
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
			"AngelScript class name generation failed"))
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
	createdAssetRuntime.Shutdown();

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
				classes.front().name == "PlayerController",
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
	if (!Check(instance != 0 && reflected, "Script instance was not created") ||
		!Check(speed && score && transient && hidden, "Script properties were not reflected") ||
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
			"Script Range metadata did not clamp the property"))
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
	if (serializedState.size() != 3)
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
			serializedState.size() == 3,
			"Serialized script property filtering is incorrect"))
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
	if (!Check(
			loadedState.size() == serializedState.size(),
			"Serialized script property JSON did not round-trip") ||
		!Check(
			restored &&
				std::abs(ReadNumber(restored, "speed") - 10.0) < 0.001 &&
				std::abs(ReadNumber(restored, "score") - 118.0) < 0.001,
			"Saved script properties were not restored into an instance"))
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
