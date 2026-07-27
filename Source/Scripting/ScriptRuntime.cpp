#include "ScriptRuntime.h"

#include "../Globals.h"
#include "../Reflection/PropertySerializer.h"

#include <angelscript.h>
#include <scriptbuilder/scriptbuilder.h>
#include <scriptstdstring/scriptstdstring.h>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <map>
#include <optional>
#include <sstream>
#include <system_error>
#include <unordered_map>
#include <utility>

namespace EGE
{
	namespace
	{
		constexpr std::chrono::milliseconds ScanInterval(250);
		constexpr std::chrono::milliseconds ReloadDebounce(200);
		constexpr const char* ScriptReflectionDomain = "AngelScript";

		void ScriptLog(const std::string& message)
		{
			LOG("[AngelScript] %s", message.c_str());
		}

		ScriptDiagnosticSeverity ToSeverity(asEMsgType type)
		{
			switch (type)
			{
				case asMSGTYPE_ERROR:
					return ScriptDiagnosticSeverity::Error;
				case asMSGTYPE_WARNING:
					return ScriptDiagnosticSeverity::Warning;
				default:
					return ScriptDiagnosticSeverity::Information;
			}
		}

		const char* ToLabel(ScriptDiagnosticSeverity severity)
		{
			switch (severity)
			{
				case ScriptDiagnosticSeverity::Error:
					return "error";
				case ScriptDiagnosticSeverity::Warning:
					return "warning";
				default:
					return "info";
			}
		}

		bool ReadTextFile(
			const std::filesystem::path& path, std::string& contents)
		{
			std::ifstream stream(path, std::ios::binary);
			if (!stream)
				return false;

			std::ostringstream buffer;
			buffer << stream.rdbuf();
			if (!stream.good() && !stream.eof())
				return false;

			contents = buffer.str();
			return true;
		}

		bool WriteIfDifferent(
			const std::filesystem::path& path, const std::string& contents)
		{
			std::string current;
			if (ReadTextFile(path, current) && current == contents)
				return true;

			std::ofstream stream(
				path, std::ios::binary | std::ios::trunc);
			if (!stream)
				return false;

			stream.write(
				contents.data(),
				static_cast<std::streamsize>(contents.size()));
			return stream.good();
		}

		std::string Trim(std::string text)
		{
			const auto first = std::find_if_not(
				text.begin(), text.end(),
				[](unsigned char character)
				{
					return std::isspace(character) != 0;
				});
			const auto last = std::find_if_not(
				text.rbegin(), text.rend(),
				[](unsigned char character)
				{
					return std::isspace(character) != 0;
				}).base();
			if (first >= last)
				return {};
			return std::string(first, last);
		}

		bool EqualsIgnoreCase(
			const std::string& left, const std::string& right)
		{
			if (left.size() != right.size())
				return false;
			for (std::size_t index = 0; index < left.size(); ++index)
			{
				if (std::tolower(
						static_cast<unsigned char>(left[index])) !=
					std::tolower(
						static_cast<unsigned char>(right[index])))
				{
					return false;
				}
			}
			return true;
		}

		struct ParsedMetadata
		{
			std::string name;
			std::string arguments;
		};

		ParsedMetadata ParseMetadata(std::string metadata)
		{
			metadata = Trim(std::move(metadata));
			if (metadata.size() >= 2 &&
				metadata.front() == '[' && metadata.back() == ']')
			{
				metadata = Trim(
					metadata.substr(1, metadata.size() - 2));
			}

			const std::size_t opening = metadata.find('(');
			if (opening == std::string::npos)
				return {Trim(std::move(metadata)), {}};

			const std::size_t closing = metadata.find_last_of(')');
			if (closing == std::string::npos || closing < opening)
				return {Trim(metadata.substr(0, opening)), {}};

			return {
				Trim(metadata.substr(0, opening)),
				Trim(metadata.substr(opening + 1, closing - opening - 1))};
		}

		std::optional<std::string> FindMetadataArguments(
			const std::vector<std::string>& metadata,
			const std::string& name)
		{
			for (const std::string& value : metadata)
			{
				const ParsedMetadata parsed = ParseMetadata(value);
				if (EqualsIgnoreCase(parsed.name, name))
					return parsed.arguments;
			}
			return std::nullopt;
		}

		bool HasMetadata(
			const std::vector<std::string>& metadata,
			const std::string& name)
		{
			return FindMetadataArguments(metadata, name).has_value();
		}

		std::string Unquote(std::string text)
		{
			text = Trim(std::move(text));
			if (text.size() < 2 ||
				(text.front() != '"' && text.front() != '\'') ||
				text.back() != text.front())
			{
				return text;
			}

			const char quote = text.front();
			std::string result;
			result.reserve(text.size() - 2);
			bool escaped = false;
			for (std::size_t index = 1; index + 1 < text.size(); ++index)
			{
				const char character = text[index];
				if (escaped)
				{
					switch (character)
					{
						case 'n': result.push_back('\n'); break;
						case 't': result.push_back('\t'); break;
						case '\\': result.push_back('\\'); break;
						default:
							if (character == quote)
								result.push_back(quote);
							else
								result.push_back(character);
							break;
					}
					escaped = false;
				}
				else if (character == '\\')
				{
					escaped = true;
				}
				else
				{
					result.push_back(character);
				}
			}
			if (escaped)
				result.push_back('\\');
			return result;
		}

