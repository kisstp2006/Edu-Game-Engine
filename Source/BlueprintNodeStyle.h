#pragma once

#include <imgui.h>
#include <imgui_node_editor.h>

#include <algorithm>
#include <cstdint>
#include <string>

namespace EGE::BlueprintNodeStyle
{
	namespace ed = ax::NodeEditor;

	enum class PinShape
	{
		Circle,
		Diamond,
		Square
	};

	struct Pin
	{
		std::uint64_t id = 0;
		std::string label;
		ImVec4 color = ImVec4(1, 1, 1, 1);
		PinShape shape = PinShape::Circle;
		bool connected = false;
	};

	inline ImVec4 WithAlpha(ImVec4 color, float alpha)
	{
		color.w *= alpha;
		return color;
	}

	inline ImVec4 ScaleRgb(ImVec4 color, float scale)
	{
		color.x = std::clamp(color.x * scale, 0.0f, 1.0f);
		color.y = std::clamp(color.y * scale, 0.0f, 1.0f);
		color.z = std::clamp(color.z * scale, 0.0f, 1.0f);
		return color;
	}

	inline ImVec2 Add(const ImVec2& left, const ImVec2& right)
	{
		return ImVec2(left.x + right.x, left.y + right.y);
	}

	inline ImVec2 Subtract(const ImVec2& left, const ImVec2& right)
	{
		return ImVec2(left.x - right.x, left.y - right.y);
	}

	inline void Apply()
	{
		ed::Style& style = ed::GetStyle();
		style.NodePadding = ImVec4(0, 0, 0, 0);
		style.NodeRounding = 8.0f;
		style.NodeBorderWidth = 1.0f;
		style.HoveredNodeBorderWidth = 2.0f;
		style.SelectedNodeBorderWidth = 3.0f;
		style.PinRounding = 4.0f;
		style.PinBorderWidth = 0.0f;
		style.LinkStrength = 110.0f;
		style.SourceDirection = ImVec2(1.0f, 0.0f);
		style.TargetDirection = ImVec2(-1.0f, 0.0f);
		style.ScrollDuration = 0.25f;
		style.Colors[ed::StyleColor_Bg] =
			ImVec4(0.105f, 0.11f, 0.14f, 1.0f);
		style.Colors[ed::StyleColor_Grid] =
			ImVec4(0.31f, 0.32f, 0.37f, 0.32f);
		style.Colors[ed::StyleColor_NodeBg] =
			ImVec4(0.095f, 0.10f, 0.12f, 0.98f);
		style.Colors[ed::StyleColor_NodeBorder] =
			ImVec4(0.43f, 0.44f, 0.49f, 0.78f);
		style.Colors[ed::StyleColor_HovNodeBorder] =
			ImVec4(0.25f, 0.68f, 0.95f, 1.0f);
		style.Colors[ed::StyleColor_SelNodeBorder] =
			ImVec4(0.18f, 0.70f, 1.0f, 1.0f);
		style.Colors[ed::StyleColor_HovLinkBorder] =
			ImVec4(0.85f, 0.93f, 1.0f, 1.0f);
		style.Colors[ed::StyleColor_SelLinkBorder] =
			ImVec4(1.0f, 0.74f, 0.24f, 1.0f);
		style.Colors[ed::StyleColor_NodeSelRect] =
			ImVec4(0.15f, 0.55f, 0.9f, 0.18f);
		style.Colors[ed::StyleColor_NodeSelRectBorder] =
			ImVec4(0.20f, 0.68f, 1.0f, 0.72f);
	}

