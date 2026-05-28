#ifndef FS_BRIDGECOMMANDHANDLER_H
#define FS_BRIDGECOMMANDHANDLER_H

#include <optional>
#include <string>
#include <string_view>

// IBridgeEnvironment is the slice of viewer functionality the bridge command
// handler needs. The real implementation lives in fslslbridge.cpp; unit tests
// substitute a fake. Keeping this dependency-light (only std types) lets the
// command-parsing logic be tested without the viewer build.
class IBridgeEnvironment
{
public:
    virtual ~IBridgeEnvironment() = default;

    // Raw setting lookup — NO allowlist enforcement here; the handler owns that
    // policy. Returns nullopt if no control by that name exists.
    virtual std::optional<std::string> getSettingValue(const std::string& key) const = 0;

    // Send a pipe-delimited reply back to the in-world bridge script.
    virtual void sendToLSL(const std::string& message) = 0;

    // Show a user-facing alert popup.
    virtual void showAlert(const std::string& message) = 0;

    // Toggle a viewer floater open/closed by registered name.
    virtual void toggleFloater(const std::string& name) = 0;

    // Viewer identity, for the getViewerInfo command.
    virtual std::string getVersion() const = 0;
    virtual std::string getChannel() const = 0;
    virtual std::string getGridLabel() const = 0;
};

// Parses and dispatches the subset of LSL->viewer bridge commands that are pure
// logic (no inventory or bridge-state coupling): getViewerInfo, getSettingValue,
// showNotification, openFloater. Extracted from FSLSLBridge::lslToViewer so the
// parsing, allowlist, and UTF-8 truncation logic can be unit-tested against a
// fake IBridgeEnvironment.
//
// SECURITY: this handler does NOT verify that the message came from the genuine
// bridge object. That gate stays in FSLSLBridge::lslToViewer, which only invokes
// handle() after confirming the sender is the bridge and the bridge is enabled.
class FSBridgeCommandHandler
{
public:
    explicit FSBridgeCommandHandler(IBridgeEnvironment& env) : mEnv(env) {}

    // Returns true if `message`'s tag is one of this handler's commands (in which
    // case it was handled), false if the tag is not recognised.
    bool handle(std::string_view message);

private:
    bool handleGetViewerInfo();
    bool handleGetSettingValue(std::string_view message);
    bool handleShowNotification(std::string_view message);
    bool handleOpenFloater(std::string_view message);

    IBridgeEnvironment& mEnv;
};

#endif // FS_BRIDGECOMMANDHANDLER_H
