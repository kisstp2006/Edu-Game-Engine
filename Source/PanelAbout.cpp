#include "PanelAbout.h"
#include "Globals.h"
#include <imgui.h>

void EGE::DrawAboutSection()
{
	ImGui::Text("Version %s", VERSION);
	ImGui::Separator();
	ImGui::TextUnformatted("Developers");
	ImGui::BulletText("Ricard Pillosu");
	ImGui::BulletText("Carlos Fuentes (ilgenio)");
	ImGui::BulletText("Kiss Tibor");
	ImGui::Spacing();
	ImGui::TextWrapped(
		"EDU Engine is licensed under the Public Domain. "
		"See LICENSE for more information.");
}