	inline void DrawPinIcon(
		const ImVec2& minimum,
		const ImVec2& maximum,
		const Pin& pin)
	{
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		const ImVec2 center(
			(minimum.x + maximum.x) * 0.5f,
			(minimum.y + maximum.y) * 0.5f);
		const float radius =
			std::min(maximum.x - minimum.x, maximum.y - minimum.y) *
			0.38f;
		const ImU32 outline =
			ImGui::ColorConvertFloat4ToU32(WithAlpha(pin.color, 0.95f));
		const ImU32 fill =
			ImGui::ColorConvertFloat4ToU32(
				pin.connected
					? WithAlpha(pin.color, 0.95f)
					: ImVec4(0.075f, 0.08f, 0.10f, 1.0f));
		const ImU32 shadow = IM_COL32(0, 0, 0, 190);

		if (pin.shape == PinShape::Circle)
		{
			drawList->AddCircleFilled(center, radius + 2.0f, shadow, 16);
			drawList->AddCircleFilled(center, radius, fill, 16);
			drawList->AddCircle(center, radius, outline, 16, 1.7f);
			if (pin.connected)
				drawList->AddCircleFilled(
					center, radius * 0.38f, IM_COL32(245, 250, 255, 225), 12);
			return;
		}

		if (pin.shape == PinShape::Diamond)
		{
			const ImVec2 top(center.x, center.y - radius);
			const ImVec2 right(center.x + radius, center.y);
			const ImVec2 bottom(center.x, center.y + radius);
			const ImVec2 left(center.x - radius, center.y);
			drawList->AddQuadFilled(top, right, bottom, left, shadow);
			const float inset = pin.connected ? 0.0f : 1.8f;
			drawList->AddQuadFilled(
				ImVec2(top.x, top.y + inset),
				ImVec2(right.x - inset, right.y),
				ImVec2(bottom.x, bottom.y - inset),
				ImVec2(left.x + inset, left.y),
				fill);
			drawList->AddLine(top, right, outline, 1.7f);
			drawList->AddLine(right, bottom, outline, 1.7f);
			drawList->AddLine(bottom, left, outline, 1.7f);
			drawList->AddLine(left, top, outline, 1.7f);
			return;
		}

		const ImVec2 squareMin(center.x - radius, center.y - radius);
		const ImVec2 squareMax(center.x + radius, center.y + radius);
		drawList->AddRectFilled(
			Subtract(squareMin, ImVec2(2, 2)),
			Add(squareMax, ImVec2(2, 2)),
			shadow,
			2.0f);
		drawList->AddRectFilled(squareMin, squareMax, fill, 2.0f);
		drawList->AddRect(squareMin, squareMax, outline, 2.0f, 15, 1.7f);
	}

	class NodeBuilder
	{
	public:
		NodeBuilder(
			std::uint64_t nodeId,
			std::string title,
			ImVec4 headerColor,
			float width = 220.0f)
			: nodeId(nodeId),
			  title(std::move(title)),
			  headerColor(headerColor),
			  width(std::max(width, 150.0f))
		{
		}

		void Begin()
		{
			ed::PushStyleVar(ed::StyleVar_NodePadding, ImVec4(0, 0, 0, 0));
			ed::BeginNode(ed::NodeId(nodeId));
			ImGui::PushID(
				reinterpret_cast<void*>(
					static_cast<std::uintptr_t>(nodeId)));
			ImGui::BeginGroup();

			nodeMinimum = ImGui::GetCursorScreenPos();
			headerMinimum = nodeMinimum;
			ImGui::Dummy(ImVec2(width, HeaderHeight));
			headerMaximum = ImGui::GetItemRectMax();

			const ImVec2 titleSize = ImGui::CalcTextSize(title.c_str());
			ImGui::GetWindowDrawList()->AddText(
				ImVec2(
					headerMinimum.x + HorizontalPadding,
					headerMinimum.y +
						(HeaderHeight - titleSize.y) * 0.5f),
				ImGui::GetColorU32(ImGuiCol_Text),
				title.c_str());

			contentMinimum = ImVec2(
				nodeMinimum.x + HorizontalPadding,
				headerMaximum.y + VerticalPadding);
			ImGui::SetCursorScreenPos(contentMinimum);
			ImGui::BeginGroup();
		}

