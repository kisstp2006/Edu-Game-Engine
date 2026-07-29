#ifndef EGE_COMPONENT_SCRIPT_H
#define EGE_COMPONENT_SCRIPT_H

#include "Component.h"
#include "Reflection/TypeRegistry.h"
#include "Scripting/ScriptRuntime.h"

#include <map>
#include <string>

class ComponentScript final : public Component
{
public:
	explicit ComponentScript(GameObject* gameObject);
	~ComponentScript() override;

	void OnSave(Config& config) const override;
	void OnLoad(Config* config) override;

	void OnStart() override;
	void OnActivate() override;
	void OnDeActivate() override;
	void OnPlay() override;
	void OnFixedUpdate(float deltaTime) override;
	void OnUpdate(float deltaTime) override;
	void OnLateUpdate(float deltaTime) override;
	void OnCollision(GameObject* other) override;
	void OnStop() override;
	void RemapSerializedReferences(
		const std::map<uint, uint>& gameObjectIds,
		const std::map<uint, uint>& componentIds) override;

	[[nodiscard]] const std::string& GetScriptClass() const;
	[[nodiscard]] const std::string& GetScriptAssetId() const;
	void SetScriptClass(const std::string& className);
	void SetScriptReference(
		const std::string& assetId,
		const std::string& className);
	void RefreshScriptReference();
	[[nodiscard]] EGE::ScriptInstanceHandle GetInstanceHandle() const;
	[[nodiscard]] bool IsBound() const;

private:
	[[nodiscard]] EGE::ScriptRuntime* GetRuntime() const;
	void ResolveScriptReference();
	void EnsureInstance();

	std::string assetId_;
	std::string className_;
	EGE::PropertyBag storedState_;
	EGE::ScriptInstanceHandle instanceHandle_ = 0;
};

#endif