		std::optional<PropertyRange> ParseRange(
			const std::vector<std::string>& metadata)
		{
			const std::optional<std::string> arguments =
				FindMetadataArguments(metadata, "Range");
			if (!arguments)
				return std::nullopt;

			const std::size_t separator = arguments->find(',');
			if (separator == std::string::npos)
				return std::nullopt;

			const std::string minimumText =
				Trim(arguments->substr(0, separator));
			const std::string maximumText =
				Trim(arguments->substr(separator + 1));
			char* minimumEnd = nullptr;
			char* maximumEnd = nullptr;
			const double minimum =
				std::strtod(minimumText.c_str(), &minimumEnd);
			const double maximum =
				std::strtod(maximumText.c_str(), &maximumEnd);
			if (!minimumEnd || *minimumEnd != '\0' ||
				!maximumEnd || *maximumEnd != '\0' ||
				minimum > maximum)
			{
				return std::nullopt;
			}

			return PropertyRange{minimum, maximum};
		}

		std::string QualifiedTypeName(const asITypeInfo& type)
		{
			const char* nameSpace = type.GetNamespace();
			if (nameSpace && *nameSpace)
				return std::string(nameSpace) + "::" + type.GetName();
			return type.GetName();
		}

		bool HasDefaultFactory(const asITypeInfo& type)
		{
			for (asUINT index = 0; index < type.GetFactoryCount(); ++index)
			{
				const asIScriptFunction* factory =
					type.GetFactoryByIndex(index);
				if (factory && factory->GetParamCount() == 0)
					return true;
			}
			return false;
		}

		PropertyKind ToPropertyKind(
			asIScriptEngine& engine, int typeId)
		{
			switch (typeId)
			{
				case asTYPEID_BOOL: return PropertyKind::Boolean;
				case asTYPEID_INT8: return PropertyKind::Int8;
				case asTYPEID_INT16: return PropertyKind::Int16;
				case asTYPEID_INT32: return PropertyKind::Int32;
				case asTYPEID_INT64: return PropertyKind::Int64;
				case asTYPEID_UINT8: return PropertyKind::UInt8;
				case asTYPEID_UINT16: return PropertyKind::UInt16;
				case asTYPEID_UINT32: return PropertyKind::UInt32;
				case asTYPEID_UINT64: return PropertyKind::UInt64;
				case asTYPEID_FLOAT: return PropertyKind::Float;
				case asTYPEID_DOUBLE: return PropertyKind::Double;
				default: break;
			}

			if ((typeId & asTYPEID_OBJHANDLE) != 0)
				return PropertyKind::Unsupported;

			const asITypeInfo* type = engine.GetTypeInfoById(typeId);
			if (!type)
				return PropertyKind::Unsupported;
			if ((type->GetFlags() & asOBJ_ENUM) != 0)
				return PropertyKind::Enumeration;
			if (std::string(type->GetName()) == "string")
				return PropertyKind::String;
			return PropertyKind::Unsupported;
		}

		bool ReadScriptProperty(
			const void* rawObject,
			asUINT propertyIndex,
			PropertyKind kind,
			PropertyValue& value)
		{
			if (!rawObject)
				return false;
			auto* object = const_cast<asIScriptObject*>(
				static_cast<const asIScriptObject*>(rawObject));
			void* address = object->GetAddressOfProperty(propertyIndex);
			if (!address)
				return false;

			switch (kind)
			{
				case PropertyKind::Boolean:
					value = *static_cast<bool*>(address); return true;
				case PropertyKind::Int8:
					value = static_cast<std::int64_t>(
						*static_cast<std::int8_t*>(address)); return true;
				case PropertyKind::Int16:
					value = static_cast<std::int64_t>(
						*static_cast<std::int16_t*>(address)); return true;
				case PropertyKind::Int32:
				case PropertyKind::Enumeration:
					value = static_cast<std::int64_t>(
						*static_cast<std::int32_t*>(address)); return true;
				case PropertyKind::Int64:
					value = *static_cast<std::int64_t*>(address); return true;
				case PropertyKind::UInt8:
					value = static_cast<std::uint64_t>(
						*static_cast<std::uint8_t*>(address)); return true;
				case PropertyKind::UInt16:
					value = static_cast<std::uint64_t>(
						*static_cast<std::uint16_t*>(address)); return true;
				case PropertyKind::UInt32:
					value = static_cast<std::uint64_t>(
						*static_cast<std::uint32_t*>(address)); return true;
				case PropertyKind::UInt64:
					value = *static_cast<std::uint64_t*>(address); return true;
				case PropertyKind::Float:
					value = static_cast<double>(
						*static_cast<float*>(address)); return true;
				case PropertyKind::Double:
					value = *static_cast<double*>(address); return true;
				case PropertyKind::String:
					value = *static_cast<std::string*>(address); return true;
				default:
					return false;
			}
		}

