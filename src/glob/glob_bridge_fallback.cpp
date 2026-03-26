/**
 * Glob Bridge Fallback Implementation
 *
 * Simple filesystem-based glob expansion when the full aglob engine
 * is not available. Supports basic patterns: *, **.
 *
 * Copyright (c) 2025-2026 Aria Language Project
 */

#include "glob/glob_bridge.hpp"
#include <algorithm>
#include <regex>
#include <set>

namespace aria::make::glob {

// Simple wildcard-to-regex converter for basic patterns
static std::string pattern_to_regex(const std::string& pattern) {
    std::string result;
    for (size_t i = 0; i < pattern.size(); i++) {
        char c = pattern[i];
        if (c == '*') {
            if (i + 1 < pattern.size() && pattern[i + 1] == '*') {
                // ** matches everything including /
                result += ".*";
                i++; // skip second *
                // Skip trailing / after **
                if (i + 1 < pattern.size() && pattern[i + 1] == '/') {
                    result += "/?";
                    i++;
                }
            } else {
                // * matches everything except /
                result += "[^/]*";
            }
        } else if (c == '?') {
            result += "[^/]";
        } else if (c == '[') {
            // Pass through character classes
            result += '[';
            i++;
            if (i < pattern.size() && pattern[i] == '!') {
                result += '^';
                i++;
            }
            while (i < pattern.size() && pattern[i] != ']') {
                result += pattern[i];
                i++;
            }
            if (i < pattern.size()) result += ']';
        } else if (c == '.' || c == '(' || c == ')' || c == '+' ||
                   c == '{' || c == '}' || c == '^' || c == '$' || c == '|') {
            result += '\\';
            result += c;
        } else {
            result += c;
        }
    }
    return result;
}

GlobResult expand_pattern(
    const fs::path& base_dir,
    const std::string& pattern,
    const GlobOptions& options)
{
    GlobResult result;

    if (!fs::exists(base_dir) || !fs::is_directory(base_dir)) {
        result.error = GlobError::INVALID_BASE_DIR;
        result.error_message = "Base directory does not exist: " + base_dir.string();
        return result;
    }

    std::string regex_str = pattern_to_regex(pattern);

    try {
        std::regex re(regex_str, options.case_sensitive
            ? std::regex::ECMAScript
            : (std::regex::ECMAScript | std::regex::icase));

        bool recursive = pattern.find("**") != std::string::npos;

        auto iterate = [&](auto& iter) {
            for (const auto& entry : iter) {
                if (options.files_only && !entry.is_regular_file()) continue;
                if (!options.include_hidden) {
                    auto fname = entry.path().filename().string();
                    if (!fname.empty() && fname[0] == '.') continue;
                }

                // Get path relative to base_dir
                auto rel = fs::relative(entry.path(), base_dir).string();
                if (std::regex_match(rel, re)) {
                    result.paths.push_back(entry.path().string());
                }
            }
        };

        if (recursive) {
            auto iter = fs::recursive_directory_iterator(
                base_dir,
                options.follow_symlinks
                    ? fs::directory_options::follow_directory_symlink
                    : fs::directory_options::none);
            iterate(iter);
        } else {
            auto iter = fs::directory_iterator(base_dir);
            iterate(iter);
        }

    } catch (const std::filesystem::filesystem_error& e) {
        result.error = GlobError::FILESYSTEM_ERROR;
        result.error_message = e.what();
        return result;
    } catch (const std::regex_error& e) {
        result.error = GlobError::PATTERN_SYNTAX_ERROR;
        result.error_message = e.what();
        return result;
    }

    std::sort(result.paths.begin(), result.paths.end());
    return result;
}

GlobResult expand_patterns(
    const fs::path& base_dir,
    const std::vector<std::string>& patterns,
    const GlobOptions& options)
{
    GlobResult combined;
    std::set<std::string> seen;

    for (const auto& pat : patterns) {
        auto r = expand_pattern(base_dir, pat, options);
        if (!r.ok()) return r;
        for (auto& p : r.paths) {
            if (seen.insert(p).second) {
                combined.paths.push_back(std::move(p));
            }
        }
    }

    std::sort(combined.paths.begin(), combined.paths.end());
    return combined;
}

bool path_matches(
    const fs::path& path,
    const std::string& pattern,
    bool case_sensitive)
{
    std::string regex_str = pattern_to_regex(pattern);
    try {
        std::regex re(regex_str, case_sensitive
            ? std::regex::ECMAScript
            : (std::regex::ECMAScript | std::regex::icase));
        return std::regex_match(path.string(), re);
    } catch (...) {
        return false;
    }
}

bool validate_pattern(const std::string& pattern) {
    int bracket_depth = 0;
    for (size_t i = 0; i < pattern.size(); i++) {
        if (pattern[i] == '[') bracket_depth++;
        else if (pattern[i] == ']') {
            if (bracket_depth > 0) bracket_depth--;
            else return false; // unmatched ]
        }
    }
    return bracket_depth == 0;
}

const char* error_string(GlobError error) {
    switch (error) {
        case GlobError::OK: return "OK";
        case GlobError::INVALID_BASE_DIR: return "Invalid base directory";
        case GlobError::PATTERN_SYNTAX_ERROR: return "Pattern syntax error";
        case GlobError::ACCESS_DENIED: return "Access denied";
        case GlobError::FILESYSTEM_ERROR: return "Filesystem error";
        case GlobError::SYMLINK_CYCLE: return "Symlink cycle detected";
        case GlobError::MAX_DEPTH_EXCEEDED: return "Maximum depth exceeded";
        case GlobError::UNKNOWN_ERROR: return "Unknown error";
        default: return "Unknown error";
    }
}

} // namespace aria::make::glob
