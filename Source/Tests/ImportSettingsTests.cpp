#include "../ImportSettings.h"

#include <cmath>
#include <iostream>
#include <vector>

namespace
{
	struct CustomRange
	{
		int first = 0;
		int last = 10;
	};

	bool Check(bool condition, const char* message)
	{
		if (!condition)
			std::cerr << message << '\n';
		return condition;
	}

	bool NearlyEqual(float lhs, float rhs)
	{
		return std::abs(lhs - rhs) < 0.0001f;
	}

	bool NearlyEqual(const float2& lhs, const float2& rhs)
	{
		return NearlyEqual(lhs.x, rhs.x) &&
			NearlyEqual(lhs.y, rhs.y);
	}

	bool NearlyEqual(const float3& lhs, const float3& rhs)
	{
		return NearlyEqual(lhs.x, rhs.x) &&
			NearlyEqual(lhs.y, rhs.y) &&
			NearlyEqual(lhs.z, rhs.z);
	}

	bool NearlyEqual(const float4& lhs, const float4& rhs)
	{
		return NearlyEqual(lhs.x, rhs.x) &&
			NearlyEqual(lhs.y, rhs.y) &&
			NearlyEqual(lhs.z, rhs.z) &&
			NearlyEqual(lhs.w, rhs.w);
	}
}

int main()
{
	EGE::ImportCustomEditorRegistry::Get().Register<CustomRange>(
		"Test.CustomRange",
		[](const EGE::ImportSetting&, CustomRange&)
		{
			return false;
		});

	EGE::ImportSettings settings;
	settings.AddBoolean("enabled", "Enabled", true);
	settings.AddInteger("count", "Count", 7);
	settings.AddFloat("strength", "Strength", 0.75);
	settings.AddString("name", "Name", "Asset");
	settings.AddEnumeration(
		"mode", "Mode", 2, {{"First", 1}, {"Second", 2}});
	settings.AddVector2("size", "Size", float2(128.0f, 64.0f));
	settings.AddVector3("scale", "Scale", float3(1.0f, 2.0f, 3.0f));
	settings.AddColor("tint", "Tint", float4(0.1f, 0.2f, 0.3f, 0.4f));
	settings.AddCustom(
		"range", "Range", "Test.CustomRange", CustomRange{3, 9});

	const CustomRange* range =
		settings.GetCustom<CustomRange>("range", "Test.CustomRange");
	if (!Check(settings.GetBoolean("enabled"), "Boolean value failed") ||
		!Check(settings.GetInteger("count") == 7, "Integer value failed") ||
		!Check(
			std::abs(settings.GetFloat("strength") - 0.75) < 0.0001,
			"Float value failed") ||
		!Check(settings.GetString("name") == "Asset", "String value failed") ||
		!Check(settings.GetInteger("mode") == 2, "Enum value failed") ||
		!Check(
			NearlyEqual(
				settings.GetVector2("size"),
				float2(128.0f, 64.0f)),
			"Vector2 value failed") ||
		!Check(
			NearlyEqual(
				settings.GetVector3("scale"),
				float3(1.0f, 2.0f, 3.0f)),
			"Vector3 value failed") ||
		!Check(
			NearlyEqual(
				settings.GetColor("tint"),
				float4(0.1f, 0.2f, 0.3f, 0.4f)),
			"Color value failed") ||
		!Check(range && range->first == 3 && range->last == 9,
			"Custom value failed"))
	{
		return 1;
	}

	auto* customField = settings.Find("range");
	auto& custom = std::get<EGE::ImportCustomValue>(customField->value);
	static_cast<CustomRange*>(custom.storage.get())->first = 99;
	settings.Reset();
	range = settings.GetCustom<CustomRange>(
		"range", "Test.CustomRange");
	if (!Check(
			range && range->first == 3 && range->last == 9,
			"Custom default was not cloned during reset"))
	{
		return 1;
	}

	return 0;
}
