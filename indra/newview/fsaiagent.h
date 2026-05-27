/**
 * @file fsaiagent.h
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

#ifndef FS_AIAGENT_H
#define FS_AIAGENT_H

#include "llsingleton.h"
#include "llsd.h"
#include "llcorehttputil.h"
#include "llcoros.h"
#include "llviewerparcelmgr.h"
#include <boost/signals2.hpp>
#include <functional>
#include <string>
#include <vector>

class FSAIAgent : public LLSingleton<FSAIAgent>, public LLParcelObserver
{
    LLSINGLETON(FSAIAgent);
    ~FSAIAgent();

public:
    struct Message { std::string role; std::string content; };
    using response_callback_t = std::function<void(bool success, const std::string& text)>;

    static constexpr int MAX_CHAT_LINES = 20;

    // Core query — send user text, receive assistant reply via callback
    void query(const std::string& user_text, response_callback_t callback);
    bool isBusy() const { return mBusy; }

    // Feed nearby chat lines into the context buffer
    void addNearbyChatLine(const std::string& speaker, const std::string& text);

    // History management
    void clearHistory();
    void saveHistory();
    void loadHistory();

    // Observer lifecycle — call from floater onOpen / destructor
    void initObservers();
    void cleanupObservers();

    // LLParcelObserver — fires on parcel change
    void changed() override;

    // Region change handler
    void onRegionChanged();

    // Post a message directly to the AI Agent floater if it is open
    void notifyFloater(const std::string& label, const std::string& text);

private:
    std::string buildSystemPrompt() const;
    void        queryCoro(const std::string& user_text, response_callback_t callback);

    // Strip [SAY: text] blocks from response, speak them in local chat, return cleaned text
    static std::string extractAndSpeak(const std::string& text);

    std::vector<Message>      mHistory;
    std::vector<std::string>  mRecentChat;

    bool        mBusy            = false;
    bool        mHistoryLoaded   = false;
    bool        mObserversInited = false;
    std::string mLastParcelName;
    std::string mLastRegionName;

    boost::signals2::connection mRegionChangedConnection;
};

#endif // FS_AIAGENT_H