		bool WriteScriptProperty(
			void* rawObject,
			asUINT propertyIndex,
			PropertyKind kind,
			const PropertyValue& value)
		{
			if (!rawObject)
				return false;
			auto* object = static_cast<asIScriptObject*>(rawObject);
			void* address = object->GetAddressOfProperty(propertyIndex);
			if (!address)
				return false;

			switch (kind)
			{
				case PropertyKind::Boolean:
					*static_cast<bool*>(address) = std::get<bool>(value);
					return true;
				case PropertyKind::Int8:
					*static_cast<std::int8_t*>(address) =
						static_cast<std::int8_t>(
							std::get<std::int64_t>(value));
					return true;
				case PropertyKind::Int16:
					*static_cast<std::int16_t*>(address) =
						static_cast<std::int16_t>(
							std::get<std::int64_t>(value));
					return true;
				case PropertyKind::Int32:
				case PropertyKind::Enumeration:
					*static_cast<std::int32_t*>(address) =
						static_cast<std::int32_t>(
							std::get<std::int64_t>(value));
					return true;
				case PropertyKind::Int64:
					*static_cast<std::int64_t*>(address) =
						std::get<std::int64_t>(value);
					return true;
				case PropertyKind::UInt8:
					*static_cast<std::uint8_t*>(address) =
						static_cast<std::uint8_t>(
							std::get<std::uint64_t>(value));
					return true;
				case PropertyKind::UInt16:
					*static_cast<std::uint16_t*>(address) =
						static_cast<std::uint16_t>(
							std::get<std::uint64_t>(value));
					return true;
				case PropertyKind::UInt32:
					*static_cast<std::uint32_t*>(address) =
						static_cast<std::uint32_t>(
							std::get<std::uint64_t>(value));
					return true;
				case PropertyKind::UInt64:
					*static_cast<std::uint64_t*>(address) =
						std::get<std::uint64_t>(value);
					return true;
				case PropertyKind::Float:
					*static_cast<float*>(address) =
						static_cast<float>(std::get<double>(value));
					return true;
				case PropertyKind::Double:
					*static_cast<double*>(address) =
						std::get<double>(value);
					return true;
				case PropertyKind::String:
					*static_cast<std::string*>(address) =
						std::get<std::string>(value);
					return true;
				default:
					return false;
			}
		}
	}

	struct ScriptRuntime::Impl
	{
		struct FileStamp
		{
			std::filesystem::file_time_type modified;
			std::uintmax_t size = 0;

			bool operator==(const FileStamp&) const = default;
		};

		using Snapshot = std::map<std::filesystem::path, FileStamp>;

		struct GlobalHooks
		{
			asIScriptFunction* start = nullptr;
			asIScriptFunction* update = nullptr;
			asIScriptFunction* stop = nullptr;
		};

		struct ClassBinding
		{
			asITypeInfo* type = nullptr;
			asIScriptFunction* start = nullptr;
			asIScriptFunction* update = nullptr;
			asIScriptFunction* stop = nullptr;
			asIScriptFunction* enable = nullptr;
			asIScriptFunction* disable = nullptr;
			asIScriptFunction* beforeReload = nullptr;
			asIScriptFunction* afterReload = nullptr;
		};

		struct Candidate
		{
			std::string moduleName;
			asIScriptModule* module = nullptr;
			GlobalHooks globalHooks;
			std::unordered_map<std::string, ClassBinding> classes;
			std::vector<TypeDescriptor> descriptors;
		};

		struct ScriptInstance
		{
			ScriptInstanceHandle handle = 0;
			std::string className;
			asIScriptObject* object = nullptr;
			PropertyBag state;
			bool running = false;
			bool enabled = true;
			bool executionFaulted = false;
		};

		struct BuildInput
		{
			std::map<std::string, std::string> sources;
		};

		asIScriptEngine* engine = nullptr;
		asIScriptContext* context = nullptr;
		std::filesystem::path projectRoot;
		std::filesystem::path scriptRoot;
		std::string activeModuleName;
		GlobalHooks globalHooks;
		std::unordered_map<std::string, ClassBinding> classes;
		std::map<ScriptInstanceHandle, ScriptInstance> instances;
		std::vector<ScriptDiagnostic> diagnostics;
		Snapshot watchedSnapshot;
		Snapshot pendingSnapshot;
		std::chrono::steady_clock::time_point nextScan;
		std::chrono::steady_clock::time_point pendingSince;
		unsigned long long generation = 0;
		ScriptInstanceHandle nextInstanceHandle = 1;
		bool initialized = false;
		bool hotReloadEnabled = true;
		bool playing = false;
		bool paused = false;
		bool executionFaulted = false;
		bool hasPendingSnapshot = false;
		bool lastReloadSuccessful = true;

		static void MessageCallback(
			const asSMessageInfo* message, void* userData)
		{
			auto& runtime = *static_cast<Impl*>(userData);
			ScriptDiagnostic diagnostic;
			diagnostic.file = message->section
				? std::filesystem::path(message->section)
				: std::filesystem::path();
			diagnostic.line = message->row;
			diagnostic.column = message->col;
			diagnostic.severity = ToSeverity(message->type);
			diagnostic.message = message->message
				? message->message
				: "Unknown compiler message";
			runtime.diagnostics.push_back(diagnostic);

			LOG(
				"[AngelScript] %s(%d, %d): %s: %s",
				diagnostic.file.string().c_str(),
				diagnostic.line,
				diagnostic.column,
				ToLabel(diagnostic.severity),
				diagnostic.message.c_str());
		}

		static int IncludeCallback(
			const char* include,
			const char* from,
			CScriptBuilder* builder,
			void* userData)
		{
			if (!include || !from || !builder || !userData)
				return -1;

			auto& input = *static_cast<BuildInput*>(userData);
			const std::filesystem::path requested(include);
			if (requested.is_absolute())
				return -1;

			const std::filesystem::path resolved =
				(std::filesystem::path(from).parent_path() / requested)
					.lexically_normal();
			const std::string key = resolved.generic_string();
			if (key.starts_with("../") || key == "..")
				return -1;

			const auto iterator = input.sources.find(key);
			if (iterator == input.sources.end())
				return -1;

			return builder->AddSectionFromMemory(
				iterator->first.c_str(),
				iterator->second.data(),
				static_cast<unsigned int>(iterator->second.size()));
		}

