// appcontext_test.cpp
//
// First Catch2 unit test for Phoenix-Firestorm. Demonstrates the AppContext
// dependency-injection seam introduced in Phase 2 / Track D:
//   - A mock IAOEngine can be substituted into gAppContext.aoEngine
//   - Any caller that uses gAppContext.aoEngine->X() now exercises the mock
//   - Production code remains unchanged; only test code touches gAppContext
//
// This is the proof-of-concept for the seam — once individual call sites are
// migrated from AOEngine::instance().X() to gAppContext.aoEngine->X(), each
// migrated caller becomes independently testable using the same pattern.

#define CATCH_CONFIG_MAIN
#include <catch.hpp>

#include "appcontext.h"
#include "iaoengine.h"

namespace
{
    // Records every call into IAOEngine so test cases can assert on it.
    // Methods we don't care about are stubbed with default returns.
    class MockAOEngine : public IAOEngine
    {
    public:
        int    enable_calls{ 0 };
        bool   last_enable_arg{ false };
        int    enableStands_calls{ 0 };
        bool   last_enableStands_arg{ false };
        int    override_calls{ 0 };
        LLUUID last_override_motion{};
        bool   last_override_start{ false };
        LLUUID override_return{ LLUUID::null };
        LLUUID ao_folder{ LLUUID::null };
        bool   in_mouselook{ false };
        std::string current_set_name{ "MockSet" };
        int    saveSettings_calls{ 0 };

        void enable(bool e) override { ++enable_calls; last_enable_arg = e; }
        void enableStands(bool e) override { ++enableStands_calls; last_enableStands_arg = e; }
        LLUUID override(const LLUUID& motion, bool start) override
        {
            ++override_calls;
            last_override_motion = motion;
            last_override_start  = start;
            return override_return;
        }
        void tick() override {}
        void update() override {}
        const LLUUID& getAOFolder() const override { return ao_folder; }
        void inMouselook(bool m) override { in_mouselook = m; }
        void checkSitCancel() override {}
        void checkBelowWater(bool) override {}
        const std::string getCurrentSetName() const override { return current_set_name; }
        void saveSettings() override { ++saveSettings_calls; }
    };

    // RAII helper: replace gAppContext.aoEngine for the duration of a test,
    // restore the previous value on scope exit. Tests can run in any order
    // without leaking mock pointers into other tests.
    class ScopedMockAOEngine
    {
    public:
        explicit ScopedMockAOEngine(IAOEngine* mock)
            : mPrevious(gAppContext.aoEngine) { gAppContext.aoEngine = mock; }
        ~ScopedMockAOEngine() { gAppContext.aoEngine = mPrevious; }
    private:
        IAOEngine* mPrevious;
    };
}

TEST_CASE("AppContext: a mock IAOEngine can be substituted via gAppContext", "[appcontext][track-d]")
{
    MockAOEngine mock;
    ScopedMockAOEngine guard(&mock);

    REQUIRE(gAppContext.aoEngine == &mock);

    gAppContext.aoEngine->enable(true);
    gAppContext.aoEngine->enable(false);

    CHECK(mock.enable_calls == 2);
    CHECK(mock.last_enable_arg == false);
}

TEST_CASE("AppContext: override() round-trips arguments and returns the mock's value", "[appcontext][track-d]")
{
    MockAOEngine mock;
    mock.override_return = LLUUID("12345678-1234-1234-1234-123456789abc");

    ScopedMockAOEngine guard(&mock);

    const LLUUID motion{ "fedcba98-7654-3210-fedc-ba9876543210" };
    const LLUUID returned = gAppContext.aoEngine->override(motion, true);

    CHECK(mock.override_calls == 1);
    CHECK(mock.last_override_motion == motion);
    CHECK(mock.last_override_start == true);
    CHECK(returned == mock.override_return);
}

TEST_CASE("AppContext: a const accessor on IAOEngine sees mock state", "[appcontext][track-d]")
{
    MockAOEngine mock;
    mock.ao_folder = LLUUID("aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa");
    mock.current_set_name = "TestSet42";

    ScopedMockAOEngine guard(&mock);

    CHECK(gAppContext.aoEngine->getAOFolder() == mock.ao_folder);
    CHECK(gAppContext.aoEngine->getCurrentSetName() == "TestSet42");
}

TEST_CASE("AppContext: scoped guard restores previous aoEngine on exit", "[appcontext][track-d]")
{
    MockAOEngine first;
    MockAOEngine second;

    auto* original = gAppContext.aoEngine;

    {
        ScopedMockAOEngine outer(&first);
        REQUIRE(gAppContext.aoEngine == &first);
        {
            ScopedMockAOEngine inner(&second);
            REQUIRE(gAppContext.aoEngine == &second);
        }
        REQUIRE(gAppContext.aoEngine == &first);
    }

    CHECK(gAppContext.aoEngine == original);
}
