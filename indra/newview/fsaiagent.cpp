/**
 * @file fsaiagent.cpp
 * @brief In-viewer AI agent singleton - gathers world context and queries LLM API
 *
 * $LicenseInfo:firstyear=2025&license=fsviewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (C) 2025, The Phoenix Firestorm Project, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * The Phoenix Firestorm Project, Inc., 1831 Oakwood Drive, Fairmont, Minnesota 56031-3225 USA
 * http://www.firestormviewer.org
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"
#include "fsaiagent.h"

#include "fsfloateraiagent.h"
#include "fsnearbychathub.h"

#include "llagent.h"
#include "llappearancemgr.h"
#include "llavatarname.h"
#include "llavatarnamecache.h"
#include "lldir.h"
#include "llfloaterreg.h"
#include "llinventorymodel.h"
#include "llparcel.h"
#include "llsdserialize.h"
#include "llviewercontrol.h"
#include "llviewerparcelmgr.h"
#include "llviewerregion.h"
#include "llworld.h"
#include "llsdjson.h"

#include <boost/json.hpp>
#include <sstream>
#include <regex>

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

FSAIAgent::FSAIAgent() = default;

FSAIAgent::~FSAIAgent()
{
    cleanupObservers();
}

// ---------------------------------------------------------------------------
// Observer management
// ---------------------------------------------------------------------------

void FSAIAgent::initObservers()
{
    if (mObserversInited) return;
    mObserversInited = true;

    LLViewerParcelMgr::getInstance()->addObserver(this);

    mRegionChangedConnection = gAgent.addRegionChangedCallback(
        [this]() { onRegionChanged(); }
    );

    // Capture current location so the first change() is truly a change
    LLParcel* parcel = LLViewerParcelMgr::getInstance()->getAgentParcel();
    if (parcel) mLastParcelName = parcel->getName();
    LLViewerRegion* region = gAgent.getRegion();
    if (region)    mLastRegionName = region->getName();
}

void FSAIAgent::cleanupObservers()
{
    if (!mObserversInited) return;
    mObserversInited = false;

    LLViewerParcelMgr::getInstance()->removeObserver(this);
    mRegionChangedConnection.disconnect();
}

// LLParcelObserver::changed — fires frequently; only act on actual name changes
void FSAIAgent::changed()
{
    LLParcel* parcel = LLViewerParcelMgr::getInstance()->getAgentParcel();
    if (!parcel) return;
    std::string parcel_name = parcel->getName();
    if (parcel_name == mLastParcelName) return;
    mLastParcelName = parcel_name;

    LLViewerRegion* region = gAgent.getRegion();
    std::string region_name = region ? region->getName() : "(unknown)";

    std::ostringstream msg;
    msg << "Moved to: " << (parcel_name.empty() ? "(unnamed parcel)" : parcel_name)
        << " in " << region_name;
    if (parcel)
    {
        msg << " | build=" << (parcel->getAllowModify() ? "yes" : "no")
            << " scripts=" << (parcel->getAllowOtherScripts() ? "yes" : "no")
            << " fly=" << (parcel->getAllowFly() ? "yes" : "no");
    }
    notifyFloater("Location", msg.str());
}

void FSAIAgent::onRegionChanged()
{
    LLViewerRegion* region = gAgent.getRegion();
    if (!region) return;
    std::string region_name = region->getName();
    if (region_name == mLastRegionName) return;
    mLastRegionName = region_name;
    mLastParcelName.clear(); // reset so next parcel change triggers too

    notifyFloater("Region", "Crossed into region: " + region_name);
}

void FSAIAgent::notifyFloater(const std::string& label, const std::string& text)
{
    FSFloaterAIAgent* floater = LLFloaterReg::findTypedInstance<FSFloaterAIAgent>("fs_ai_agent");
    if (floater)
        floater->appendMessage(label, text);
}

// ---------------------------------------------------------------------------
// Nearby chat feed
// ---------------------------------------------------------------------------

void FSAIAgent::addNearbyChatLine(const std::string& speaker, const std::string& text)
{
    if (mRecentChat.size() >= static_cast<size_t>(MAX_CHAT_LINES))
        mRecentChat.erase(mRecentChat.begin());
    mRecentChat.push_back(speaker + ": " + text);
}

// ---------------------------------------------------------------------------
// History persistence
// ---------------------------------------------------------------------------

void FSAIAgent::saveHistory()
{
    if (mHistory.empty()) return;
    std::string filepath = gDirUtilp->getExpandedFilename(LL_PATH_PER_SL_ACCOUNT, "fs_ai_history.json");
    boost::json::array arr;
    for (const auto& msg : mHistory)
        arr.push_back(boost::json::object{{"role", msg.role}, {"content", msg.content}});
    llofstream file(filepath);
    if (file.is_open())
        file << boost::json::serialize(arr);
}

void FSAIAgent::loadHistory()
{
    if (mHistoryLoaded) return;
    mHistoryLoaded = true;
    std::string filepath = gDirUtilp->getExpandedFilename(LL_PATH_PER_SL_ACCOUNT, "fs_ai_history.json");
    llifstream file(filepath);
    if (!file.is_open()) return;
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    try
    {
        auto parsed = boost::json::parse(content);
        for (const auto& item : parsed.as_array())
        {
            mHistory.push_back({
                std::string(item.at("role").as_string()),
                std::string(item.at("content").as_string())
            });
        }
    }
    catch (...) {} // corrupt file — start fresh
}

void FSAIAgent::clearHistory()
{
    mHistory.clear();
    // Remove saved file too
    std::string filepath = gDirUtilp->getExpandedFilename(LL_PATH_PER_SL_ACCOUNT, "fs_ai_history.json");
    LLFile::remove(filepath);
}

// ---------------------------------------------------------------------------
// World context / system prompt
// ---------------------------------------------------------------------------

std::string FSAIAgent::buildSystemPrompt() const
{
    std::ostringstream ctx;
    ctx << "You are an in-world assistant running inside the Firestorm viewer for Second Life.\n\n";

    // --- Agent identity ---
    LLAvatarName av_self;
    if (LLAvatarNameCache::get(gAgent.getID(), &av_self))
        ctx << "User's avatar: " << av_self.getCompleteName() << "\n";
    const std::string& group_name = gAgent.getGroupName();
    if (!group_name.empty())
        ctx << "Active group: " << group_name << "\n";

    // --- Region & parcel ---
    LLViewerRegion* region = gAgent.getRegion();
    ctx << "Region: " << (region ? region->getName() : "(not connected)") << "\n";

    LLParcel* parcel = LLViewerParcelMgr::getInstance()->getAgentParcel();
    if (parcel)
    {
        std::string parcel_name = parcel->getName();
        ctx << "Parcel: " << (parcel_name.empty() ? "(unnamed)" : parcel_name) << "\n";
        ctx << "Parcel permissions:"
            << " build=" << (parcel->getAllowModify() ? "yes" : "no")
            << " scripts=" << (parcel->getAllowOtherScripts() ? "yes" : "no")
            << " fly=" << (parcel->getAllowFly() ? "yes" : "no")
            << " voice=" << (parcel->getParcelFlagAllowVoice() ? "yes" : "no")
            << " public=" << (parcel->isPublic() ? "yes" : "no")
            << "\n";
        S32 prims_used = parcel->getSimWidePrimCount();
        S32 prims_max  = parcel->getSimWideMaxPrimCapacity();
        if (prims_max > 0)
            ctx << "Prims: " << prims_used << " used / " << prims_max << " max\n";
    }

    // --- Position & movement ---
    LLVector3 pos = gAgent.getPositionAgent();
    ctx << "Position: (" << llround(pos.mV[VX]) << ", "
                         << llround(pos.mV[VY]) << ", "
                         << llround(pos.mV[VZ]) << ")\n";
    ctx << "Flying: " << (gAgent.getFlying() ? "yes" : "no") << "\n";
    ctx << "Sitting: " << (LLAgent::isSitting() ? "yes" : "no") << "\n";

    // --- Inventory & outfit ---
    ctx << "Inventory items: " << gInventory.getItemCount() << "\n";
    std::string outfit_name;
    if (LLAppearanceMgr::getInstance()->getBaseOutfitName(outfit_name) && !outfit_name.empty())
        ctx << "Current outfit: " << outfit_name << "\n";

    // --- Nearby avatars (within 20m) ---
    uuid_vec_t avatar_ids;
    std::vector<LLVector3d> avatar_positions;
    LLWorld::instance().getAvatars(&avatar_ids, &avatar_positions, gAgent.getPositionGlobal(), 20.f);
    if (!avatar_ids.empty())
    {
        ctx << "Nearby avatars (" << avatar_ids.size() << " within 20m):\n";
        for (size_t i = 0; i < avatar_ids.size() && i < 10; ++i)
        {
            LLAvatarName nearby_name;
            if (LLAvatarNameCache::get(avatar_ids[i], &nearby_name))
            {
                F64 dx   = avatar_positions[i].mdV[VX] - gAgent.getPositionGlobal().mdV[VX];
                F64 dy   = avatar_positions[i].mdV[VY] - gAgent.getPositionGlobal().mdV[VY];
                ctx << "  " << nearby_name.getCompleteName()
                    << " (" << llround(sqrt(dx*dx + dy*dy)) << "m)\n";
            }
        }
    }
    else
    {
        ctx << "Nearby avatars: none\n";
    }

    // --- Recent nearby chat ---
    if (!mRecentChat.empty())
    {
        ctx << "Recent nearby chat:\n";
        for (const auto& line : mRecentChat)
            ctx << "  " << line << "\n";
    }

    ctx << "\n";
    ctx << "Instructions:\n";
    ctx << "- Reply in plain text only. Do not use markdown (no **, no #, no bullet hyphens with bold).\n";
    ctx << "- Be concise.\n";
    ctx << "- Answer questions about Second Life, help with LSL scripting, and explain the viewer UI.\n";
    if (gSavedSettings.getBOOL("FSAIAgentSpeakInChat"))
    {
        ctx << "- You may speak aloud in local chat by including [SAY: your message] in your response. "
               "Use this sparingly and only when it adds value (e.g. greeting nearby avatars).\n";
    }

    return ctx.str();
}

// ---------------------------------------------------------------------------
// [SAY:] extraction
// ---------------------------------------------------------------------------

// static
std::string FSAIAgent::extractAndSpeak(const std::string& text)
{
    static const std::string SAY_OPEN  = "[SAY:";
    static const std::string SAY_CLOSE = "]";
    bool can_speak = gSavedSettings.getBOOL("FSAIAgentSpeakInChat");

    std::string cleaned;
    size_t pos = 0;
    while (pos < text.size())
    {
        size_t start = text.find(SAY_OPEN, pos);
        if (start == std::string::npos)
        {
            cleaned += text.substr(pos);
            break;
        }
        cleaned += text.substr(pos, start - pos);
        size_t end = text.find(SAY_CLOSE, start + SAY_OPEN.size());
        if (end == std::string::npos)
        {
            cleaned += text.substr(start);
            break;
        }
        std::string say_text = text.substr(start + SAY_OPEN.size(), end - start - SAY_OPEN.size());
        LLStringUtil::trim(say_text);
        if (can_speak && !say_text.empty())
            FSNearbyChat::instance().sendChatFromViewer(say_text, CHAT_TYPE_NORMAL, false);
        pos = end + SAY_CLOSE.size();
    }
    return cleaned;
}

// ---------------------------------------------------------------------------
// LLM query
// ---------------------------------------------------------------------------

void FSAIAgent::query(const std::string& user_text, response_callback_t callback)
{
    if (mBusy)
    {
        callback(false, "Still waiting for previous response.");
        return;
    }
    loadHistory();
    mBusy = true;
    LLCoros::instance().launch(
        "FSAIAgent::queryCoro",
        [this, user_text, callback]() { queryCoro(user_text, callback); }
    );
}

void FSAIAgent::queryCoro(const std::string& user_text, response_callback_t callback)
{
    const std::string api_key  = gSavedSettings.getString("FSAIAgentAPIKey");
    const std::string model    = gSavedSettings.getString("FSAIAgentModel");
    const std::string endpoint = gSavedSettings.getString("FSAIAgentEndpoint");

    if (api_key.empty())
    {
        mBusy = false;
        callback(false, "No API key set. Open Preferences > Firestorm > AI Agent and enter your Anthropic API key.");
        return;
    }

    mHistory.push_back({"user", user_text});

    boost::json::array messages_json;
    for (const auto& msg : mHistory)
        messages_json.push_back(boost::json::object{{"role", msg.role}, {"content", msg.content}});

    boost::json::object body_json{
        {"model",      model},
        {"max_tokens", 1024},
        {"system",     buildSystemPrompt()},
        {"messages",   messages_json}
    };

    const std::string body_str = boost::json::serialize(body_json);
    LLCore::BufferArray::ptr_t raw(new LLCore::BufferArray());
    LLCore::BufferArrayStream body_stream(raw.get());
    body_stream << body_str;

    LLCore::HttpRequest::policy_t policy = LLCore::HttpRequest::DEFAULT_POLICY_ID;
    LLCoreHttpUtil::HttpCoroutineAdapter adapter("FSAIAgentQuery", policy);
    LLCore::HttpRequest::ptr_t  request = std::make_shared<LLCore::HttpRequest>();
    LLCore::HttpOptions::ptr_t  options = std::make_shared<LLCore::HttpOptions>();
    options->setWantHeaders(true);

    LLCore::HttpHeaders::ptr_t headers = std::make_shared<LLCore::HttpHeaders>();
    headers->append("x-api-key",         api_key);
    headers->append("anthropic-version", "2023-06-01");
    headers->append("content-type",      "application/json");

    LLSD result = adapter.postRawAndSuspend(request, endpoint, raw, options, headers);

    LLCore::HttpStatus status =
        LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(
            result[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS]);

    mBusy = false;

    if (!status)
    {
        if (!mHistory.empty()) mHistory.pop_back();
        callback(false, "API error: " + status.getMessage());
        return;
    }

    const LLSD::Binary& raw_data =
        result[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS_RAW].asBinary();
    std::string response_str(raw_data.begin(), raw_data.end());

    std::string assistant_text;
    try
    {
        auto parsed = boost::json::parse(response_str);
        assistant_text = parsed.at("content").at(0).at("text").as_string().c_str();
    }
    catch (const std::exception& e)
    {
        if (!mHistory.empty()) mHistory.pop_back();
        callback(false, std::string("Failed to parse response: ") + e.what());
        return;
    }

    // Extract and speak any [SAY:] commands; get cleaned display text
    std::string display_text = extractAndSpeak(assistant_text);

    mHistory.push_back({"assistant", assistant_text}); // store original (with [SAY:]) for context
    saveHistory();

    callback(true, display_text);
}
