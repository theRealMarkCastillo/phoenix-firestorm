/**
 * @file fsfloateraiagent.cpp
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

#include "llviewerprecompiledheaders.h"
#include "fsfloateraiagent.h"

#include "fsaiagent.h"
#include "llbutton.h"
#include "lllineeditor.h"
#include "lltexteditor.h"
#include "llviewercontrol.h"

FSFloaterAIAgent::FSFloaterAIAgent(const LLSD& key)
    : LLFloater(key)
{
}

bool FSFloaterAIAgent::postBuild()
{
    mChatHistory = getChild<LLTextEditor>("chat_history");
    mUserInput   = getChild<LLLineEditor>("user_input");
    mSendButton  = getChild<LLButton>("send_btn");

    mSendButton->setClickedCallback([this](LLUICtrl*, const LLSD&) { onSendClicked(); });
    mUserInput->setCommitCallback([this](LLUICtrl*, const LLSD&) { onInputCommit(); });
    mUserInput->setCommitOnFocusLost(false);

    mChatHistory->setReadOnly(true);
    mChatHistory->setWordWrap(true);
    mChatHistory->setMaxTextLength(0x7fffffff); // effectively unlimited for chat history

    return true;
}

void FSFloaterAIAgent::onOpen(const LLSD& key)
{
    FSAIAgent::instance().initObservers();
    FSAIAgent::instance().loadHistory();
    mNeedsFocusRestore = true;
}

void FSFloaterAIAgent::onClose(bool app_quitting)
{
    FSAIAgent::instance().cleanupObservers();
    LLFloater::onClose(app_quitting);
}

void FSFloaterAIAgent::draw()
{
    LLFloater::draw();
    if (mNeedsFocusRestore && mUserInput && mUserInput->getEnabled())
    {
        mUserInput->setFocus(true);
        mNeedsFocusRestore = false;
    }
}

void FSFloaterAIAgent::onSendClicked()
{
    submitQuery();
}

void FSFloaterAIAgent::onInputCommit()
{
    submitQuery();
}

void FSFloaterAIAgent::submitQuery()
{
    std::string text = mUserInput->getText();
    if (text.empty() || FSAIAgent::instance().isBusy())
        return;

    mUserInput->clear();
    appendMessage("You", text);
    setWaiting(true);

    FSAIAgent::instance().query(text, [this](bool success, const std::string& response)
    {
        setWaiting(false);
        if (success)
            appendMessage("Assistant", response);
        else
            appendMessage("Error", response);
    });
}

void FSFloaterAIAgent::appendMessage(const std::string& label, const std::string& text)
{
    if (!mChatHistory) return;

    // Move cursor to end and append.
    mChatHistory->setCursorAndScrollToEnd();
    std::string entry = label + ": " + text + "\n\n";
    mChatHistory->appendText(entry, false);
}

void FSFloaterAIAgent::setWaiting(bool waiting)
{
    mSendButton->setEnabled(!waiting);
    mUserInput->setEnabled(!waiting);

    if (waiting)
    {
        appendMessage("Assistant", "Thinking...");
    }
    else
    {
        // Defer focus restore to the next draw frame so it lands after
        // all coroutine/UI state has fully unwound.
        mNeedsFocusRestore = true;
    }
}