		bool RegisterApi()
		{
			RegisterStdString(engine);

			const int result = engine->RegisterGlobalFunction(
				"void Log(const string &in message)",
				asFUNCTION(ScriptLog),
				asCALL_CDECL);
			if (result < 0)
			{
				LOG(
					"AngelScript API registration failed for Log: %d",
					result);
				return false;
			}
			return true;
		}

		Snapshot ScanScripts() const
		{
			Snapshot snapshot;
			std::error_code error;
			if (!std::filesystem::is_directory(scriptRoot, error))
				return snapshot;

			std::filesystem::recursive_directory_iterator iterator(
				scriptRoot,
				std::filesystem::directory_options::skip_permission_denied,
				error);
			const std::filesystem::recursive_directory_iterator end;
			while (iterator != end)
			{
				if (!error && iterator->is_regular_file(error))
				{
					std::string extension =
						iterator->path().extension().string();
					std::transform(
						extension.begin(), extension.end(),
						extension.begin(),
						[](unsigned char character)
						{
							return static_cast<char>(
								std::tolower(character));
						});
					if (extension == ".as")
					{
						FileStamp stamp;
						stamp.size = iterator->file_size(error);
						if (!error)
							stamp.modified =
								iterator->last_write_time(error);
						if (!error)
						{
							snapshot.emplace(
								iterator->path().lexically_normal(),
								stamp);
						}
					}
				}

				error.clear();
				iterator.increment(error);
			}
			return snapshot;
		}

		bool Invoke(
			asIScriptFunction* function,
			void* object = nullptr,
			std::optional<float> deltaTime = std::nullopt)
		{
			if (!function)
				return true;

			if (context->Prepare(function) < 0)
			{
				LOG(
					"[AngelScript] Could not prepare %s",
					function->GetDeclaration());
				return false;
			}
			if (object && context->SetObject(object) < 0)
			{
				context->Unprepare();
				return false;
			}
			if (deltaTime && context->SetArgFloat(0, *deltaTime) < 0)
			{
				context->Unprepare();
				return false;
			}

			const int result = context->Execute();
			if (result == asEXECUTION_FINISHED)
			{
				context->Unprepare();
				return true;
			}

			if (result == asEXECUTION_EXCEPTION)
			{
				const asIScriptFunction* exceptionFunction =
					context->GetExceptionFunction();
				const char* section = nullptr;
				int column = 0;
				const int line =
					context->GetLineNumber(0, &column, &section);
				LOG(
					"[AngelScript] Exception in %s at %s(%d, %d): %s",
					exceptionFunction
						? exceptionFunction->GetDeclaration()
						: "<unknown>",
					section ? section : "<script>",
					line,
					column,
					context->GetExceptionString());
			}
			else
			{
				LOG(
					"[AngelScript] Execution of %s ended with code %d",
					function->GetDeclaration(),
					result);
			}

			context->Unprepare();
			return false;
		}

		void ReleaseInstanceObject(ScriptInstance& instance)
		{
			if (!instance.object)
				return;
			const auto binding = classes.find(instance.className);
			if (binding != classes.end())
			{
				engine->ReleaseScriptObject(
					instance.object, binding->second.type);
			}
			instance.object = nullptr;
		}

		bool Instantiate(
			ScriptInstance& instance,
			bool afterReload,
			bool startIfRunning)
		{
			const auto binding = classes.find(instance.className);
			if (binding == classes.end() || !binding->second.type)
				return false;

			instance.object = static_cast<asIScriptObject*>(
				engine->CreateScriptObject(binding->second.type));
			if (!instance.object)
			{
				LOG(
					"Could not create AngelScript class [%s]",
					instance.className.c_str());
				return false;
			}

			const auto descriptor = TypeRegistry::Get().Find(
				ScriptReflectionDomain, instance.className);
			if (descriptor)
				ApplyProperties(
					*descriptor, instance.object, instance.state, false);

			instance.executionFaulted = false;
			if (afterReload &&
				!Invoke(
					binding->second.afterReload, instance.object))
			{
				instance.executionFaulted = true;
			}
			else if (startIfRunning && instance.running &&
				!Invoke(binding->second.start, instance.object))
			{
				instance.executionFaulted = true;
			}
			if (instance.enabled &&
				!Invoke(binding->second.enable, instance.object))
			{
				instance.executionFaulted = true;
			}
			return true;
		}

		PropertyDescriptor BuildPropertyDescriptor(
			CScriptBuilder& builder,
			asITypeInfo& type,
			asUINT propertyIndex)
		{
			const char* name = nullptr;
			int typeId = asTYPEID_VOID;
			bool isPrivate = false;
			bool isProtected = false;
			bool isReference = false;
			bool isConst = false;
			type.GetProperty(
				propertyIndex,
				&name,
				&typeId,
				&isPrivate,
				&isProtected,
				nullptr,
				&isReference,
				nullptr,
				nullptr,
				nullptr,
				&isConst);

			const std::vector<std::string> metadata =
				builder.GetMetadataForTypeProperty(
					type.GetTypeId(),
					static_cast<int>(propertyIndex));

			PropertyDescriptor descriptor;
			descriptor.name = name ? name : "Property";
			descriptor.displayName =
				HumanizeIdentifier(descriptor.name);
			const char* typeDeclaration =
				engine->GetTypeDeclaration(typeId, true);
			descriptor.typeName =
				typeDeclaration ? typeDeclaration : "unknown";
			descriptor.kind =
				isReference
					? PropertyKind::Unsupported
					: ToPropertyKind(*engine, typeId);
			if (descriptor.kind == PropertyKind::Unsupported &&
				descriptor.typeName == "string")
			{
				descriptor.kind = PropertyKind::String;
			}
			const bool explicitlySerialized =
				HasMetadata(metadata, "SerializeField");
			descriptor.attributes.serialized =
				(!isPrivate && !isProtected) || explicitlySerialized;
			descriptor.attributes.visible =
				descriptor.attributes.serialized &&
				!HasMetadata(metadata, "HideInInspector");
			descriptor.attributes.readOnly = isConst;
			descriptor.attributes.range = ParseRange(metadata);
			if (const auto header =
				FindMetadataArguments(metadata, "Header"))
			{
				descriptor.attributes.header = Unquote(*header);
			}

			if (descriptor.kind != PropertyKind::Unsupported)
			{
				const PropertyKind kind = descriptor.kind;
				descriptor.reader = [propertyIndex, kind](
					const void* object, PropertyValue& value)
				{
					return ReadScriptProperty(
						object, propertyIndex, kind, value);
				};
				descriptor.writer = [propertyIndex, kind](
					void* object, const PropertyValue& value)
				{
					return WriteScriptProperty(
						object, propertyIndex, kind, value);
				};
			}
			return descriptor;
		}

