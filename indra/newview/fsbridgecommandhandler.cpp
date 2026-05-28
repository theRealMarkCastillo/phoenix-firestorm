// Skip the viewer's precompiled-header chain when building this file into the
// slim Catch2 test target — this handler only needs llcommon, not all of newview.
// In the test build we still need linden_common.h (it defines LL_COMMON_API and
// the fundamental types that llstring.h / llerror.h depend on).
#ifndef FS_BRIDGE_HANDLER_TEST_BUILD
#include "llviewerprecompiledheaders.h"
#else
#include "linden_common.h"
#endif

#include "fsbridgecommandhandler.h"

#include "llstring.h"   // LLStringUtil::trim
#include "llerror.h"    // LL_WARNS

#include <set>

bool FSBridgeCommandHandler::handle(std::string_view message)
{
    if (message.empty() || message[0] != '<')
    {
        return false;
    }

    // Extract the leading tag: text up to the first '>' or ' ', whichever comes
    // first. Mirrors the tag logic in FSLSLBridge::lslToViewer.
    size_t closebracket = message.find('>');
    size_t firstblank   = message.find(' ');
    size_t tagend;
    if (closebracket == std::string_view::npos)
    {
        tagend = firstblank;
    }
    else if (firstblank == std::string_view::npos)
    {
        tagend = closebracket;
    }
    else
    {
        tagend = (closebracket < firstblank) ? closebracket : firstblank;
    }
    if (tagend == std::string_view::npos)
    {
        return false;
    }
    const std::string_view tag = message.substr(0, tagend + 1);

    if (tag == "<getViewerInfo/>")   { return handleGetViewerInfo(); }
    if (tag == "<getSettingValue>")  { return handleGetSettingValue(message); }
    if (tag == "<showNotification>") { return handleShowNotification(message); }
    if (tag == "<openFloater>")      { return handleOpenFloater(message); }

    return false;
}

bool FSBridgeCommandHandler::handleGetViewerInfo()
{
    // Reply: "ViewerInfo|<version>|<channel>|<grid label>"
    mEnv.sendToLSL("ViewerInfo|" + mEnv.getVersion() + "|" + mEnv.getChannel() + "|" + mEnv.getGridLabel());
    return true;
}

bool FSBridgeCommandHandler::handleGetSettingValue(std::string_view message)
{
    // <getSettingValue>SettingName</getSettingValue>
    // Reply: "SettingValue|<key>|<value>"  on success
    //        "SettingValue||DENIED"        key not allowlisted or contains '|'
    //        "SettingValue||ERROR"         malformed (missing close tag)
    // A reply is ALWAYS sent so bridge-side coroutines never hang.
    static const std::string open_tag  = "<getSettingValue>";
    static const std::string close_tag = "</getSettingValue>";
    const size_t val_start = message.find(open_tag) + open_tag.size();
    const size_t val_end   = message.find(close_tag);
    if (val_start == std::string_view::npos || val_end == std::string_view::npos || val_end <= val_start)
    {
        LL_WARNS("FSLSLBridge") << "getSettingValue: malformed message (missing close tag)" << LL_ENDL;
        mEnv.sendToLSL("SettingValue||ERROR");
        return true;
    }

    std::string key{ message.substr(val_start, val_end - val_start) };
    LLStringUtil::trim(key);

    // A key containing '|' would corrupt the pipe-delimited reply the bridge parses.
    if (key.find('|') != std::string::npos)
    {
        LL_WARNS("FSLSLBridge") << "getSettingValue: key contains '|', rejected" << LL_ENDL;
        mEnv.sendToLSL("SettingValue||DENIED");
        return true;
    }

    static const std::set<std::string> FS_BRIDGE_SETTING_ALLOWLIST = {
        "UseLSLBridge",
        "UseLSLFlightAssist",
        "UseMoveLock",
        "RelockMoveLockAfterMovement",
        "RelockMoveLockAfterRegionChange",
        "UseAO",
        "UseAOStands",
        "PauseAO",
        "BridgeIntegrationOC",
        "BridgeIntegrationLM",
    };

    if (!FS_BRIDGE_SETTING_ALLOWLIST.count(key))
    {
        LL_WARNS("FSLSLBridge") << "getSettingValue: key not in allowlist: " << key << LL_ENDL;
        mEnv.sendToLSL("SettingValue||DENIED");
        return true;
    }

    const std::string value = mEnv.getSettingValue(key).value_or("");
    mEnv.sendToLSL("SettingValue|" + key + "|" + value);
    return true;
}

bool FSBridgeCommandHandler::handleShowNotification(std::string_view message)
{
    // <showNotification>Message text</showNotification>
    // Trimmed and capped at 512 bytes on a valid UTF-8 boundary.
    static const std::string open_tag  = "<showNotification>";
    static const std::string close_tag = "</showNotification>";
    const size_t text_start = message.find(open_tag) + open_tag.size();
    const size_t text_end   = message.find(close_tag);
    if (text_start == std::string_view::npos || text_end == std::string_view::npos || text_end <= text_start)
    {
        return true; // recognised but malformed; nothing to show
    }

    std::string text{ message.substr(text_start, text_end - text_start) };
    LLStringUtil::trim(text);
    if (text.empty())
    {
        return true;
    }

    if (text.size() > 512)
    {
        size_t cut = 512;
        // Walk back to the start of any multi-byte UTF-8 sequence so we don't
        // leave a partial code-unit at the end of the string.
        while (cut > 0 && (static_cast<unsigned char>(text[cut]) & 0xC0) == 0x80)
        {
            --cut;
        }
        text.resize(cut);
    }
    mEnv.showAlert(text);
    return true;
}

bool FSBridgeCommandHandler::handleOpenFloater(std::string_view message)
{
    // <openFloater>floater_name</openFloater>
    // Toggles a Firestorm floater open/closed. Only allowlisted names accepted.
    static const std::string open_tag  = "<openFloater>";
    static const std::string close_tag = "</openFloater>";
    const size_t name_start = message.find(open_tag) + open_tag.size();
    const size_t name_end   = message.find(close_tag);
    if (name_start == std::string_view::npos || name_end == std::string_view::npos || name_end <= name_start)
    {
        return true; // recognised but malformed
    }

    std::string floater_name{ message.substr(name_start, name_end - name_start) };
    LLStringUtil::trim(floater_name);

    static const std::set<std::string> FS_BRIDGE_FLOATER_ALLOWLIST = {
        "area_search",
        "contacts",
        "nearby_chat",
        "people",
        "radar",
        "fs_radar",
    };

    if (FS_BRIDGE_FLOATER_ALLOWLIST.count(floater_name))
    {
        mEnv.toggleFloater(floater_name);
    }
    else
    {
        LL_WARNS("FSLSLBridge") << "openFloater: floater not in allowlist: " << floater_name << LL_ENDL;
    }
    return true;
}
