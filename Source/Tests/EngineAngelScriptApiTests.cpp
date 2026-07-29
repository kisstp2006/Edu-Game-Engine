#include "../Scripting/ScriptBindings.h"

#include <cstdlib>
#include <iostream>

int main()
{
	if (!EGE::RunEngineBindingsSelfTest())
	{
		std::cerr
			<< "Engine AngelScript API self-test "
			<< "did not compile.\n";
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