		void DiscoverClasses(
			CScriptBuilder& builder, Candidate& candidate)
		{
			for (asUINT index = 0;
				index < candidate.module->GetObjectTypeCount();
				++index)
			{
				asITypeInfo* type =
					candidate.module->GetObjectTypeByIndex(index);
				if (!type ||
					(type->GetFlags() & asOBJ_SCRIPT_OBJECT) == 0 ||
					(type->GetFlags() & asOBJ_ABSTRACT) != 0 ||
					!HasDefaultFactory(*type))
				{
					continue;
				}

				ClassBinding binding;
				binding.type = type;
				binding.start =
					type->GetMethodByDecl("void OnStart()");
				binding.update =
					type->GetMethodByDecl("void OnUpdate(float)");
				binding.stop =
					type->GetMethodByDecl("void OnStop()");
				binding.enable =
					type->GetMethodByDecl("void OnEnable()");
				binding.disable =
					type->GetMethodByDecl("void OnDisable()");
				binding.beforeReload =
					type->GetMethodByDecl("void OnBeforeReload()");
				binding.afterReload =
					type->GetMethodByDecl("void OnAfterReload()");

				const std::vector<std::string> classMetadata =
					builder.GetMetadataForType(type->GetTypeId());
				const bool hasLifecycle =
					binding.start || binding.update || binding.stop ||
					binding.enable || binding.disable ||
					binding.beforeReload || binding.afterReload;
				if (!hasLifecycle &&
					!HasMetadata(classMetadata, "ScriptComponent"))
				{
					continue;
				}
				if (HasMetadata(classMetadata, "HideInInspector"))
					continue;

				const std::string qualifiedName =
					QualifiedTypeName(*type);
				TypeDescriptor descriptor;
				descriptor.domain = ScriptReflectionDomain;
				descriptor.id = qualifiedName;
				descriptor.displayName =
					HumanizeIdentifier(type->GetName());
				for (asUINT property = 0;
					property < type->GetPropertyCount();
					++property)
				{
					descriptor.properties.push_back(
						BuildPropertyDescriptor(
							builder, *type, property));
				}

				candidate.classes.emplace(
					qualifiedName, binding);
				candidate.descriptors.push_back(
					std::move(descriptor));
			}
		}

		bool BuildCandidate(
			const Snapshot& snapshot, Candidate& candidate)
		{
			candidate.moduleName =
				"EGE.ProjectScripts.Candidate." +
				std::to_string(generation + 1);

			BuildInput input;
			for (const auto& [path, stamp] : snapshot)
			{
				(void)stamp;
				const std::string section =
					path.lexically_relative(projectRoot).generic_string();
				std::string source;
				if (!ReadTextFile(path, source))
				{
					LOG(
						"Could not read AngelScript source [%s]",
						path.string().c_str());
					return false;
				}
				input.sources.emplace(section, std::move(source));
			}

			CScriptBuilder builder;
			if (builder.StartNewModule(
					engine, candidate.moduleName.c_str()) < 0)
			{
				LOG("Could not create an AngelScript candidate module");
				return false;
			}
			builder.SetIncludeCallback(
				IncludeCallback, &input);

			for (const auto& [section, source] : input.sources)
			{
				if (builder.AddSectionFromMemory(
						section.c_str(),
						source.data(),
						static_cast<unsigned int>(source.size())) < 0)
				{
					engine->DiscardModule(candidate.moduleName.c_str());
					LOG(
						"Could not add AngelScript source [%s]",
						section.c_str());
					return false;
				}
			}

			if (builder.BuildModule() < 0)
			{
				engine->DiscardModule(candidate.moduleName.c_str());
				return false;
			}

			candidate.module = builder.GetModule();
			candidate.globalHooks.start =
				candidate.module->GetFunctionByDecl("void OnStart()");
			candidate.globalHooks.update =
				candidate.module->GetFunctionByDecl(
					"void OnUpdate(float)");
			candidate.globalHooks.stop =
				candidate.module->GetFunctionByDecl("void OnStop()");
			DiscoverClasses(builder, candidate);
			return true;
		}

		void CaptureInstance(
			ScriptInstance& instance,
			bool serializedOnly)
		{
			if (!instance.object)
				return;
			const auto descriptor = TypeRegistry::Get().Find(
				ScriptReflectionDomain, instance.className);
			if (descriptor)
			{
				instance.state = CaptureProperties(
					*descriptor,
					instance.object,
					serializedOnly);
			}
		}

