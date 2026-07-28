#ifndef EGE_SCRIPT_RUNTIME_H
#define EGE_SCRIPT_RUNTIME_H

#include "../Reflection/TypeRegistry.h"
#include "ScriptApiRegistry.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace EGE
{
	enum class ScriptDiagnosticSeverity
	{
		Information,
		Warning,
		Error
	};

	struct ScriptDiagnostic
	{
		std::filesystem::path file;
		int line = 0;
		int column = 0;
		ScriptDiagnosticSeverity severity =
			ScriptDiagnosticSeverity::Information;
		std::string message;
	};

	using ScriptInstanceHandle = std::uint64_t;

	struct ScriptClassInfo
	{
		std::string name;
		std::string displayName;
		std::string assetId;
		std::filesystem::path sourcePath;
	};

	struct ReflectedScriptObject
	{
		std::shared_ptr<const TypeDescriptor> type;
		void* object = nullptr;

		[[nodiscard]] explicit operator bool() const
		{
			return type && object;
		}
	};

	class ScriptRuntime final
	{
	public:
		ScriptRuntime();
		~ScriptRuntime();

		ScriptRuntime(const ScriptRuntime&) = delete;
		ScriptRuntime& operator=(const ScriptRuntime&) = delete;

		bool Initialize();
		void Shutdown();
		[[nodiscard]] bool RegisterApi(
			std::string name,
			ScriptApiRegistry::Registrar registrar,
			std::string& error);

		bool SetProjectRoot(const std::filesystem::path& projectRoot);
		bool ClearProjectRoot();
		void SetEditorBuild(bool isEditorBuild);
		void SetHotReloadEnabled(bool enabled);
		[[nodiscard]] bool IsHotReloadEnabled() const;

		void Tick(float deltaTime);
		void EnterPlayMode();
		void PausePlayMode();
		void ResumePlayMode();
		void LeavePlayMode();
		bool ForceReload();

		[[nodiscard]] std::vector<ScriptClassInfo>
			GetAvailableClasses() const;
		[[nodiscard]] bool HasClass(const std::string& className) const;
		[[nodiscard]] std::string ResolveClass(
			const std::string& assetId,
			const std::string& classNameFallback = {}) const;
		[[nodiscard]] ScriptInstanceHandle CreateInstance(
			const std::string& className,
			PropertyBag initialState = {});
		bool SetInstanceClass(
			ScriptInstanceHandle handle,
			const std::string& className,
			PropertyBag initialState = {});
		void SetInstanceOwnerId(
			ScriptInstanceHandle handle,
			std::uint32_t ownerId);
		void DestroyInstance(ScriptInstanceHandle handle);
		void StartInstance(ScriptInstanceHandle handle);
		void FixedUpdateInstance(ScriptInstanceHandle handle, float deltaTime);
		void UpdateInstance(ScriptInstanceHandle handle, float deltaTime);
		void LateUpdateInstance(ScriptInstanceHandle handle, float deltaTime);
		void StopInstance(ScriptInstanceHandle handle);
		void EnableInstance(ScriptInstanceHandle handle);
		void DisableInstance(ScriptInstanceHandle handle);
		[[nodiscard]] ReflectedScriptObject GetReflectedInstance(
			ScriptInstanceHandle handle) const;
		[[nodiscard]] PropertyBag CaptureInstanceState(
			ScriptInstanceHandle handle,
			bool serializedOnly) const;
		[[nodiscard]] bool IsInstanceBound(
			ScriptInstanceHandle handle) const;

		[[nodiscard]] bool HasLoadedScripts() const;
		[[nodiscard]] bool WasLastReloadSuccessful() const;
		[[nodiscard]] unsigned long long GetGeneration() const;
		[[nodiscard]] const std::filesystem::path& GetScriptRoot() const;
		[[nodiscard]] const std::vector<ScriptDiagnostic>&
			GetDiagnostics() const;
		void ClearDiagnostics();

	private:
		struct Impl;
		std::unique_ptr<Impl> impl_;
	};
}

#endif