		void PinRow(const Pin* input, const Pin* output)
		{
			const ImVec2 rowMinimum = ImGui::GetCursorScreenPos();
			float rowBottom = rowMinimum.y + RowHeight;

			if (input)
			{
				ImGui::SetCursorScreenPos(rowMinimum);
				rowBottom = std::max(
					rowBottom,
					DrawPin(*input, ed::PinKind::Input, false));
			}

			if (output)
			{
				const float labelWidth =
					ImGui::CalcTextSize(output->label.c_str()).x;
				const float pinWidth =
					labelWidth + PinSize + PinLabelSpacing;
				ImGui::SetCursorScreenPos(
					ImVec2(
						contentMinimum.x + ContentWidth() - pinWidth,
						rowMinimum.y));
				rowBottom = std::max(
					rowBottom,
					DrawPin(*output, ed::PinKind::Output, true));
			}

			ImGui::SetCursorScreenPos(
				ImVec2(
					contentMinimum.x,
					rowBottom + ImGui::GetStyle().ItemSpacing.y));
		}

		void Text(const char* value, const ImVec4& color)
		{
			ImGui::PushStyleColor(ImGuiCol_Text, color);
			ImGui::TextUnformatted(value);
			ImGui::PopStyleColor();
		}

		void Text(const char* value)
		{
			ImGui::TextUnformatted(value);
		}

		float ContentWidth() const
		{
			return width - HorizontalPadding * 2.0f;
		}

		void End()
		{
			ImGui::EndGroup();
			ImVec2 contentMaximum = ImGui::GetItemRectMax();
			contentMaximum.y = std::max(
				contentMaximum.y,
				headerMaximum.y + VerticalPadding);
			ImGui::SetCursorScreenPos(
				ImVec2(
					nodeMinimum.x,
					contentMaximum.y + VerticalPadding));
			ImGui::Dummy(ImVec2(width, 0.0f));
			ImGui::EndGroup();
			ed::EndNode();

			if (ImGui::IsItemVisible())
				DrawHeader();

			ImGui::PopID();
			ed::PopStyleVar();
		}

	private:
		float DrawPin(
			const Pin& pin,
			ed::PinKind kind,
			bool output)
		{
			ed::PushStyleVar(
				ed::StyleVar_PivotAlignment,
				output ? ImVec2(1.0f, 0.5f) : ImVec2(0.0f, 0.5f));
			ed::PushStyleVar(ed::StyleVar_PivotSize, ImVec2(0, 0));
			ed::BeginPin(ed::PinId(pin.id), kind);
			ImGui::BeginGroup();

			const float textHeight = ImGui::GetTextLineHeight();
			const float rowHeight = std::max(textHeight, PinSize);
			const ImVec2 rowMinimum = ImGui::GetCursorScreenPos();
			ImVec2 iconMinimum;

			if (!output)
			{
				iconMinimum = ImVec2(
					rowMinimum.x,
					rowMinimum.y + (rowHeight - PinSize) * 0.5f);
				ImGui::SetCursorScreenPos(iconMinimum);
				ImGui::Dummy(ImVec2(PinSize, PinSize));
				ImGui::SameLine(0.0f, PinLabelSpacing);
				ImGui::SetCursorPosY(
					ImGui::GetCursorPosY() +
					(rowHeight - textHeight) * 0.5f);
				ImGui::TextUnformatted(pin.label.c_str());
			}
			else
			{
				ImGui::SetCursorPosY(
					ImGui::GetCursorPosY() +
					(rowHeight - textHeight) * 0.5f);
				ImGui::TextUnformatted(pin.label.c_str());
				ImGui::SameLine(0.0f, PinLabelSpacing);
				iconMinimum = ImVec2(
					ImGui::GetCursorScreenPos().x,
					rowMinimum.y + (rowHeight - PinSize) * 0.5f);
				ImGui::SetCursorScreenPos(iconMinimum);
				ImGui::Dummy(ImVec2(PinSize, PinSize));
			}

			const ImVec2 iconMaximum =
				Add(iconMinimum, ImVec2(PinSize, PinSize));
			const ImVec2 iconCenter(
				(iconMinimum.x + iconMaximum.x) * 0.5f,
				(iconMinimum.y + iconMaximum.y) * 0.5f);
			DrawPinIcon(iconMinimum, iconMaximum, pin);

			ImGui::EndGroup();
			ed::PinRect(
				ImGui::GetItemRectMin(),
				ImGui::GetItemRectMax());
			ed::PinPivotRect(iconCenter, iconCenter);
			const float bottom = ImGui::GetItemRectMax().y;
			ed::EndPin();
			ed::PopStyleVar(2);
			return bottom;
		}

