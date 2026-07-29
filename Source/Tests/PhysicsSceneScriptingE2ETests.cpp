#include "../PhysicsSceneScriptingSelfTest.h"

#include <cstdlib>
#include <iostream>

int main()
{
	if (!EGE::RunPhysicsSceneScriptingSelfTest())
	{
		std::cerr
			<< "Physics scene and AngelScript end-to-end test failed.\n";
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
