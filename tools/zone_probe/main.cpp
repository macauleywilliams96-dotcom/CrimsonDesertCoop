// cdmp_zone_probe: runs the enemy-zone predicate (common/enemy_load.h) on the
// ActorLoadDataAcks of a sniffer recording and prints each verdict, to see why
// an enemy is or is not a zone enemy on the current game build.
//
//   cdmp_zone_probe <cdmp_packets_*.bin> [--all]
//
// Without --all only the loads with a hostile faction are listed.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <set>
#include <string>
#include <vector>

#include "common/enemy_load.h"

namespace {

constexpr uint16_t kOpActorLoadDataAck = 2005;
constexpr uint8_t kDirServerToClient = 2;

uint32_t u32(const uint8_t* p) {
    uint32_t v;
    std::memcpy(&v, p, 4);
    return v;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: cdmp_zone_probe <cdmp_packets_*.bin> [--all]\n");
        return 1;
    }
    const bool all = argc > 2 && std::string(argv[2]) == "--all";
    std::ifstream f(argv[1], std::ios::binary);
    const std::vector<uint8_t> b((std::istreambuf_iterator<char>(f)), {});
    if (b.size() < 12 || std::memcmp(b.data(), "CDPK", 4) != 0) {
        std::fprintf(stderr, "not a cdmp packet recording\n");
        return 1;
    }
    std::set<uint32_t> seen;
    for (size_t at = 12; at + 24 <= b.size();) {
        const uint8_t dir = b[at + 8];
        uint16_t copied;
        std::memcpy(&copied, b.data() + at + 12, 2);
        const uint8_t* data = b.data() + at + 24;
        at += 24 + copied;
        if (at > b.size() || dir != kDirServerToClient || copied < 0x60) continue;
        uint16_t op;
        std::memcpy(&op, data, 2);
        if (op != kOpActorLoadDataAck) continue;
        const std::span<const uint8_t> load(data + 5, copied - 5);
        if (load[2] != 3) continue;
        const uint32_t key = u32(load.data() + 3), faction = u32(load.data() + 0x1C);
        if (!all && !cdmp::enemy_load::is_table_hostile_faction(faction)) continue;
        if (!seen.insert(key).second) continue;
        const auto c = cdmp::enemy_load::zone_check(load, {});
        const char* v = c.verdict == cdmp::enemy_load::Verdict::Yes            ? "YES"
                        : c.verdict == cdmp::enemy_load::Verdict::FollowParent ? "PARENT"
                                                                               : "no";
        const auto o = cdmp::enemy_load::origin(load);
        std::printf("0x%08X %-24s faction %u  %-6s %-22s origin %c 0x%08X/%u (class %d)\n", key,
                    cdmp::enemy_load::name(load).c_str(), faction, v, c.why, static_cast<char>(o.kind), o.id, o.sub,
                    cdmp::enemy_load::origin_class(o.id));
    }
    return 0;
}
