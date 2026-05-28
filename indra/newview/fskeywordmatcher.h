#ifndef FS_KEYWORDMATCHER_H
#define FS_KEYWORDMATCHER_H

#include <string>
#include <string_view>
#include <vector>

// Matches chat text against a user-configured keyword list. Extracted from
// FSKeywords so the parse / escape / match logic can be unit-tested without the
// viewer's settings system. Pure: depends only on std + boost::regex + llcommon
// string helpers; touches no globals.
//
// Construction parses a comma-separated keyword string and pre-processes each
// keyword (trim; lowercase if !case_sensitive; regex-escape if whole-words).
// matches() then tests a source string under the same rules.
class FSKeywordMatcher
{
public:
    FSKeywordMatcher() = default;
    FSKeywordMatcher(std::string_view csv_keywords, bool case_sensitive, bool match_whole_words);

    // True if `source` contains any configured keyword per the configured rules.
    bool matches(std::string_view source) const;

    bool empty() const { return mWords.empty(); }
    std::size_t size() const { return mWords.size(); }

private:
    // Trimmed, lowercased (if !mCaseSensitive), and regex-escaped (if
    // mMatchWholeWords) keywords. Empty tokens are dropped at construction.
    std::vector<std::string> mWords;
    bool mCaseSensitive{ false };
    bool mMatchWholeWords{ false };
};

#endif // FS_KEYWORDMATCHER_H
