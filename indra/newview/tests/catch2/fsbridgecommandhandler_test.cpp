// fsbridgecommandhandler_test.cpp
//
// Unit tests for FSBridgeCommandHandler — the LSL->viewer bridge command logic
// extracted from FSLSLBridge::lslToViewer (Track A) so it can be tested in
// isolation. These tests lock in the behaviour of the five bugs fixed during
// code review: pipe-injection rejection, allowlist enforcement, malformed-message
// replies, UTF-8-safe truncation, and floater-allowlist gating.

#define CATCH_CONFIG_MAIN
#include <catch.hpp>

#include "fsbridgecommandhandler.h"

#include <map>
#include <vector>

namespace
{
    // Records every interaction so tests can assert on outputs, and returns
    // canned setting values supplied by the test.
    class FakeBridgeEnvironment : public IBridgeEnvironment
    {
    public:
        std::vector<std::string> sentToLSL;
        std::vector<std::string> alerts;
        std::vector<std::string> toggledFloaters;
        std::map<std::string, std::string> settings; // key -> value (present == control exists)

        std::string version{ "1.2.3" };
        std::string channel{ "Firestorm-Test" };
        std::string gridLabel{ "TestGrid" };

        std::optional<std::string> getSettingValue(const std::string& key) const override
        {
            auto it = settings.find(key);
            if (it == settings.end())
            {
                return std::nullopt;
            }
            return it->second;
        }
        void sendToLSL(const std::string& message) override { sentToLSL.push_back(message); }
        void showAlert(const std::string& message) override { alerts.push_back(message); }
        void toggleFloater(const std::string& name) override { toggledFloaters.push_back(name); }
        std::string getVersion() const override   { return version; }
        std::string getChannel() const override   { return channel; }
        std::string getGridLabel() const override { return gridLabel; }
    };
}

TEST_CASE("handle() returns false for an unrecognised tag", "[bridge][track-a]")
{
    FakeBridgeEnvironment env;
    FSBridgeCommandHandler handler(env);

    CHECK_FALSE(handler.handle("<someOtherTag>payload</someOtherTag>"));
    CHECK_FALSE(handler.handle("not even a tag"));
    CHECK_FALSE(handler.handle(""));
    CHECK(env.sentToLSL.empty());
}

TEST_CASE("getViewerInfo replies with version|channel|grid", "[bridge][track-a]")
{
    FakeBridgeEnvironment env;
    env.version = "7.1.9";
    env.channel = "Firestorm-Release";
    env.gridLabel = "Second Life";
    FSBridgeCommandHandler handler(env);

    REQUIRE(handler.handle("<getViewerInfo/>"));
    REQUIRE(env.sentToLSL.size() == 1);
    CHECK(env.sentToLSL[0] == "ViewerInfo|7.1.9|Firestorm-Release|Second Life");
}

TEST_CASE("getSettingValue returns the value for an allowlisted key", "[bridge][track-a]")
{
    FakeBridgeEnvironment env;
    env.settings["UseAO"] = "1";
    FSBridgeCommandHandler handler(env);

    REQUIRE(handler.handle("<getSettingValue>UseAO</getSettingValue>"));
    REQUIRE(env.sentToLSL.size() == 1);
    CHECK(env.sentToLSL[0] == "SettingValue|UseAO|1");
}

TEST_CASE("getSettingValue trims surrounding whitespace from the key", "[bridge][track-a]")
{
    FakeBridgeEnvironment env;
    env.settings["UseMoveLock"] = "0";
    FSBridgeCommandHandler handler(env);

    REQUIRE(handler.handle("<getSettingValue>  UseMoveLock  </getSettingValue>"));
    REQUIRE(env.sentToLSL.size() == 1);
    CHECK(env.sentToLSL[0] == "SettingValue|UseMoveLock|0");
}

TEST_CASE("getSettingValue returns empty value when the control does not exist", "[bridge][track-a]")
{
    FakeBridgeEnvironment env; // allowlisted key, but no control registered
    FSBridgeCommandHandler handler(env);

    REQUIRE(handler.handle("<getSettingValue>PauseAO</getSettingValue>"));
    REQUIRE(env.sentToLSL.size() == 1);
    CHECK(env.sentToLSL[0] == "SettingValue|PauseAO|");
}