		bool CommitCandidate(Candidate candidate)
		{
			std::string registryError;
			if (!TypeRegistry::Get().ValidateDomain(
					ScriptReflectionDomain,
					candidate.descriptors,
					registryError))
			{
				engine->DiscardModule(candidate.moduleName.c_str());
				LOG(
					"AngelScript reflection rejected: %s",
					registryError.c_str());
				return false;
			}

			std::unordered_map<ScriptInstanceHandle, bool> wasBound;
			for (auto& [handle, instance] : instances)
			{
				wasBound[handle] = instance.object != nullptr;
				const auto binding = classes.find(instance.className);
				if (instance.object && binding != classes.end())
				{
					Invoke(
						binding->second.beforeReload,
						instance.object);
					CaptureInstance(instance, false);
				}
			}

			if (playing)
				Invoke(globalHooks.stop);
			for (auto& [handle, instance] : instances)
			{
				(void)handle;
				ReleaseInstanceObject(instance);
			}

			if (!activeModuleName.empty())
				engine->DiscardModule(activeModuleName.c_str());

			activeModuleName = candidate.moduleName;
			globalHooks = candidate.globalHooks;
			classes = std::move(candidate.classes);
			TypeRegistry::Get().ReplaceDomain(
				ScriptReflectionDomain,
				std::move(candidate.descriptors),
				registryError);

			for (auto& [handle, instance] : instances)
			{
				const bool reloaded = wasBound[handle];
				Instantiate(instance, reloaded, !reloaded);
			}

			++generation;
			lastReloadSuccessful = true;
			executionFaulted = false;

			LOG(
				"AngelScript generation %llu loaded with %zu "
				"script class(es)",
				generation,
				classes.size());

			if (playing && !Invoke(globalHooks.start))
				executionFaulted = true;
			return true;
		}

		void ClearActiveModule()
		{
			for (auto& [handle, instance] : instances)
			{
				(void)handle;
				if (instance.object)
				CaptureInstance(instance, false);
				ReleaseInstanceObject(instance);
			}
			if (playing)
				Invoke(globalHooks.stop);
			if (!activeModuleName.empty())
				engine->DiscardModule(activeModuleName.c_str());

			activeModuleName.clear();
			globalHooks = {};
			classes.clear();
			TypeRegistry::Get().ClearDomain(
				ScriptReflectionDomain);
			executionFaulted = false;
		}

		bool Reload(const Snapshot& snapshot)
		{
			diagnostics.clear();
			if (snapshot.empty())
			{
				ClearActiveModule();
				++generation;
				lastReloadSuccessful = true;
				LOG(
					"AngelScript project contains no scripts in [%s]",
					scriptRoot.string().c_str());
				return true;
			}

			Candidate candidate;
			if (!BuildCandidate(snapshot, candidate))
			{
				lastReloadSuccessful = false;
				LOG(
					"AngelScript reload rejected; generation %llu "
					"continues running",
					generation);
				return false;
			}

			if (!CommitCandidate(std::move(candidate)))
			{
				lastReloadSuccessful = false;
				return false;
			}
			return true;
		}

		void Poll()
		{
			if (!hotReloadEnabled || scriptRoot.empty())
				return;

			const auto now = std::chrono::steady_clock::now();
			if (now < nextScan)
				return;
			nextScan = now + ScanInterval;

			const Snapshot current = ScanScripts();
			if (current == watchedSnapshot)
			{
				hasPendingSnapshot = false;
				return;
			}

			if (!hasPendingSnapshot || current != pendingSnapshot)
			{
				pendingSnapshot = current;
				pendingSince = now;
				hasPendingSnapshot = true;
				return;
			}

			if (now - pendingSince < ReloadDebounce)
				return;

			Reload(current);
			watchedSnapshot = current;
			hasPendingSnapshot = false;
		}

		void GenerateLanguageServerDefinitions() const
		{
			if (projectRoot.empty() || !engine)
				return;

			std::ostringstream definitions;
			definitions
				<< "// Generated by Edu Game Engine. Do not edit by hand.\n"
				<< "// The AngelScript language server imports this file "
					"for engine API completion.\n\n";

			for (asUINT index = 0;
				index < engine->GetGlobalFunctionCount();
				++index)
			{
				const asIScriptFunction* function =
					engine->GetGlobalFunctionByIndex(index);
				if (function)
				{
					definitions
						<< function->GetDeclaration(true, true, true)
						<< ";\n";
				}
			}

			const std::filesystem::path output =
				projectRoot / "as.predefined";
			if (!WriteIfDifferent(output, definitions.str()))
			{
				LOG(
					"Could not update AngelScript language server API: %s",
					output.string().c_str());
			}
		}
	};

	ScriptRuntime::ScriptRuntime()
		: impl_(std::make_unique<Impl>())
	{
	}

	ScriptRuntime::~ScriptRuntime()
	{
		Shutdown();
	}

	bool ScriptRuntime::Initialize()
	{
		if (impl_->initialized)
			return true;

		impl_->engine = asCreateScriptEngine();
		if (!impl_->engine)
		{
			LOG("Could not create the AngelScript engine");
			return false;
		}

		if (impl_->engine->SetMessageCallback(
				asFUNCTION(Impl::MessageCallback),
				impl_.get(),
				asCALL_CDECL) < 0 ||
			!impl_->RegisterApi())
		{
			Shutdown();
			return false;
		}

		impl_->context = impl_->engine->CreateContext();
		if (!impl_->context)
		{
			LOG("Could not create the AngelScript execution context");
			Shutdown();
			return false;
		}

		impl_->initialized = true;
		LOG("AngelScript scripting runtime initialized");
		return true;
	}

