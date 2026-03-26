// test_state_manager.cpp — Unit tests for aria_make StateManager
// Part of aria_make - Aria Build System

#include "../include/state/state_manager.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
#include <string>

namespace fs = std::filesystem;

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) \
    do { \
        tests_run++; \
        std::cout << "  " << #name << "... "; \
        try { \
            test_##name(); \
            tests_passed++; \
            std::cout << "PASS" << std::endl; \
        } catch (const std::exception& e) { \
            std::cout << "FAIL: " << e.what() << std::endl; \
        } catch (...) { \
            std::cout << "FAIL: unknown exception" << std::endl; \
        } \
    } while(0)

#define ASSERT(cond) \
    do { if (!(cond)) throw std::runtime_error("Assertion failed: " #cond); } while(0)

// Helper: create a temp directory for tests
static fs::path make_temp_dir() {
    auto tmp = fs::temp_directory_path() / "aria_make_test";
    fs::create_directories(tmp);
    return tmp;
}

// Helper: write a file with content
static void write_file(const fs::path& path, const std::string& content) {
    std::ofstream out(path);
    out << content;
}

// Helper: cleanup temp dir
static void cleanup(const fs::path& dir) {
    if (fs::exists(dir)) {
        fs::remove_all(dir);
    }
}

// =========================================================================
// Test: construction and empty state
// =========================================================================
void test_construction() {
    auto tmp = make_temp_dir() / "test_construction";
    fs::create_directories(tmp);

    aria::make::StateManager sm(tmp);
    ASSERT(!sm.has_state());
    ASSERT(sm.target_count() == 0);

    cleanup(tmp);
}

// =========================================================================
// Test: load returns false when no state file exists
// =========================================================================
void test_load_no_file() {
    auto tmp = make_temp_dir() / "test_load_no_file";
    fs::create_directories(tmp);

    aria::make::StateManager sm(tmp);
    sm.load();
    // Should succeed (empty state) or return false — either way, no crash
    ASSERT(sm.target_count() == 0);

    cleanup(tmp);
}

// =========================================================================
// Test: save and load round-trip
// =========================================================================
void test_save_load_roundtrip() {
    auto tmp = make_temp_dir() / "test_roundtrip";
    fs::create_directories(tmp);

    // Create a fake source file
    auto src = tmp / "test.aria";
    write_file(src, "func main() -> NIL:\n    print(\"hello\")\nend\n");

    // Create a fake output
    auto output = tmp / "test_binary";
    write_file(output, "fake binary");

    {
        aria::make::StateManager sm(tmp);
        sm.update_record(
            "test_target",
            output,
            {src.string()},
            {},  // resolved deps
            {},  // implicit deps
            {"-O2"},
            100  // build duration ms
        );
        ASSERT(sm.target_count() == 1);
        ASSERT(sm.save());
    }

    // Reload in new instance
    {
        aria::make::StateManager sm2(tmp);
        sm2.load();
        ASSERT(sm2.has_state());
        ASSERT(sm2.target_count() == 1);

        auto record = sm2.get_record("test_target");
        ASSERT(record.has_value());
    }

    cleanup(tmp);
}

// =========================================================================
// Test: dirty detection — missing artifact
// =========================================================================
void test_dirty_missing_artifact() {
    auto tmp = make_temp_dir() / "test_dirty_missing";
    fs::create_directories(tmp);

    auto src = tmp / "test.aria";
    write_file(src, "func main() -> NIL:\n    print(\"hello\")\nend\n");

    aria::make::StateManager sm(tmp);
    // No prior record → should be dirty
    auto reason = sm.check_dirty(
        "new_target",
        tmp / "nonexistent_output",
        {src.string()},
        {"-O2"}
    );
    ASSERT(reason != aria::make::DirtyReason::CLEAN);

    cleanup(tmp);
}

// =========================================================================
// Test: dirty detection — source changed
// =========================================================================
void test_dirty_source_changed() {
    auto tmp = make_temp_dir() / "test_dirty_source";
    fs::create_directories(tmp);

    auto src = tmp / "test.aria";
    auto output = tmp / "test_binary";
    write_file(src, "func main() -> NIL:\n    print(\"v1\")\nend\n");
    write_file(output, "fake binary");

    aria::make::StateManager sm(tmp);
    sm.update_record("target", output, {src.string()}, {}, {}, {"-O2"}, 50);

    // Should be clean now
    auto reason1 = sm.check_dirty("target", output, {src.string()}, {"-O2"});
    ASSERT(reason1 == aria::make::DirtyReason::CLEAN);

    // Modify source
    write_file(src, "func main() -> NIL:\n    print(\"v2\")\nend\n");
    sm.invalidate_hash_cache(src);

    auto reason2 = sm.check_dirty("target", output, {src.string()}, {"-O2"});
    ASSERT(reason2 == aria::make::DirtyReason::SOURCE_CHANGED);

    cleanup(tmp);
}

// =========================================================================
// Test: dirty detection — flags changed
// =========================================================================
void test_dirty_flags_changed() {
    auto tmp = make_temp_dir() / "test_dirty_flags";
    fs::create_directories(tmp);

    auto src = tmp / "test.aria";
    auto output = tmp / "test_binary";
    write_file(src, "func main() -> NIL:\n    print(\"hello\")\nend\n");
    write_file(output, "fake binary");

    aria::make::StateManager sm(tmp);
    sm.update_record("target", output, {src.string()}, {}, {}, {"-O2"}, 50);

    // Same flags → clean
    auto reason1 = sm.check_dirty("target", output, {src.string()}, {"-O2"});
    ASSERT(reason1 == aria::make::DirtyReason::CLEAN);

    // Different flags → dirty
    auto reason2 = sm.check_dirty("target", output, {src.string()}, {"-O0", "-g"});
    ASSERT(reason2 == aria::make::DirtyReason::FLAGS_CHANGED);

    cleanup(tmp);
}

// =========================================================================
// Test: clear state
// =========================================================================
void test_clear() {
    auto tmp = make_temp_dir() / "test_clear";
    fs::create_directories(tmp);

    auto src = tmp / "test.aria";
    auto output = tmp / "test_binary";
    write_file(src, "func main() -> NIL:\n    print(\"hello\")\nend\n");
    write_file(output, "fake binary");

    aria::make::StateManager sm(tmp);
    sm.update_record("target", output, {src.string()}, {}, {}, {}, 50);
    ASSERT(sm.target_count() == 1);

    sm.clear();
    ASSERT(sm.target_count() == 0);
    ASSERT(!sm.has_state());

    cleanup(tmp);
}

// =========================================================================
// Test: invalidate specific target
// =========================================================================
void test_invalidate() {
    auto tmp = make_temp_dir() / "test_invalidate";
    fs::create_directories(tmp);

    auto src = tmp / "test.aria";
    auto output = tmp / "test_binary";
    write_file(src, "func main() -> NIL:\n    print(\"hello\")\nend\n");
    write_file(output, "fake binary");

    aria::make::StateManager sm(tmp);
    sm.update_record("target", output, {src.string()}, {}, {}, {}, 50);
    ASSERT(sm.target_count() == 1);

    sm.invalidate("target");
    ASSERT(sm.target_count() == 0);

    cleanup(tmp);
}

// =========================================================================
// Test: hash_flags deterministic
// =========================================================================
void test_hash_flags_deterministic() {
    auto h1 = aria::make::StateManager::hash_flags({"-O2", "-Wall"});
    auto h2 = aria::make::StateManager::hash_flags({"-O2", "-Wall"});
    ASSERT(h1 == h2);

    auto h3 = aria::make::StateManager::hash_flags({"-O0"});
    ASSERT(h1 != h3);
}

// =========================================================================
// Test: hash_flags order matters
// =========================================================================
void test_hash_flags_order_matters() {
    auto h1 = aria::make::StateManager::hash_flags({"-O2", "-Wall"});
    auto h2 = aria::make::StateManager::hash_flags({"-Wall", "-O2"});
    // Different order should produce different hash (flags are order-sensitive)
    ASSERT(h1 != h2);
}

// =========================================================================
// Test: multiple targets
// =========================================================================
void test_multiple_targets() {
    auto tmp = make_temp_dir() / "test_multi";
    fs::create_directories(tmp);

    auto src1 = tmp / "a.aria";
    auto src2 = tmp / "b.aria";
    auto out1 = tmp / "a_bin";
    auto out2 = tmp / "b_bin";
    write_file(src1, "// a");
    write_file(src2, "// b");
    write_file(out1, "bin_a");
    write_file(out2, "bin_b");

    aria::make::StateManager sm(tmp);
    sm.update_record("target_a", out1, {src1.string()}, {}, {}, {"-O2"}, 10);
    sm.update_record("target_b", out2, {src2.string()}, {}, {}, {"-O1"}, 20);

    ASSERT(sm.target_count() == 2);
    ASSERT(sm.get_record("target_a").has_value());
    ASSERT(sm.get_record("target_b").has_value());
    ASSERT(!sm.get_record("target_c").has_value());

    cleanup(tmp);
}

// =========================================================================
// Test: build stats
// =========================================================================
void test_build_stats() {
    auto tmp = make_temp_dir() / "test_stats";
    fs::create_directories(tmp);

    aria::make::StateManager sm(tmp);
    (void)sm.get_stats();
    // Should be zero or default initialized
    // Just verify it doesn't crash
    sm.reset_stats();
    (void)sm.get_stats();

    cleanup(tmp);
}

// =========================================================================
// Main
// =========================================================================
int main() {
    std::cout << "=== StateManager Tests ===" << std::endl;

    // Clean up any leftovers
    cleanup(fs::temp_directory_path() / "aria_make_test");

    TEST(construction);
    TEST(load_no_file);
    TEST(save_load_roundtrip);
    TEST(dirty_missing_artifact);
    TEST(dirty_source_changed);
    TEST(dirty_flags_changed);
    TEST(clear);
    TEST(invalidate);
    TEST(hash_flags_deterministic);
    TEST(hash_flags_order_matters);
    TEST(multiple_targets);
    TEST(build_stats);

    std::cout << std::endl;
    std::cout << tests_passed << "/" << tests_run << " tests passed" << std::endl;

    // Cleanup
    cleanup(fs::temp_directory_path() / "aria_make_test");

    return (tests_passed == tests_run) ? 0 : 1;
}
