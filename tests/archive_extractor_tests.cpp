#include "pkgmaster/downloader/archive_extractor.hpp"
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#ifdef PKGMASTER_TEST_LIBARCHIVE
#include <archive.h>
#include <archive_entry.h>
static bool make_tar(const std::string& path,bool malicious) {
    archive* writer=archive_write_new();
    if(!writer)return false;
    archive_write_set_format_pax_restricted(writer);
    if(archive_write_open_filename(writer,path.c_str())!=ARCHIVE_OK){archive_write_free(writer);return false;}
    archive_entry* entry=archive_entry_new();
    archive_entry_set_pathname(entry,malicious?"../outside.pkg":"folder/");
    archive_entry_set_filetype(entry,malicious?AE_IFREG:AE_IFDIR);
    archive_entry_set_perm(entry,0755);
    archive_entry_set_size(entry,malicious?3:0);
    bool ok=archive_write_header(writer,entry)==ARCHIVE_OK;
    if(ok&&malicious)ok=archive_write_data(writer,"bad",3)==3;
    archive_entry_free(entry);
    if(ok&&!malicious) {
        entry=archive_entry_new();
        archive_entry_set_pathname(entry,"folder/game.pkg");
        archive_entry_set_filetype(entry,AE_IFREG);
        archive_entry_set_perm(entry,0644);
        archive_entry_set_size(entry,3);
        ok=archive_write_header(writer,entry)==ARCHIVE_OK&&archive_write_data(writer,"PKG",3)==3;
        archive_entry_free(entry);
    }
    ok=archive_write_close(writer)==ARCHIVE_OK&&ok;
    archive_write_free(writer);
    return ok;
}
#endif
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
#ifdef PKGMASTER_TEST_LIBARCHIVE
    const std::string archive_path="pkgmaster-safe-archive-test.tar";
    const std::string output_path="pkgmaster-safe-archive-output";
    const std::string bad_archive="pkgmaster-bad-archive-test.tar";
    std::filesystem::remove_all(output_path);
    assert(make_tar(archive_path,false));
    options.archive_path=archive_path;
    options.destination_directory=output_path;
    auto extracted=pkgmaster::extract_archive_safely(options);
    assert(extracted.status==pkgmaster::ArchiveStatus::Completed);
    assert(extracted.extracted_files==1&&extracted.detected_pkg_files==1&&extracted.extracted_bytes==3);
    std::ifstream pkg(output_path+"/folder/game.pkg",std::ios::binary);
    const std::string contents((std::istreambuf_iterator<char>(pkg)),std::istreambuf_iterator<char>());
    assert(contents=="PKG");
    std::filesystem::remove_all(output_path);
    assert(make_tar(bad_archive,true));
    options.archive_path=bad_archive;
    options.destination_directory=output_path;
    auto rejected=pkgmaster::extract_archive_safely(options);
    assert(rejected.status==pkgmaster::ArchiveStatus::Failed);
    assert(!std::filesystem::exists(output_path));
    assert(!std::filesystem::exists("outside.pkg"));
    std::remove(archive_path.c_str());
    std::remove(bad_archive.c_str());
#endif
    return 0;
}