		void DrawHeader() const
		{
			ImDrawList* drawList =
				ed::GetNodeBackgroundDrawList(ed::NodeId(nodeId));
			const float border = ed::GetStyle().NodeBorderWidth * 0.5f;
			const ImVec2 minimum =
				Add(headerMinimum, ImVec2(border, border));
			const ImVec2 maximum =
				Subtract(headerMaximum, ImVec2(border, 0.0f));
			const float rounding = ed::GetStyle().NodeRounding;
			const ImVec4 top = ScaleRgb(headerColor, 1.18f);
			const ImVec4 bottom = ScaleRgb(headerColor, 0.52f);
			const ImU32 topColor =
				ImGui::ColorConvertFloat4ToU32(top);
			const ImU32 bottomColor =
				ImGui::ColorConvertFloat4ToU32(bottom);

			drawList->AddRectFilled(
				minimum,
				maximum,
				topColor,
				rounding,
				ImDrawCornerFlags_TopLeft |
					ImDrawCornerFlags_TopRight);

			const float sideInset = rounding * 0.55f;
			drawList->AddRectFilledMultiColor(
				ImVec2(minimum.x + sideInset, minimum.y),
				ImVec2(maximum.x - sideInset, maximum.y),
				topColor,
				topColor,
				bottomColor,
				bottomColor);
			const float roundedStart = minimum.y + sideInset;
			const ImVec4 sideTop = ScaleRgb(headerColor, 0.92f);
			const ImU32 sideTopColor =
				ImGui::ColorConvertFloat4ToU32(sideTop);
			drawList->AddRectFilledMultiColor(
				ImVec2(minimum.x, roundedStart),
				ImVec2(minimum.x + sideInset, maximum.y),
				sideTopColor,
				sideTopColor,
				bottomColor,
				bottomColor);
			drawList->AddRectFilledMultiColor(
				ImVec2(maximum.x - sideInset, roundedStart),
				ImVec2(maximum.x, maximum.y),
				sideTopColor,
				sideTopColor,
				bottomColor,
				bottomColor);
			drawList->AddLine(
				ImVec2(minimum.x, maximum.y - 0.5f),
				ImVec2(maximum.x, maximum.y - 0.5f),
				IM_COL32(255, 255, 255, 42),
				1.0f);
			drawList->AddLine(
				ImVec2(minimum.x + rounding, minimum.y + 1.0f),
				ImVec2(maximum.x - rounding, minimum.y + 1.0f),
				IM_COL32(255, 255, 255, 58),
				1.0f);
		}

		static constexpr float HeaderHeight = 34.0f;
		static constexpr float HorizontalPadding = 10.0f;
		static constexpr float VerticalPadding = 8.0f;
		static constexpr float PinSize = 16.0f;
		static constexpr float PinLabelSpacing = 5.0f;
		static constexpr float RowHeight = 20.0f;

		std::uint64_t nodeId = 0;
		std::string title;
		ImVec4 headerColor;
		float width = 220.0f;
		ImVec2 nodeMinimum{};
		ImVec2 headerMinimum{};
		ImVec2 headerMaximum{};
		ImVec2 contentMinimum{};
	};
}
