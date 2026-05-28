#ifndef FS_ISETTINGSREADER_H
#define FS_ISETTINGSREADER_H

#include "stdtypes.h"
#include <string>

// Read-only view of a settings group (e.g. gSavedSettings / gSavedPerAccountSettings).
// Lets settings-dependent logic be unit-tested against a fake reader instead of the
// live LLControlGroup. Read-only by design: writes stay with the concrete group, so
// test code can't accidentally mutate persistent settings.
//
// The production adapter (over LLControlGroup) lives in fscommon.cpp for now; it can
// be promoted to a shared header once more than one subsystem consumes the seam.
class ISettingsReader
{
public:
    virtual ~ISettingsReader() = default;

    virtual bool        getBOOL(const std::string& key) const = 0;
    virtual std::string getString(const std::string& key) const = 0;
    virtual S32         getS32(const std::string& key) const = 0;
    virtual F32         getF32(const std::string& key) const = 0;
};

#endif // FS_ISETTINGSREADER_H
