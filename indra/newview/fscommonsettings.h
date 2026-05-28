#ifndef FS_FSCOMMONSETTINGS_H
#define FS_FSCOMMONSETTINGS_H

#include "stdtypes.h"   // KEY, MASK

class ISettingsReader;

// Settings-dependent FSCommon predicates whose ONLY external dependency is the
// settings group. Extracted behind ISettingsReader so they can be unit-tested
// without the live viewer. The zero-extra-arg overloads in fscommon.cpp wrap
// these with a reader backed by gSavedSettings.
namespace FSCommon
{
    // True if the (key, modifier) combo is the "focus filter editor" shortcut
    // (Ctrl+F) AND the feature is enabled in settings.
    bool isFilterEditorKeyCombo(KEY key, MASK mask, const ISettingsReader& settings);

    // True if the currently selected internal skin is the legacy "Vintage" skin.
    bool isLegacySkin(const ISettingsReader& settings);
}

#endif // FS_FSCOMMONSETTINGS_H
