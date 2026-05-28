// Slim, settings-only FSCommon predicates. Built into the viewer normally and
// into a Catch2 test target with FS_FSCOMMONSETTINGS_TEST_BUILD (which skips the
// viewer PCH so the unit links against llcommon alone).
#ifndef FS_FSCOMMONSETTINGS_TEST_BUILD
#include "llviewerprecompiledheaders.h"
#else
#include "linden_common.h"
#endif

#include "fscommonsettings.h"
#include "isettingsreader.h"

#include "indra_constants.h"   // MASK_CONTROL

namespace FSCommon
{
    bool isFilterEditorKeyCombo(KEY key, MASK mask, const ISettingsReader& settings)
    {
        return mask == MASK_CONTROL
            && key == 'F'
            && settings.getBOOL("FSSelectLocalSearchEditorOnShortcut");
    }

    bool isLegacySkin(const ISettingsReader& settings)
    {
        return settings.getString("FSInternalSkinCurrent") == "Vintage";
    }
}
