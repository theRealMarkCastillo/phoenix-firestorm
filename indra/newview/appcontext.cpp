// Skip the viewer's precompiled-header chain when building this file as part
// of the slim Catch2 test target — the PCH drags in basically all of newview's
// dependencies and is not needed to define a struct of pointers.
#ifndef FS_APPCONTEXT_TEST_BUILD
#include "llviewerprecompiledheaders.h"
#endif
#include "appcontext.h"

AppContext gAppContext{};
