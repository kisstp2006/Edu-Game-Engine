#ifndef EGE_PANEL_SCRIPT_DIAGNOSTICS_H
#define EGE_PANEL_SCRIPT_DIAGNOSTICS_H

#include "Panel.h"

class PanelScriptDiagnostics final : public Panel
{
public:
	PanelScriptDiagnostics();
	void Draw() override;
};

#endif
