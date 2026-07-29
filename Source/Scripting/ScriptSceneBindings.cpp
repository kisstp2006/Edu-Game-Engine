#include "ScriptSceneBindings.h"

#include "ScriptMath.h"
#include "ScriptObjectReference.h"

#include "../Application.h"
#include "../GameObject.h"
#include "../ModuleLevelManager.h"

#include <angelscript.h>

namespace EGE
{
	namespace
	{
		void SetScriptException(const std::string& message)
		{
			if (asIScriptContext* context = asGetActiveContext())
				context->SetException(message.c_str());
		}

		GameObject* ResolveParent(
			const ScriptGameObjectReference* reference)
		{
			if (!reference)
				return nullptr;
			GameObject* parent = reference->Resolve();
			if (!parent)
				SetScriptException(
					"The prefab parent reference is no longer valid.");
			return parent;
		}

		bool LoadScene(const std::string& path)
		{
			if (!App || !App->level)
				return false;
			std::string error;
			const bool requested =
				App->level->RequestLoad(path.c_str(), &error);
			if (!requested && !error.empty())
				SetScriptException(error);
			return requested;
		}

		bool ReloadScene()
		{
			if (!App || !App->level)
				return false;
			std::string error;
			const bool requested =
				App->level->RequestReload(&error);
			if (!requested && !error.empty())
				SetScriptException(error);
			return requested;
		}

		std::string GetSceneName()
		{
			return App && App->level
				? App->level->GetSceneName()
				: std::string();
		}

		std::string GetScenePath()
		{
			return App && App->level
				? App->level->GetScenePath().generic_string()
				: std::string();
		}

		std::string GetSceneLastError()
		{
			return App && App->level
				? App->level->GetLastSceneChangeError()
				: std::string();
		}

		bool GetSceneLoadPending()
		{
			return App && App->level &&
				App->level->HasPendingSceneChange();
		}

		ScriptGameObjectReference* InstantiatePrefab(
			const std::string& path,
			ScriptGameObjectReference* parentReference)
		{
			if (!App || !App->level)
				return nullptr;
			GameObject* parent = ResolveParent(parentReference);
			if (parentReference && !parent)
				return nullptr;

			std::string error;
			GameObject* instance = App->level->InstantiatePrefab(
				path.c_str(), parent, &error);
			if (!instance)
			{
				if (!error.empty())
					SetScriptException(error);
				return nullptr;
			}
			return MakeGameObjectReference(instance->GetUID());
		}

		ScriptGameObjectReference* InstantiatePrefabAt(
			const std::string& path,
			const ScriptVector3& position,
			const ScriptQuaternion& rotation,
			ScriptGameObjectReference* parentReference)
		{
			ScriptGameObjectReference* reference =
				InstantiatePrefab(path, parentReference);
			GameObject* instance = reference ? reference->Resolve() : nullptr;
			if (!instance)
				return reference;

			Quat nativeRotation{
				rotation.x, rotation.y, rotation.z, rotation.w};
			if (nativeRotation.LengthSq() <= 0.0000001f)
				nativeRotation = Quat::identity;
			instance->SetGlobalTransform(float4x4::FromTRS(
				{position.x, position.y, position.z},
				nativeRotation.Normalized(),
				instance->GetGlobalScale()));
			return reference;
		}
	}

	bool RegisterSceneApi(
		asIScriptEngine& engine,
		std::string& error)
	{
		engine.SetDefaultNamespace("Scene");
		const bool sceneRegistered =
			engine.RegisterGlobalFunction(
				"bool Load(const string &in path)",
				asFUNCTION(LoadScene), asCALL_CDECL) >= 0 &&
			engine.RegisterGlobalFunction(
				"bool Reload()",
				asFUNCTION(ReloadScene), asCALL_CDECL) >= 0 &&
			engine.RegisterGlobalFunction(
				"string get_name() property",
				asFUNCTION(GetSceneName), asCALL_CDECL) >= 0 &&
			engine.RegisterGlobalFunction(
				"string get_path() property",
				asFUNCTION(GetScenePath), asCALL_CDECL) >= 0 &&
			engine.RegisterGlobalFunction(
				"string get_lastError() property",
				asFUNCTION(GetSceneLastError), asCALL_CDECL) >= 0 &&
			engine.RegisterGlobalFunction(
				"bool get_loadPending() property",
				asFUNCTION(GetSceneLoadPending), asCALL_CDECL) >= 0;
		engine.SetDefaultNamespace("");
		if (!sceneRegistered)
		{
			error = "Could not register the Scene API.";
			return false;
		}

		engine.SetDefaultNamespace("Prefab");
		const bool prefabRegistered =
			engine.RegisterGlobalFunction(
				"GameObject@ Instantiate("
					"const string &in path, "
					"GameObject@+ parent = null)",
				asFUNCTION(InstantiatePrefab), asCALL_CDECL) >= 0 &&
			engine.RegisterGlobalFunction(
				"GameObject@ Instantiate("
					"const string &in path, "
					"const Vector3 &in position, "
					"const Quaternion &in rotation, "
					"GameObject@+ parent = null)",
				asFUNCTION(InstantiatePrefabAt), asCALL_CDECL) >= 0;
		engine.SetDefaultNamespace("");
		if (prefabRegistered)
			return true;

		error = "Could not register the Prefab API.";
		return false;
	}
}
