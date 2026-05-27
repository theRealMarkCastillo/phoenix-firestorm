/**
 * @file fsfloateraiagent.h
 * @brief In-viewer AI agent chat floater
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

#ifndef FS_FLOATERAIAGENT_H
#define FS_FLOATERAIAGENT_H

#include "llfloater.h"

class LLButton;
class LLLineEditor;
class LLTextEditor;

class FSFloaterAIAgent : public LLFloater
{
public:
    FSFloaterAIAgent(const LLSD& key);
    ~FSFloaterAIAgent() = default;

    bool postBuild() override;
    void onOpen(const LLSD& key) override;
    void onClose(bool app_quitting) override;
    void draw() override;

    // Public so FSAIAgent can push proactive/location messages
    void appendMessage(const std::string& label, const std::string& text);

private:
    void onSendClicked();
    void onInputCommit();
    void submitQuery();
    void setWaiting(bool waiting);

    LLTextEditor* mChatHistory       = nullptr;
    LLLineEditor* mUserInput         = nullptr;
    LLButton*     mSendButton        = nullptr;
    bool          mNeedsFocusRestore = false;
};

#endif // FS_FLOATERAIAGENT_H
