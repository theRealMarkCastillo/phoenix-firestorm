// fscommonsettings_test.cpp
//
// Proof-of-concept for the ISettingsReader seam: two FSCommon predicates whose
// only external dependency was gSavedSettings are now testable with a fake
// reader, no live viewer required. Demonstrates that seaming the settings group
// alone is sufficient to unit-test genuinely settings-only logic.

#define CATCH_CONFIG_MAIN
#include <catch.hpp>

#include "linden_common.h"

#include "fscommonsettings.h"
#include "isettingsreader.h"
#include "indra_constants.h"   // MASK_CONTROL, MASK_ALT

#include <map>

namespace
{
    // A fake settings group: returns only what the test puts in. Absent keys
    // read as false / empty, matching LLControlGroup's "missing control" default.
    class FakeSettings : public ISettingsReader
    {
    public:
        std::map<std::string, bool> bools;
        std::map<std::string, std::string> strings;

        bool getBOOL(const std::string& key) const override
        {
            auto it = bools.find(key);
            return it != bools.end() && it->second;
        }
        std::string getString(const std::string& key) const override
        {
            auto it = strings.find(key);
            return it != strings.end() ? it->second : std::string();
        }
        S32 getS32(const std::string&) const override { return 0; }
        F32 getF32(const std::string&) const override { return 0.f; }
    };
}

TEST_CASE("isFilterEditorKeyCombo matches Ctrl+F only when the setting is enabled", "[settings]")
{
    FakeSettings s;
    s.bools["FSSelectLocalSearchEditorOnShortcut"] = true;

    CHECK(FSCommon::isFilterEditorKeyCombo('F', MASK_CONTROL, s));
    CHECK_FALSE(FSCommon::isFilterEditorKeyCombo('G', MASK_CONTROL, s)); // wrong key
    CHECK_FALSE(FSCommon::isFilterEditorKeyCombo('F', MASK_ALT, s));     // wrong modifier
    CHECK_FALSE(FSCommon::isFilterEditorKeyCombo('F', 0, s));            // no modifier
}

TEST_CASE("isFilterEditorKeyCombo is false when the setting is disabled or absent", "[settings]")
{
    FakeSettings disabled;
    disabled.bools["FSSelectLocalSearchEditorOnShortcut"] = false;
    CHECK_FALSE(FSCommon::isFilterEditorKeyCombo('F', MASK_CONTROL, disabled));

    FakeSettings absent; // key never set
    CHECK_FALSE(FSCommon::isFilterEditorKeyCombo('F', MASK_CONTROL, absent));
}

TEST_CASE("isLegacySkin is true only for the Vintage skin", "[settings]")
{
    FakeSettings s;

    s.strings["FSInternalSkinCurrent"] = "Vintage";
    CHECK(FSCommon::isLegacySkin(s));

    s.strings["FSInternalSkinCurrent"] = "Latency";
    CHECK_FALSE(FSCommon::isLegacySkin(s));

    FakeSettings absent; // no skin set => not legacy
    CHECK_FALSE(FSCommon::isLegacySkin(absent));
}