TEST_CASE("getSettingValue denies a key that is not in the allowlist", "[bridge][track-a]")
{
    FakeBridgeEnvironment env;
    env.settings["RenderVolumeLODFactor"] = "4.0"; // exists, but not allowlisted
    FSBridgeCommandHandler handler(env);

    REQUIRE(handler.handle("<getSettingValue>RenderVolumeLODFactor</getSettingValue>"));
    REQUIRE(env.sentToLSL.size() == 1);
    CHECK(env.sentToLSL[0] == "SettingValue||DENIED");
}

TEST_CASE("getSettingValue rejects a key containing a pipe (injection guard)", "[bridge][track-a]")
{
    FakeBridgeEnvironment env;
    FSBridgeCommandHandler handler(env);

    REQUIRE(handler.handle("<getSettingValue>UseAO|evil</getSettingValue>"));
    REQUIRE(env.sentToLSL.size() == 1);
    CHECK(env.sentToLSL[0] == "SettingValue||DENIED");
}

TEST_CASE("getSettingValue replies ERROR on a malformed (unterminated) message", "[bridge][track-a]")
{
    FakeBridgeEnvironment env;
    FSBridgeCommandHandler handler(env);

    REQUIRE(handler.handle("<getSettingValue>UseAO")); // no close tag
    REQUIRE(env.sentToLSL.size() == 1);
    CHECK(env.sentToLSL[0] == "SettingValue||ERROR");
}

TEST_CASE("showNotification shows trimmed text", "[bridge][track-a]")
{
    FakeBridgeEnvironment env;
    FSBridgeCommandHandler handler(env);

    REQUIRE(handler.handle("<showNotification>  hello world  </showNotification>"));
    REQUIRE(env.alerts.size() == 1);
    CHECK(env.alerts[0] == "hello world");
}

TEST_CASE("showNotification ignores empty text", "[bridge][track-a]")
{
    FakeBridgeEnvironment env;
    FSBridgeCommandHandler handler(env);

    REQUIRE(handler.handle("<showNotification>   </showNotification>"));
    CHECK(env.alerts.empty());
}

TEST_CASE("showNotification caps ASCII text at 512 bytes", "[bridge][track-a]")
{
    FakeBridgeEnvironment env;
    FSBridgeCommandHandler handler(env);

    const std::string body(600, 'a');
    REQUIRE(handler.handle("<showNotification>" + body + "</showNotification>"));
    REQUIRE(env.alerts.size() == 1);
    CHECK(env.alerts[0].size() == 512);
}

TEST_CASE("showNotification truncates on a UTF-8 boundary, never mid-sequence", "[bridge][track-a]")
{
    FakeBridgeEnvironment env;
    FSBridgeCommandHandler handler(env);

    // "a" followed by 256 two-byte 'é' (0xC3 0xA9). Total payload = 1 + 512 = 513
    // bytes. A naive resize(512) would split the 256th 'é' (its lead byte lands
    // at index 511, continuation at 512). The walk-back must drop the whole
    // character, leaving "a" + 255 'é' = 511 bytes.
    std::string body = "a";
    for (int i = 0; i < 256; ++i)
    {
        body += "\xc3\xa9";
    }
    REQUIRE(body.size() == 513);

    REQUIRE(handler.handle("<showNotification>" + body + "</showNotification>"));
    REQUIRE(env.alerts.size() == 1);

    const std::string& out = env.alerts[0];
    CHECK(out.size() == 511);
    // Last byte must be a complete 'é' continuation byte (0xA9), i.e. the string
    // ends on a whole character, not a dangling lead byte.
    REQUIRE_FALSE(out.empty());
    CHECK(static_cast<unsigned char>(out.back()) == 0xA9);
}

TEST_CASE("openFloater toggles an allowlisted floater", "[bridge][track-a]")
{
    FakeBridgeEnvironment env;
    FSBridgeCommandHandler handler(env);

    REQUIRE(handler.handle("<openFloater>area_search</openFloater>"));
    REQUIRE(env.toggledFloaters.size() == 1);
    CHECK(env.toggledFloaters[0] == "area_search");
}

TEST_CASE("openFloater ignores a non-allowlisted floater (e.g. inventory)", "[bridge][track-a]")
{
    FakeBridgeEnvironment env;
    FSBridgeCommandHandler handler(env);

    // "inventory" was deliberately removed from the allowlist (RLV @showinv bypass).
    REQUIRE(handler.handle("<openFloater>inventory</openFloater>"));
    CHECK(env.toggledFloaters.empty());
}
