#include "../ModelImportCoordinates.h"

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace
{
	bool NearlyEqual(float left, float right)
	{
		return std::fabs(left - right) < 0.0001f;
	}

	bool Expect(bool condition, const char* message)
	{
		if (condition)
			return true;
		std::cerr << message << '\n';
		return false;
	}
}

int main()
{
	using namespace EGE::ModelImportCoordinates;

	if (!Expect(IsGlTf("model.glb"), "GLB was not detected.") ||
		!Expect(IsGlTf("MODEL.GLTF"), "GLTF detection is case-sensitive.") ||
		!Expect(!IsGlTf("model.fbx"), "FBX must not use GLTF conversion.") ||
		!Expect(!IsGlTf("model.dae"), "DAE must not use GLTF conversion.") ||
		!Expect(!IsGlTf("model.obj"), "OBJ must not use GLTF conversion.") ||
		!Expect(!IsGlTf("model.3ds"), "3DS must not use GLTF conversion.") ||
		!Expect(!IsGlTf("model.ply"), "PLY must not use GLTF conversion.") ||
		!Expect(!IsGlTf("model.stl"), "STL must not use GLTF conversion.") ||
		!Expect(
			!IsGlTf("model.blend"),
			"Blender files must not use GLTF conversion."))
	{
		return EXIT_FAILURE;
	}

	const float3 position =
		Position(float3(1.0f, 2.0f, 3.0f), float3(2.0f), true);
	if (!Expect(
			position.Equals(float3(-2.0f, 4.0f, 6.0f)),
			"Position conversion is incorrect."))
	{
		return EXIT_FAILURE;
	}

	const float4 tangent =
		Tangent(float4(1.0f, 2.0f, 3.0f, 1.0f), true);
	if (!Expect(
			tangent.Equals(float4(-1.0f, 2.0f, 3.0f, -1.0f)),
			"Tangent handedness conversion is incorrect."))
	{
		return EXIT_FAILURE;
	}

	const Quat rotation =
		Rotation(Quat(0.1f, 0.2f, 0.3f, 0.9f), true);
	if (!Expect(
			NearlyEqual(rotation.x, 0.1f) &&
				NearlyEqual(rotation.y, -0.2f) &&
				NearlyEqual(rotation.z, -0.3f) &&
				NearlyEqual(rotation.w, 0.9f),
			"Rotation conversion is incorrect."))
	{
		return EXIT_FAILURE;
	}

	float4x4 matrix = float4x4::identity;
	matrix.SetTranslatePart(float3(3.0f, 4.0f, 5.0f));
	matrix = Transform(matrix, float3(2.0f), true);
	if (!Expect(
			matrix.TranslatePart().Equals(
				float3(-6.0f, 8.0f, 10.0f)),
			"Transform conversion is incorrect."))
	{
		return EXIT_FAILURE;
	}

	unsigned indices[] = {0, 1, 2, 3, 4, 5};
	ReverseTriangleWinding(indices, 6, true);
	if (!Expect(
			indices[0] == 0 && indices[1] == 2 && indices[2] == 1 &&
				indices[3] == 3 && indices[4] == 5 &&
				indices[5] == 4,
			"Triangle winding conversion is incorrect."))
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
