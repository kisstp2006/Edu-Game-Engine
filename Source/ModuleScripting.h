#ifndef EGE_MODULE_SCRIPTING_H
#define EGE_MODULE_SCRIPTING_H

#include "Globals.h"
#include "Module.h"
#include "Scripting/ScriptRuntime.h"

class ModuleScripting final : public Module
{
public:
	explicit ModuleScripting(bool startEnabled = true);

	bool Init(Config* config = nullptr) override;
	bool Start(Config* config = nullptr) override;
	update_status PreUpdate(float dt) override;
	update_status Update(float dt) override;
	bool CleanUp() override;
	void ReceiveEvent(const Event& event) override;

	void SetHotReloadEnabled(bool enabled);
	bool Reload();

	[[nodiscard]] EGE::ScriptRuntime& GetRuntime();
	[[nodiscard]] const EGE::ScriptRuntime& GetRuntime() const;

private:
	bool SynchronizeProject();

	EGE::ScriptRuntime runtime_;
};

#endif
