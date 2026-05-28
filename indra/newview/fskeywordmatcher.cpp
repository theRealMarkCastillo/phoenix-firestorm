// Skip the viewer's precompiled-header chain when building this file into the
// slim Catch2 test target. linden_common.h still provides LL_COMMON_API and the
// fundamental types that llstring.h depends on.
#ifndef FS_KEYWORD_MATCHER_TEST_BUILD
#include "llviewerprecompiledheaders.h"
#else
#include "linden_common.h"
#endif

#include "fskeywordmatcher.h"

#include "llstring.h"   // LLStringUtil::toLower / trim

#include <boost/regex.hpp>

namespace
{
    // Escape regex metacharacters so a literal keyword is matched verbatim inside
    // a \b...\b whole-word pattern. Mirrors the original FSKeywords escaping.
    std::string escapeRegexLiteral(const std::string& token)
    {
        return boost::regex_replace(
            token,
            boost::regex("[.^$|()\\[\\]{}*+?\\\\]"),
            "\\\\&",
            boost::match_default | boost::format_sed);
    }
}

FSKeywordMatcher::FSKeywordMatcher(std::string_view csv_keywords, bool case_sensitive, bool match_whole_words)
    : mCaseSensitive(case_sensitive)
    , mMatchWholeWords(match_whole_words)
{
    std::string s{ csv_keywords };
    if (!mCaseSensitive)
    {
        LLStringUtil::toLower(s);
    }

    const boost::regex comma(",");
    boost::sregex_token_iterator it(s.begin(), s.end(), comma, -1), end;
    for (; it != end; ++it)
    {
        std::string token(*it);
        LLStringUtil::trim(token);

        // Drop empty keywords (e.g. from a trailing comma). An empty keyword
        // would otherwise match every message: substring find("") returns 0 and
        // the whole-word pattern "\b\b" matches almost anywhere.
        if (token.empty())
        {
            continue;
        }

        mWords.push_back(mMatchWholeWords ? escapeRegexLiteral(token) : token);
    }
}

bool FSKeywordMatcher::matches(std::string_view source_view) const
{
    if (mWords.empty())
    {
        return false;
    }

    std::string source{ source_view };
    if (!mCaseSensitive)
    {
        LLStringUtil::toLower(source);
    }

    if (mMatchWholeWords)
    {
        for (const auto& word : mWords)
        {
            if (boost::regex_search(source, boost::regex("\\b" + word + "\\b")))
            {
                return true;
            }
        }
    }
    else
    {
        for (const auto& word : mWords)
        {
            if (source.find(word) != std::string::npos)
            {
                return true;
            }
        }
    }

    return false;
}