	void ScriptRuntime::Shutdown()
	{
		if (!impl_)
			return;

		if (impl_->playing)
			LeavePlayMode();
		if (impl_->engine)
			impl_->ClearActiveModule();
		impl_->instances.clear();
		if (impl_->context)
		{
			impl_->context->Release();
			impl_->context = nullptr;
		}
		if (impl_->engine)
		{
			impl_->engine->ShutDownAndRelease();
			impl_->engine = nullptr;
		}

		TypeRegistry::Get().ClearDomain(
			ScriptReflectionDomain);
		impl_->initialized = false;
		impl_->projectRoot.clear();
		impl_->scriptRoot.clear();
		impl_->watchedSnapshot.clear();
		impl_->pendingSnapshot.clear();
		impl_->hasPendingSnapshot = false;
	}

	bool ScriptRuntime::SetProjectRoot(
		const std::filesystem::path& projectRoot)
	{
		if (!impl_->initialized)
			return false;

		std::error_code error;
		const std::filesystem::path normalized =
			std::filesystem::absolute(projectRoot, error).lexically_normal();
		if (error)
			return false;
		if (normalized == impl_->projectRoot)
			return true;

		const std::filesystem::path newScriptRoot =
			normalized / "Assets";
		std::filesystem::create_directories(
			newScriptRoot / "Scripts", error);
		if (error)
		{
			LOG(
				"Could not create the project script directory [%s]: %s",
				newScriptRoot.string().c_str(),
				error.message().c_str());
			return false;
		}

		impl_->projectRoot = normalized;
		impl_->scriptRoot = newScriptRoot;
		impl_->GenerateLanguageServerDefinitions();
		impl_->watchedSnapshot = impl_->ScanScripts();
		impl_->pendingSnapshot.clear();
		impl_->hasPendingSnapshot = false;
		impl_->nextScan =
			std::chrono::steady_clock::now() + ScanInterval;
		impl_->Reload(impl_->watchedSnapshot);
		return true;
	}

	void ScriptRuntime::SetHotReloadEnabled(bool enabled)
	{
		impl_->hotReloadEnabled = enabled;
		impl_->hasPendingSnapshot = false;
		impl_->nextScan = std::chrono::steady_clock::now();
	}

	bool ScriptRuntime::IsHotReloadEnabled() const
	{
		return impl_->hotReloadEnabled;
	}

	void ScriptRuntime::Tick(float deltaTime)
	{
		impl_->Poll();
		if (impl_->playing && !impl_->paused &&
			!impl_->executionFaulted &&
			!impl_->Invoke(
				impl_->globalHooks.update,
				nullptr,
				deltaTime))
		{
			impl_->executionFaulted = true;
		}
	}

	void ScriptRuntime::EnterPlayMode()
	{
		if (!impl_->initialized || impl_->playing)
			return;
		impl_->playing = true;
		impl_->paused = false;
		impl_->executionFaulted = false;
		if (!impl_->Invoke(impl_->globalHooks.start))
			impl_->executionFaulted = true;
	}

	void ScriptRuntime::PausePlayMode()
	{
		if (impl_->playing)
			impl_->paused = true;
	}

	void ScriptRuntime::ResumePlayMode()
	{
		if (impl_->playing)
			impl_->paused = false;
	}

	void ScriptRuntime::LeavePlayMode()
	{
		if (!impl_->playing)
			return;
		impl_->Invoke(impl_->globalHooks.stop);
		impl_->playing = false;
		impl_->paused = false;
		impl_->executionFaulted = false;
	}

	bool ScriptRuntime::ForceReload()
	{
		if (!impl_->initialized || impl_->scriptRoot.empty())
			return false;

		const Impl::Snapshot current = impl_->ScanScripts();
		const bool result = impl_->Reload(current);
		impl_->watchedSnapshot = current;
		impl_->hasPendingSnapshot = false;
		return result;
	}

	std::vector<ScriptClassInfo>
		ScriptRuntime::GetAvailableClasses() const
	{
		std::vector<ScriptClassInfo> result;
		const auto descriptors = TypeRegistry::Get().GetDomain(
			ScriptReflectionDomain);
		result.reserve(descriptors.size());
		for (const auto& descriptor : descriptors)
			result.push_back(
				{descriptor->id, descriptor->displayName});
		return result;
	}

	bool ScriptRuntime::HasClass(const std::string& className) const
	{
		return impl_->classes.contains(className);
	}

	ScriptInstanceHandle ScriptRuntime::CreateInstance(
		const std::string& className,
		PropertyBag initialState)
	{
		if (!impl_->initialized)
			return 0;

		const ScriptInstanceHandle handle =
			impl_->nextInstanceHandle++;
		Impl::ScriptInstance instance;
		instance.handle = handle;
		instance.className = className;
		instance.state = std::move(initialState);
		auto [iterator, inserted] =
			impl_->instances.emplace(handle, std::move(instance));
		if (!inserted)
			return 0;
		if (!className.empty())
			impl_->Instantiate(iterator->second, false, false);
		return handle;
	}

	bool ScriptRuntime::SetInstanceClass(
		ScriptInstanceHandle handle,
		const std::string& className,
		PropertyBag initialState)
	{
		const auto iterator = impl_->instances.find(handle);
		if (iterator == impl_->instances.end())
			return false;

		Impl::ScriptInstance& instance = iterator->second;
		const bool wasRunning = instance.running;
		if (instance.object)
		{
			const auto binding = impl_->classes.find(instance.className);
			if (wasRunning && binding != impl_->classes.end())
				impl_->Invoke(binding->second.stop, instance.object);
			impl_->ReleaseInstanceObject(instance);
		}

		instance.className = className;
		instance.state = std::move(initialState);
		instance.executionFaulted = false;
		if (className.empty())
			return true;
		return impl_->Instantiate(instance, false, wasRunning);
	}

