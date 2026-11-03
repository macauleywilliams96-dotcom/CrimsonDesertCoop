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
#include <map>
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
    const bool peers = argc > 2 && std::string(argv[2]) == "--peers";
    std::ifstream f(argv[1], std::ios::binary);
    const std::vector<uint8_t> b((std::istreambuf_iterator<char>(f)), {});
    if (b.size() < 12 || std::memcmp(b.data(), "CDPK", 4) != 0) {
        std::fprintf(stderr, "not a cdmp packet recording\n");
        return 1;
    }
    if (argc > 2 && std::string(argv[2]) == "--hits") {
        // Our DoFrameEventReqs that name an enemy copy (0xB01Fxxxx): the head
        // bytes that differ between them, to find the attack's fields.
        std::vector<std::pair<double, std::vector<uint8_t>>> hits;
        std::vector<uint32_t> last_actions;
        for (size_t at = 12; at + 24 <= b.size();) {
            uint64_t t_us;
            std::memcpy(&t_us, b.data() + at, 8);
            const uint8_t dir = b[at + 8];
            uint16_t copied;
            std::memcpy(&copied, b.data() + at + 12, 2);
            const uint8_t* data = b.data() + at + 24;
            at += 24 + copied;
            if (at > b.size() || dir != 1 || copied < 5 + 12) continue;
            uint16_t op;
            std::memcpy(&op, data, 2);
            if (op != 2202 && copied < 5 + 0x287) continue;
            if (op == 2202 && u32(data + 5) == (argc > 3 ? static_cast<uint32_t>(std::stoul(argv[3], nullptr, 16)) : 0xA0100001u)) {
                last_actions.push_back(u32(data + 5 + 8));
                if (last_actions.size() > 3) last_actions.erase(last_actions.begin());
            }
            if (op != 3030) continue;
            std::vector<uint8_t> p(data + 5, data + copied);
            std::printf("  hit by %08X, our last actions:", u32(&p[1]));
            for (uint32_t a : last_actions) std::printf(" %08X", a);
            std::printf(" (at %.3f)\n", t_us / 1e6);
            bool copy = false;
            for (size_t i = 0x287; i + 4 <= p.size() && i < 0x287 + 8; ++i) copy = copy || (u32(&p[i]) >> 16) == 0xB01F;
            if (copy) hits.push_back({t_us / 1e6, std::move(p)});
        }
        const size_t head = 0x287;
        std::vector<size_t> vary;
        for (size_t i = 5; i < head; ++i) {
            for (const auto& h : hits) {
                if (h.second[i] != hits[0].second[i]) {
                    vary.push_back(i);
                    break;
                }
            }
        }
        std::printf("%zu hits; varying head bytes:", hits.size());
        for (size_t i : vary) std::printf(" %zu", i);
        std::printf("\n");
        for (const auto& [t, p] : hits) {
            std::printf("%9.3f len %zu target %08X |", t, p.size(), u32(&p[head]));
            for (size_t i : vary) std::printf(" %02X", p[i]);
            std::printf("\n");
        }
        return 0;
    }
    if (argc > 2 && std::string(argv[2]) == "--attack-fields") {
        // Every DoFrameEventReq of ours: u32 head fields with few, recurring
        // values (an attack id), with each value's count and first times.
        std::vector<std::pair<double, std::vector<uint8_t>>> hits;
        for (size_t at = 12; at + 24 <= b.size();) {
            uint64_t t_us;
            std::memcpy(&t_us, b.data() + at, 8);
            const uint8_t dir = b[at + 8];
            uint16_t copied;
            std::memcpy(&copied, b.data() + at + 12, 2);
            const uint8_t* data = b.data() + at + 24;
            at += 24 + copied;
            if (at > b.size() || dir != 1 || copied < 5 + 0x287) continue;
            uint16_t op;
            std::memcpy(&op, data, 2);
            if (op == 3030) hits.push_back({t_us / 1e6, std::vector<uint8_t>(data + 5, data + copied)});
        }
        std::printf("%zu hits\n", hits.size());
        for (size_t i = 5; i + 4 <= 0x287; ++i) {
            std::map<uint32_t, std::vector<double>> vals;
            for (const auto& [t, p] : hits) vals[u32(&p[i])].push_back(t);
            if (vals.size() < 2 || vals.size() > 12) continue;
            bool hashy = false;
            for (const auto& [v, ts] : vals) hashy = hashy || ((v >> 24) != 0 && (v >> 24) != 0xFF && (v >> 23) != 0x7F && (v >> 24) != 0x3F && (v >> 24) != 0xBF && (v >> 24) != 0x40 && (v >> 24) != 0xC0);
            if (!hashy) continue;
            std::printf("+%zu:", i);
            for (const auto& [v, ts] : vals) std::printf(" %08X x%zu (%.1f)", v, ts.size(), ts.front() - hits[0].first);
            std::printf("\n");
        }
        return 0;
    }
    if (argc > 3 && std::string(argv[2]) == "--move-bytes") {
        // Our on-foot MoveActorReqs of one actor: each run of Moves whose
        // non-float bytes (0x30-0x44, 0x6C-0xB4, 0xD0-0xEE) differ from the
        // most common pattern, with the bytes, to spot an animation-driven flag.
        const uint32_t who = std::stoul(argv[3], nullptr, 16);
        std::vector<std::pair<double, std::vector<uint8_t>>> mv;
        for (size_t at = 12; at + 24 <= b.size();) {
            uint64_t t_us;
            std::memcpy(&t_us, b.data() + at, 8);
            const uint8_t dir = b[at + 8];
            uint16_t copied;
            std::memcpy(&copied, b.data() + at + 12, 2);
            const uint8_t* data = b.data() + at + 24;
            at += 24 + copied;
            if (at > b.size() || dir != 1 || copied < 5 + 0xEE) continue;
            uint16_t op;
            std::memcpy(&op, data, 2);
            const uint8_t* m = data + 5;
            if (op != 2833 || u32(m) != who || u32(m + 0x44) != 0xFFFFFFFFu) continue;
            mv.push_back({t_us / 1e6, std::vector<uint8_t>(m, m + 0xEE)});
        }
        std::vector<size_t> spots;
        for (size_t i = 0x64; i < 0xB4; ++i) spots.push_back(i);
        for (size_t i = 0xE2; i < 0xEE; ++i) spots.push_back(i);
        std::map<std::vector<uint8_t>, size_t> count;
        auto key = [&](const std::vector<uint8_t>& p) {
            std::vector<uint8_t> k;
            for (size_t i : spots) k.push_back(p[i]);
            return k;
        };
        for (const auto& [t, p] : mv) ++count[key(p)];
        std::printf("%zu moves, %zu patterns\n", mv.size(), count.size());
        std::vector<uint8_t> prev;
        for (const auto& [t, p] : mv) {
            const auto k = key(p);
            if (k == prev) continue;
            prev = k;
            std::printf("%9.3f x%-5zu", t, count[k]);
            for (uint8_t v : k) std::printf(" %02X", v);
            std::printf("\n");
        }
        return 0;
    }
    if (argc > 3 && std::string(argv[2]) == "--moves") {
        // Our MoveActorReqs of one actor: t, local pos (+0x04), world pos
        // (+0xC4), parent (+0x44), whenever the two disagree or a gap/jump.
        const uint32_t who = std::stoul(argv[3], nullptr, 16);
        float last[3] = {};
        double last_t = 0;
        for (size_t at = 12; at + 24 <= b.size();) {
            uint64_t t_us;
            std::memcpy(&t_us, b.data() + at, 8);
            const uint8_t dir = b[at + 8];
            uint16_t copied;
            std::memcpy(&copied, b.data() + at + 12, 2);
            const uint8_t* data = b.data() + at + 24;
            at += 24 + copied;
            if (at > b.size() || dir != 1 || copied < 5 + 0xD0) continue;
            uint16_t op;
            std::memcpy(&op, data, 2);
            const uint8_t* m = data + 5;
            if (u32(m) != who) continue;
            const double t = t_us / 1e6;
            if (op == 2833) {
                float l[3], w[3];
                std::memcpy(l, m + 0x04, 12);
                std::memcpy(w, m + 0xC4, 12);
                const float dl = std::hypot(l[0] - w[0], l[2] - w[2]);
                const float jump = std::hypot(w[0] - last[0], w[2] - last[2]);
                if (dl > 0.3f || t - last_t > 0.1 || jump > 0.4f) {
                    std::printf("%9.3f move local (%.2f %.2f %.2f) world (%.2f %.2f %.2f) parent %08X gap %.3f jump %.2f\n",
                                t, l[0], l[1], l[2], w[0], w[1], w[2], u32(m + 0x44), t - last_t, jump);
                }
                std::memcpy(last, w, 12);
                last_t = t;
            } else if (op == 2005 || copied > 300) {
                std::printf("%9.3f op %u len %u\n", t, op, copied - 5);
            }
        }
        return 0;
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
        if (peers && argc > 3 && key == std::stoul(argv[3], nullptr, 16)) {
            const size_t from = std::stoul(argv[4]), to = std::min<size_t>(std::stoul(argv[5]), load.size());
            for (size_t j = from; j < to; ++j) std::printf("%s%02X", (j - from) % 16 ? " " : "\n  ", load[j]);
            std::printf("\n");
        }
        if (peers) {
            // Other actors of the same key block named inside this load.
            for (size_t i = 8; i + 4 <= load.size(); ++i) {
                const uint32_t k = u32(load.data() + i);
                if (k == key || (k & 0xFFFFF000u) != (key & 0xFFFFF000u)) continue;
                std::printf("    0x%08X at +%zu:", k, i);
                for (size_t j = i < 16 ? 0 : i - 16; j < i + 12 && j < load.size(); ++j) std::printf(" %02X", load[j]);
                std::printf("\n");
            }
        }
    }
    return 0;
}
