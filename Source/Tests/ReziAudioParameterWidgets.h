#pragma once

#include "../ReziAudioTypes.h"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <cstring>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace EGE::ReziAudio::TestUi
{
	inline std::string AudioClipLabel(const AudioClipReference& clip);

	inline AudioClipReference MakeAudioClipReference(
		const std::filesystem::path& source)
	{
		const std::string key =
			source.lexically_normal().generic_string();
		std::uint64_t hash = 1469598103934665603ULL;
		for (const unsigned char character : key)
		{
			hash ^= character;
			hash *= 1099511628211ULL;
		}
		return {
			hash == 0 ? 1 : hash,
			source.lexically_normal().string()};
	}

	inline std::vector<AudioClipReference> BuildAudioClipCatalog(
		const std::filesystem::path& selectedSource)
	{
		std::vector<AudioClipReference> clips;
		const auto add = [&clips](const std::filesystem::path& source)
		{
			const AudioClipReference clip =
				MakeAudioClipReference(source);
			if (std::find(clips.begin(), clips.end(), clip) == clips.end())
				clips.push_back(clip);
		};
		add(selectedSource);

		std::error_code error;
		const std::filesystem::path directory =
			selectedSource.parent_path();
		if (!directory.empty() &&
			std::filesystem::is_directory(directory, error))
		{
			static constexpr std::array<std::string_view, 5> extensions = {
				".wav", ".ogg", ".mp3", ".flac", ".opus"};
			for (const std::filesystem::directory_entry& entry :
				std::filesystem::directory_iterator(directory, error))
			{
				if (error || !entry.is_regular_file(error))
					continue;
				std::string extension =
					entry.path().extension().string();
				std::transform(
					extension.begin(),
					extension.end(),
					extension.begin(),
					[](unsigned char character)
					{
						return static_cast<char>(
							std::tolower(character));
					});
				if (std::find(
						extensions.begin(),
						extensions.end(),
						extension) != extensions.end())
				{
					add(entry.path());
				}
			}
		}
		std::sort(
			clips.begin(),
			clips.end(),
			[](const AudioClipReference& left,
				const AudioClipReference& right)
			{
				return AudioClipLabel(left) < AudioClipLabel(right);
			});
		return clips;
	}

	inline std::string AudioClipLabel(const AudioClipReference& clip)
	{
		if (!clip.resolvedSource.empty())
		{
			const std::filesystem::path path(clip.resolvedSource);
			if (!path.filename().empty())
				return path.filename().string();
		}
		return clip.assetId != 0
			? "Audio asset " + std::to_string(clip.assetId)
			: "None";
	}

	inline bool DrawAudioClip(
		const char* label,
		AudioClipReference& value,
		std::span<const AudioClipReference> availableClips)
	{
		bool changed = false;
		const std::string preview = AudioClipLabel(value);
		if (ImGui::BeginCombo(label, preview.c_str()))
		{
			if (ImGui::Selectable("None", !value.IsValid()))
			{
				value = {};
				changed = true;
			}
			for (const AudioClipReference& clip : availableClips)
			{
				const std::string clipLabel = AudioClipLabel(clip);
				const bool selected = clip.assetId == value.assetId &&
					clip.resolvedSource == value.resolvedSource;
				ImGui::PushID(
					static_cast<int>(clip.assetId & 0x7fffffffULL));
				if (ImGui::Selectable(clipLabel.c_str(), selected))
				{
					value = clip;
					changed = true;
				}
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip(
						"Asset ID: %llu\n%s",
						clip.assetId,
						clip.resolvedSource.c_str());
				ImGui::PopID();
			}
			ImGui::EndCombo();
		}
		return changed;
	}

	inline bool DrawParameterValue(
		const char* label,
		ParameterValue& value,
		std::span<const AudioClipReference> availableClips = {})
	{
		if (float* number = std::get_if<float>(&value))
			return ImGui::DragFloat(label, number, 0.01f);
		if (int* number = std::get_if<int>(&value))
			return ImGui::DragInt(label, number);
		if (bool* enabled = std::get_if<bool>(&value))
			return ImGui::Checkbox(label, enabled);
		if (float2* vector = std::get_if<float2>(&value))
		{
			float data[2] = {vector->x, vector->y};
			if (!ImGui::DragFloat2(label, data, 0.05f))
				return false;
			*vector = float2(data[0], data[1]);
			return true;
		}
		if (float3* vector = std::get_if<float3>(&value))
		{
			float data[3] = {vector->x, vector->y, vector->z};
			if (!ImGui::DragFloat3(label, data, 0.05f))
				return false;
			*vector = float3(data[0], data[1], data[2]);
			return true;
		}
		if (float4* color = std::get_if<float4>(&value))
		{
			float data[4] = {color->x, color->y, color->z, color->w};
			if (!ImGui::ColorEdit4(label, data))
				return false;
			*color = float4(data[0], data[1], data[2], data[3]);
			return true;
		}
		if (std::string* text = std::get_if<std::string>(&value))
		{
			char buffer[256]{};
			strncpy_s(buffer, text->c_str(), _TRUNCATE);
			if (!ImGui::InputText(label, buffer, sizeof(buffer)))
				return false;
			*text = buffer;
			return true;
		}
		if (AudioClipReference* clip =
				std::get_if<AudioClipReference>(&value))
		{
			return DrawAudioClip(label, *clip, availableClips);
		}
		if (FloatArray* values = std::get_if<FloatArray>(&value))
		{
			bool changed = false;
			if (ImGui::TreeNode(label))
			{
				std::optional<std::size_t> remove;
				for (std::size_t index = 0; index < values->size(); ++index)
				{
					ImGui::PushID(static_cast<int>(index));
					changed |= ImGui::DragFloat(
						"##Value", &(*values)[index], 0.01f);
					ImGui::SameLine();
					if (ImGui::SmallButton("x"))
						remove = index;
					ImGui::PopID();
				}
				if (remove)
				values->erase(values->begin() + *remove);
				if (ImGui::SmallButton("+ Float"))
				{
					values->push_back(0.0f);
					changed = true;
				}
				changed |= remove.has_value();
				ImGui::TreePop();
			}
			return changed;
		}
		if (IntegerArray* values = std::get_if<IntegerArray>(&value))
		{
			bool changed = false;
			if (ImGui::TreeNode(label))
			{
				std::optional<std::size_t> remove;
				for (std::size_t index = 0; index < values->size(); ++index)
				{
					ImGui::PushID(static_cast<int>(index));
					changed |= ImGui::DragInt(
						"##Value", &(*values)[index]);
					ImGui::SameLine();
					if (ImGui::SmallButton("x"))
						remove = index;
					ImGui::PopID();
				}
				if (remove)
					values->erase(values->begin() + *remove);
				if (ImGui::SmallButton("+ Integer"))
				{
					values->push_back(0);
					changed = true;
				}
				changed |= remove.has_value();
				ImGui::TreePop();
			}
			return changed;
		}
		if (AudioClipArray* values =
				std::get_if<AudioClipArray>(&value))
		{
			bool changed = false;
			if (ImGui::TreeNode(label))
			{
				std::optional<std::size_t> remove;
				for (std::size_t index = 0; index < values->size(); ++index)
				{
					ImGui::PushID(static_cast<int>(index));
					changed |= DrawAudioClip(
						"##Clip", (*values)[index], availableClips);
					ImGui::SameLine();
					if (ImGui::SmallButton("x"))
						remove = index;
					ImGui::PopID();
				}
				if (remove)
					values->erase(values->begin() + *remove);
				if (ImGui::SmallButton("+ Audio Clip"))
				{
					values->push_back({});
					changed = true;
				}
				changed |= remove.has_value();
				ImGui::TreePop();
			}
			return changed;
		}
		return false;
	}
}
