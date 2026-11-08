#include "pkgmaster/downloader/range_file_assembler.hpp"
#include "pkgmaster/core/crc32.hpp"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>

int main() {
    const std::string destination = "pkgmaster-range-assembly-test.pkg";
    const std::string staging = destination + ".part";
    std::remove(destination.c_str());
    std::remove(staging.c_str());

    const auto plan = pkgmaster::make_range_plan(10, 2, 1);
    pkgmaster::RangeFileAssembler assembler;
    std::string error;
    assert(assembler.create(destination, plan, 100, false, error));
    assert(!assembler.complete());

    const std::string second = "FGHIJ";
    const std::string first = "ABCDE";
    assert(!assembler.write_chunk(0, first.data(), 4, error));
    assert(assembler.write_chunk(1, second.data(), second.size(), error));
    assert(assembler.chunk_complete(1));
    assert(!assembler.complete());
    assert(!assembler.finalize(error));
    assert(assembler.write_chunk(0, first.data(), first.size(), error));
    assert(!assembler.write_chunk(0, first.data(), first.size(), error));
    assert(assembler.complete());
    assert(assembler.finalize(error));

    std::ifstream input(destination, std::ios::binary);
    const std::string actual((std::istreambuf_iterator<char>(input)),
                             std::istreambuf_iterator<char>());
    assert(actual == "ABCDEFGHIJ");
    std::remove(destination.c_str());


    // A validated manifest can reopen a same-sized staging file and continue
    // only ranges that were not checkpointed as complete.
    const auto recovery_plan = pkgmaster::make_range_plan(10, 2, 1);
    {
        pkgmaster::RangeFileAssembler interrupted;
        assert(interrupted.create(destination, recovery_plan, 100, false, error));
        assert(interrupted.write_chunk(1, second.data(), second.size(), error));
    }
    pkgmaster::RangeFileAssembler recovered;
    assert(recovered.open_existing(destination, recovery_plan, 100, false,
                                   std::vector<bool>{false, true}, error));
    assert(recovered.chunk_complete(1));
    assert(recovered.verify_chunk_checksum(1, pkgmaster::crc32(second.data(), second.size()), error));
    assert(!recovered.chunk_complete(0));
    assert(recovered.write_chunk(0, first.data(), first.size(), error));
    assert(recovered.complete());
    assert(recovered.finalize(error));
    std::ifstream recovered_input(destination, std::ios::binary);
    const std::string recovered_data((std::istreambuf_iterator<char>(recovered_input)),
                                     std::istreambuf_iterator<char>());
    assert(recovered_data == "ABCDEFGHIJ");
    std::remove(destination.c_str());

    // A pre-existing staging file may contain recoverable data; never truncate it.
    { std::ofstream old(staging, std::ios::binary); old << "keep"; }
    pkgmaster::RangeFileAssembler protected_file;
    assert(!protected_file.create(destination, plan, 100, false, error));
    std::ifstream preserved(staging, std::ios::binary);
    std::string sentinel;
    preserved >> sentinel;
    assert(sentinel == "keep");
    std::remove(staging.c_str());

    // Invalid plans must be rejected before creating any file.
    pkgmaster::RangePlan broken = plan;
    broken.parts[1].first = 4;
    pkgmaster::RangeFileAssembler invalid;
    assert(!invalid.create(destination, broken, 100, false, error));
    assert(!std::ifstream(staging).good());
    return 0;
}
