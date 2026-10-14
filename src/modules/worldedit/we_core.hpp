#pragma once
#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace we {

struct Pos { int x = 0, y = 0, z = 0; };
using Box = std::pair<Pos, Pos>;

struct Region {
    Pos a, b;
    bool hasA = false, hasB = false;
    bool ready() const { return hasA && hasB; }
    Pos lo() const { return {std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z)}; }
    Pos hi() const { return {std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z)}; }
    long long volume() const {
        Pos l = lo(), h = hi();
        return 1LL * (h.x - l.x + 1) * (h.y - l.y + 1) * (h.z - l.z + 1);
    }
};

inline std::vector<Box> split(const Region& r, int maxBlocks = 32768) {
    std::vector<Box> out;
    Pos l = r.lo(), h = r.hi();
    int sx = std::min(h.x - l.x + 1, 32);
    int sz = std::min(h.z - l.z + 1, 32);
    int sy = std::max(1, maxBlocks / (sx * sz));
    for (int y = l.y; y <= h.y; y += sy)
        for (int z = l.z; z <= h.z; z += sz)
            for (int x = l.x; x <= h.x; x += sx)
                out.push_back({{x, y, z}, {std::min(x + sx - 1, h.x),
                                           std::min(y + sy - 1, h.y),
                                           std::min(z + sz - 1, h.z)}});
    return out;
}

inline std::vector<std::string> fillCommands(const Region& r, const std::string& block) {
    std::vector<std::string> cmds;
    for (auto& [p, q] : split(r))
        cmds.push_back("/fill " + std::to_string(p.x) + " " + std::to_string(p.y) + " " +
                       std::to_string(p.z) + " " + std::to_string(q.x) + " " +
                       std::to_string(q.y) + " " + std::to_string(q.z) + " " + block);
    return cmds;
}

inline bool isWeCommand(const std::string& s) { return s.rfind("//", 0) == 0; }

}  // namespace we
