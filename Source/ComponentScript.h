#ifndef EGE_COMPONENT_SCRIPT_H
#define EGE_COMPONENT_SCRIPT_H

#include "Component.h"
#include "Reflection/TypeRegistry.h"
#include "Scripting/ScriptRuntime.h"

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
	void OnUpdate(float deltaTime) override;
	void OnStop() override;

	[[nodiscard]] const std::string& GetScriptClass() const;
	void SetScriptClass(const std::string& className);
	[[nodiscard]] EGE::ScriptInstanceHandle GetInstanceHandle() const;
	[[nodiscard]] bool IsBound() const;

private:
	[[nodiscard]] EGE::ScriptRuntime* GetRuntime() const;
	void EnsureInstance();

	std::string className_;
	EGE::PropertyBag storedState_;
	EGE::ScriptInstanceHandle instanceHandle_ = 0;
};

#endif
