#pragma once

#include "AssetBrowserModel.h"
#include "Globals.h"

#include <string>
#include <vector>

namespace EGE
{
	struct EditorAssetSelection
	{
		std::string sourcePath;
		AssetKind kind = AssetKind::Unknown;
		bool directory = false;
		UID primaryResource = 0;
		std::vector<UID> linkedResources;
	};
}
