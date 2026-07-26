#pragma once

#include <memory>
#include <string>

namespace EGE
{
	struct EditorAssetSelection;

	class AssetEditorManager final
	{
	public:
		AssetEditorManager();
		~AssetEditorManager();

		AssetEditorManager(const AssetEditorManager&) = delete;
		AssetEditorManager& operator=(const AssetEditorManager&) = delete;

		bool Open(
			const EditorAssetSelection& asset,
			std::string& error);
		void Draw();
		void CloseAll();

	private:
		struct Impl;
		std::unique_ptr<Impl> impl_;
	};
}