	void ScriptRuntime::DestroyInstance(ScriptInstanceHandle handle)
	{
		const auto iterator = impl_->instances.find(handle);
		if (iterator == impl_->instances.end())
			return;

		Impl::ScriptInstance& instance = iterator->second;
		if (instance.object)
		{
			const auto binding = impl_->classes.find(instance.className);
			if (instance.running && binding != impl_->classes.end())
				impl_->Invoke(binding->second.stop, instance.object);
			impl_->ReleaseInstanceObject(instance);
		}
		impl_->instances.erase(iterator);
	}

	void ScriptRuntime::StartInstance(ScriptInstanceHandle handle)
	{
		const auto iterator = impl_->instances.find(handle);
		if (iterator == impl_->instances.end())
			return;
		Impl::ScriptInstance& instance = iterator->second;
		if (instance.running)
			return;
		instance.running = true;
		instance.executionFaulted = false;
		const auto binding = impl_->classes.find(instance.className);
		if (instance.object && binding != impl_->classes.end() &&
			!impl_->Invoke(binding->second.start, instance.object))
		{
			instance.executionFaulted = true;
		}
	}

	void ScriptRuntime::UpdateInstance(
		ScriptInstanceHandle handle, float deltaTime)
	{
		const auto iterator = impl_->instances.find(handle);
		if (iterator == impl_->instances.end())
			return;
		Impl::ScriptInstance& instance = iterator->second;
		if (!instance.object || !instance.running ||
			!instance.enabled || instance.executionFaulted)
		{
			return;
		}

		const auto binding = impl_->classes.find(instance.className);
		if (binding != impl_->classes.end() &&
			!impl_->Invoke(
				binding->second.update,
				instance.object,
				deltaTime))
		{
			instance.executionFaulted = true;
		}
	}

	void ScriptRuntime::StopInstance(ScriptInstanceHandle handle)
	{
		const auto iterator = impl_->instances.find(handle);
		if (iterator == impl_->instances.end())
			return;
		Impl::ScriptInstance& instance = iterator->second;
		const auto binding = impl_->classes.find(instance.className);
		if (instance.object && instance.running &&
			binding != impl_->classes.end())
		{
			impl_->Invoke(binding->second.stop, instance.object);
		}
		instance.running = false;
		instance.executionFaulted = false;
	}

	void ScriptRuntime::EnableInstance(ScriptInstanceHandle handle)
	{
		const auto iterator = impl_->instances.find(handle);
		if (iterator == impl_->instances.end())
			return;
		Impl::ScriptInstance& instance = iterator->second;
		if (instance.enabled)
			return;
		instance.enabled = true;
		const auto binding = impl_->classes.find(instance.className);
		if (instance.object && binding != impl_->classes.end() &&
			!impl_->Invoke(binding->second.enable, instance.object))
		{
			instance.executionFaulted = true;
		}
	}

	void ScriptRuntime::DisableInstance(ScriptInstanceHandle handle)
	{
		const auto iterator = impl_->instances.find(handle);
		if (iterator == impl_->instances.end())
			return;
		Impl::ScriptInstance& instance = iterator->second;
		if (!instance.enabled)
			return;
		const auto binding = impl_->classes.find(instance.className);
		if (instance.object && binding != impl_->classes.end())
			impl_->Invoke(binding->second.disable, instance.object);
		instance.enabled = false;
	}

	ReflectedScriptObject ScriptRuntime::GetReflectedInstance(
		ScriptInstanceHandle handle) const
	{
		const auto iterator = impl_->instances.find(handle);
		if (iterator == impl_->instances.end() ||
			!iterator->second.object)
		{
			return {};
		}
		return {
			TypeRegistry::Get().Find(
				ScriptReflectionDomain,
				iterator->second.className),
			iterator->second.object};
	}

	PropertyBag ScriptRuntime::CaptureInstanceState(
		ScriptInstanceHandle handle,
		bool serializedOnly) const
	{
		const auto iterator = impl_->instances.find(handle);
		if (iterator == impl_->instances.end())
			return {};
		const Impl::ScriptInstance& instance = iterator->second;
		if (!instance.object)
			return instance.state;

		const auto descriptor = TypeRegistry::Get().Find(
			ScriptReflectionDomain, instance.className);
		if (!descriptor)
			return instance.state;
		return CaptureProperties(
			*descriptor, instance.object, serializedOnly);
	}

	bool ScriptRuntime::IsInstanceBound(
		ScriptInstanceHandle handle) const
	{
		const auto iterator = impl_->instances.find(handle);
		return iterator != impl_->instances.end() &&
			iterator->second.object != nullptr;
	}

	bool ScriptRuntime::HasLoadedScripts() const
	{
		return !impl_->activeModuleName.empty();
	}

	bool ScriptRuntime::WasLastReloadSuccessful() const
	{
		return impl_->lastReloadSuccessful;
	}

	unsigned long long ScriptRuntime::GetGeneration() const
	{
		return impl_->generation;
	}

	const std::filesystem::path& ScriptRuntime::GetScriptRoot() const
	{
		return impl_->scriptRoot;
	}

	const std::vector<ScriptDiagnostic>&
		ScriptRuntime::GetDiagnostics() const
	{
		return impl_->diagnostics;
	}
}
