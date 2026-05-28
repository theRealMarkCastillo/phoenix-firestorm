/**
 * @file fskeywords.cpp
 *
 * $LicenseInfo:firstyear=2011&license=fsviewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (C) 2011, The Phoenix Firestorm Project, Inc.
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

#include "fscommon.h"
#include "fskeywords.h"
#include "growlmanager.h"
#include "llagent.h"
#include "llchat.h"
#include "llinstantmessage.h"
#include "llmutelist.h"
#include "llui.h"
#include "llviewercontrol.h"

FSKeywords::FSKeywords()
{
    gSavedPerAccountSettings.getControl("FSKeywords")->getSignal()->connect(boost::bind(&FSKeywords::updateKeywords, this));
    gSavedPerAccountSettings.getControl("FSKeywordCaseSensitive")->getSignal()->connect(boost::bind(&FSKeywords::updateKeywords, this));
    gSavedPerAccountSettings.getControl("FSKeywordMatchWholeWords")->getSignal()->connect(boost::bind(&FSKeywords::updateKeywords, this));
    updateKeywords();
}

void FSKeywords::updateKeywords()
{
    // The matcher bakes in the case-sensitivity and whole-word settings at build
    // time; it is rebuilt whenever any of these settings change (see the signal
    // connections in the constructor).
    const bool case_sensitive   = gSavedPerAccountSettings.getBOOL("FSKeywordCaseSensitive");
    const bool match_whole_words = gSavedPerAccountSettings.getBOOL("FSKeywordMatchWholeWords");
    const std::string csv        = gSavedPerAccountSettings.getString("FSKeywords");
    mMatcher = FSKeywordMatcher(csv, case_sensitive, match_whole_words);
}

bool FSKeywords::chatContainsKeyword(const LLChat& chat, bool is_local)
{
    // Don't check if message is from us - unless it's a radar notification
    if (chat.mFromID == gAgentID && chat.mFromName != SYSTEM_FROM)
    {
        return false;
    }

    static LLCachedControl<bool> sFSKeywordOn(gSavedPerAccountSettings, "FSKeywordOn", false);
    static LLCachedControl<bool> sFSKeywordInChat(gSavedPerAccountSettings, "FSKeywordInChat", false);
    static LLCachedControl<bool> sFSKeywordInIM(gSavedPerAccountSettings, "FSKeywordInIM", false);

    if (!sFSKeywordOn ||
        (is_local && !sFSKeywordInChat) ||
        (!is_local && !sFSKeywordInIM))
    {
        return false;
    }

    static LLCachedControl<bool> sFSKeywordSpeakersName(gSavedPerAccountSettings, "FSKeywordSpeakersName", false);

    const std::string source = sFSKeywordSpeakersName
        ? (chat.mFromName + " " + chat.mText)
        : chat.mText;

    return mMatcher.matches(source);
}

// <FS:PP> FIRE-10178: Keyword Alerts in group IM do not work unless the group is in the foreground
void FSKeywords::notify(const LLChat& chat)
{
    if ((chat.mFromID != gAgentID || chat.mFromName == SYSTEM_FROM) && !chat.mMuted && !LLMuteList::getInstance()->isMuted(chat.mFromID))
    {
        static LLCachedControl<bool> PlayModeUISndFSKeywordSound(gSavedPerAccountSettings, "PlayModeUISndFSKeywordSound");
        if (PlayModeUISndFSKeywordSound)
        {
            LLUI::getInstance()->mAudioCallback(LLUUID(gSavedPerAccountSettings.getString("UISndFSKeywordSound")));
        }

        static LLCachedControl<bool> FSEnableGrowl(gSavedSettings, "FSEnableGrowl");
        if (FSEnableGrowl)
        {
            std::string msg = chat.mFromName;
            if (FSCommon::is_irc_me_prefix(chat.mText))
            {
                msg = msg + chat.mText.substr(3);
            }
            else
            {
                msg = msg + ": " + chat.mText;
            }
            GrowlManager::notify("Keyword Alert", msg, GROWL_KEYWORD_ALERT_TYPE);
        }
    }
}
// </FS:PP>
