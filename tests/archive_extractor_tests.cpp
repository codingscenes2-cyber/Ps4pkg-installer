#include "pkgmaster/downloader/archive_extractor.hpp"
#include <cassert>
int main() {
    std::string reason;
    assert(pkgmaster::is_safe_archive_entry_path("game/content.pkg",reason));
    assert(pkgmaster::is_safe_archive_entry_path("folder/subfolder/file.bin",reason));
    assert(!pkgmaster::is_safe_archive_entry_path("",reason));
    assert(!pkgmaster::is_safe_archive_entry_path("../outside.pkg",reason));
    assert(!pkgmaster::is_safe_archive_entry_path("folder/../../outside.pkg",reason));
    assert(!pkgmaster::is_safe_archive_entry_path("/etc/passwd",reason));
    assert(!pkgmaster::is_safe_archive_entry_path("C:/Windows/file",reason));
    assert(!pkgmaster::is_safe_archive_entry_path("folder\\file.pkg",reason));
    assert(!pkgmaster::is_safe_archive_entry_path("folder//file.pkg",reason));
    assert(!pkgmaster::is_safe_archive_entry_path("folder/./file.pkg",reason));
    pkgmaster::ArchiveExtractOptions options;
    auto invalid = pkgmaster::extract_archive_safely(options);
    assert(invalid.status == pkgmaster::ArchiveStatus::Failed);
    return 0;
}
