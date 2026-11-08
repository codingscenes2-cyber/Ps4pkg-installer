#include "pkgmaster/downloader/archive_extractor.hpp"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <limits>
#ifdef PKGMASTER_HAS_LIBARCHIVE
#include <archive.h>
#include <archive_entry.h>
#endif

namespace pkgmaster {
bool is_safe_archive_entry_path(const std::string& value,std::string& reason) {
    if(value.empty()||value.size()>4096||value.find('\0')!=std::string::npos||
       value.find('\\')!=std::string::npos||value.front()=='/'||value.find(':')!=std::string::npos) {
        reason="Archive entry path is empty, absolute or malformed.";return false;
    }
    std::size_t start=0;
    while(start<value.size()) {
        const auto end=value.find('/',start);
        const auto part=value.substr(start,end==std::string::npos?std::string::npos:end-start);
        if(part.empty()||part=="."||part=="..") {
            reason="Archive entry contains an empty, current or parent path component.";return false;
        }
        start=end==std::string::npos?value.size():end+1;
    }
    reason.clear();return true;
}

ArchiveExtractResult extract_archive_safely(const ArchiveExtractOptions& o) {
    ArchiveExtractResult result;
    if(o.archive_path.empty()||o.destination_directory.empty()||!o.maximum_total_bytes||
       !o.maximum_file_bytes||!o.maximum_files||!o.maximum_expansion_ratio) {
        result.error="Invalid archive extraction options.";return result;
    }
#ifndef PKGMASTER_HAS_LIBARCHIVE
    result.status=ArchiveStatus::Unsupported;
    result.error="This build has no libarchive extraction support.";return result;
#else
    namespace fs=std::filesystem;
    std::error_code ec;
    if(!fs::is_regular_file(o.archive_path,ec)||ec){result.error="Archive source is not a regular file.";return result;}
    const auto archive_size=fs::file_size(o.archive_path,ec);
    if(ec){result.error="Could not determine archive size.";return result;}
    const fs::path destination(o.destination_directory);
    if(fs::exists(destination,ec)||ec){result.error="Extraction destination already exists; use a new staging directory.";return result;}
    if(!fs::create_directories(destination,ec)||ec){result.error="Could not create extraction staging directory.";return result;}
    auto fail=[&](const std::string& message,ArchiveStatus status=ArchiveStatus::Failed){
        std::error_code ignored;fs::remove_all(destination,ignored);result.status=status;result.error=message;return result;
    };
    struct archive* reader=archive_read_new();
    if(!reader)return fail("Could not allocate archive reader.");
    archive_read_support_filter_all(reader);
    archive_read_support_format_all(reader);
    if(!o.password.empty())archive_read_add_passphrase(reader,o.password.c_str());
    if(archive_read_open_filename(reader,o.archive_path.c_str(),10240)!=ARCHIVE_OK) {
        const std::string message=archive_error_string(reader)?archive_error_string(reader):"Could not open archive.";
        archive_read_free(reader);return fail(message);
    }
    struct archive_entry* entry=nullptr;
    while(true) {
        if(o.cancel&&o.cancel->load()){archive_read_close(reader);archive_read_free(reader);return fail("Archive extraction cancelled.",ArchiveStatus::Cancelled);}
        const int next=archive_read_next_header(reader,&entry);
        if(next==ARCHIVE_EOF)break;
        if(next!=ARCHIVE_OK){const std::string message=archive_error_string(reader)?archive_error_string(reader):"Archive header is corrupt.";archive_read_close(reader);archive_read_free(reader);return fail(message);}
        const char* raw=archive_entry_pathname_utf8(entry);
        if(!raw)raw=archive_entry_pathname(entry);
        std::string name=raw?raw:"";
        const auto type=archive_entry_filetype(entry);
        if(type==AE_IFDIR)while(!name.empty()&&name.back()=='/')name.pop_back();
        std::string path_error;
        if(!is_safe_archive_entry_path(name,path_error)){archive_read_close(reader);archive_read_free(reader);return fail(path_error);}
        if(archive_entry_symlink(entry)||archive_entry_hardlink(entry)){archive_read_close(reader);archive_read_free(reader);return fail("Symbolic and hard links are not permitted in archives.");}
        const fs::path target=destination/fs::path(name);
        if(type==AE_IFDIR) {
            fs::create_directories(target,ec);
            if(ec){archive_read_close(reader);archive_read_free(reader);return fail("Could not create archive directory.");}
            continue;
        }
        if(type!=AE_IFREG){archive_read_close(reader);archive_read_free(reader);return fail("Archive contains a special or unsupported file type.");}
        if(result.extracted_files>=o.maximum_files){archive_read_close(reader);archive_read_free(reader);return fail("Archive exceeds the configured file-count limit.");}
        const auto declared=archive_entry_size(entry);
        if(declared<0||static_cast<std::uint64_t>(declared)>o.maximum_file_bytes||
           static_cast<std::uint64_t>(declared)>o.maximum_total_bytes-result.extracted_bytes) {
            archive_read_close(reader);archive_read_free(reader);return fail("Archive entry exceeds configured extraction limits.");
        }
        if(archive_size>0) {
            const long double ratio=static_cast<long double>(result.extracted_bytes+static_cast<std::uint64_t>(declared))/static_cast<long double>(archive_size);
            if(ratio>static_cast<long double>(o.maximum_expansion_ratio)){archive_read_close(reader);archive_read_free(reader);return fail("Archive expansion ratio exceeds the configured limit.");}
        }
        fs::create_directories(target.parent_path(),ec);
        if(ec){archive_read_close(reader);archive_read_free(reader);return fail("Could not create parent directory for archive entry.");}
        if(fs::exists(target,ec)||ec){archive_read_close(reader);archive_read_free(reader);return fail("Archive contains duplicate output paths.");}
        std::ofstream output(target,std::ios::binary|std::ios::out|std::ios::trunc);
        if(!output){archive_read_close(reader);archive_read_free(reader);return fail("Could not create extracted file.");}
        char buffer[65536];std::uint64_t written=0;
        while(true) {
            if(o.cancel&&o.cancel->load()){output.close();archive_read_close(reader);archive_read_free(reader);return fail("Archive extraction cancelled.",ArchiveStatus::Cancelled);}
            const auto n=archive_read_data(reader,buffer,sizeof(buffer));
            if(n==0)break;
            if(n<0){output.close();archive_read_close(reader);archive_read_free(reader);return fail("Archive entry data is corrupt or the password is incorrect.");}
            const auto bytes=static_cast<std::uint64_t>(n);
            if(bytes>static_cast<std::uint64_t>(declared)-written||
               bytes>o.maximum_total_bytes-result.extracted_bytes-written) {
                output.close();archive_read_close(reader);archive_read_free(reader);return fail("Archive entry expanded beyond its declared or configured size.");
            }
            output.write(buffer,n);
            if(!output){output.close();archive_read_close(reader);archive_read_free(reader);return fail("Could not write extracted file.");}
            written+=bytes;
        }
        output.close();
        if(output.fail()||written!=static_cast<std::uint64_t>(declared)){archive_read_close(reader);archive_read_free(reader);return fail("Extracted file length does not match archive metadata.");}
        result.extracted_bytes+=written;++result.extracted_files;
        std::string extension=target.extension().string();
        std::transform(extension.begin(),extension.end(),extension.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
        if(extension==".pkg")++result.detected_pkg_files;
    }
    archive_read_close(reader);archive_read_free(reader);
    result.status=ArchiveStatus::Completed;result.error.clear();return result;
#endif
}
}
