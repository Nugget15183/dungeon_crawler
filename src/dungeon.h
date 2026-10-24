#pragma once

#include <vector>
#include <random>
#include <algorithm>

namespace dungeon {
    enum Kind : unsigned char { K_FLOOR, K_RING, K_DEEP };
    enum Tile : unsigned char { WALL = 0, FLOOR = 1 };

    struct Rect {
        int x, y, w, h;
        Kind kind;
    };

    struct Room {
        int x, y, w, h;

        int cx() const { return x + w / 2; }
        int cy() const { return y + h / 2; }

        //keeps walls between rooms
        bool overlaps(const Room &o, int pad = 1) const {
            return x - pad < o.x + o.w && x + w + pad > o.x &&
                   y - pad < o.y + o.h && y + h + pad > o.y;
        }
    };

    struct Map {
        int w = 0, h = 0;
        std::vector<Tile> tiles;
        std::vector<Room> rooms;

        bool inBounds(int x, int y) const { return x >= 0 && y >= 0 && x < w && y < h; }

        bool isFloor(int x, int y) const {
            return inBounds(x, y) && tiles[y * w + x] == FLOOR;
        }

        //wall tile with floor touching
        bool touchesFloor(int x, int y) const {
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++)
                    if (isFloor(x + dx, y + dy)) return true;
            return false;
        }

        //the outer ring is never carved so the dungeon always has a wall border
        void carve(int x, int y) {
            if (x > 0 && y > 0 && x < w - 1 && y < h - 1) tiles[y * w + x] = FLOOR;
        }

        void carveRoom(const Room &r) {
            for (int y = r.y; y < r.y + r.h; y++)
                for (int x = r.x; x < r.x + r.w; x++)
                    carve(x, y);
        }

        //width is centered on y
        void carveH(int x1, int x2, int y, int width = 1) {
            int off = width / 2;
            for (int x = std::min(x1, x2); x <= std::max(x1, x2); x++)
                for (int i = 0; i < width; i++)
                    carve(x, y - off + i);
        }

        //width is centered on x
        void carveV(int y1, int y2, int x, int width = 1) {
            int off = width / 2;
            for (int y = std::min(y1, y2); y <= std::max(y1, y2); y++)
                for (int i = 0; i < width; i++)
                    carve(x - off + i, y);
        }
    };

    inline Map generate(int w, int h, int maxRooms, unsigned seed, int hallWidth = 2) {
        Map m;
        m.w = w;
        m.h = h;
        m.tiles.assign(static_cast<size_t>(w) * h, WALL);

        std::mt19937 rng(seed);
        auto rnd = [&](int a, int b) { return std::uniform_int_distribution<int>(a, b)(rng); };

        for (int attempt = 0; attempt < maxRooms * 10 && (int) m.rooms.size() < maxRooms; attempt++) {
            Room r;
            r.w = rnd(4, 9);
            r.h = rnd(4, 9);
            r.x = rnd(1, w - r.w - 1);
            r.y = rnd(1, h - r.h - 1);

            bool bad = false;
            for (const Room &o: m.rooms) {
                if (r.overlaps(o)) {
                    bad = true;
                    break;
                }
            }
            if (bad) continue;

            m.carveRoom(r);

            //connect to prev room
            if (!m.rooms.empty()) {
                const Room &p = m.rooms.back();
                if (rnd(0, 1)) {
                    m.carveH(p.cx(), r.cx(), p.cy(), hallWidth);
                    m.carveV(p.cy(), r.cy(), r.cx(), hallWidth);
                } else {
                    m.carveV(p.cy(), r.cy(), p.cx(), hallWidth);
                    m.carveH(p.cx(), r.cx(), r.cy(), hallWidth);
                }
            }

            m.rooms.push_back(r);
        }

        return m;
    }

    inline std::vector<Rect> greedyMesh(const Map& m, int pad = 0) {
        const int W = m.w + pad * 2;
        const int H = m.h + pad * 2;

        std::vector<Kind> kinds(static_cast<size_t>(W) * H);
        for (int gy = 0; gy < H; gy++) {
            for (int gx = 0; gx < W; gx++) {
                int x = gx - pad, y = gy - pad;
                Kind k = K_DEEP;
                if (m.isFloor(x, y)) k = K_FLOOR;
                else if (m.touchesFloor(x, y)) k = K_RING;
                kinds[gy * W + gx] = k;
            }
        }

        std::vector<char> used(static_cast<size_t>(W) * H, 0);
        std::vector<Rect> out;

        for (int gy = 0; gy < H; gy++) {
            for (int gx = 0; gx < W; gx++) {
                if (used[gy * W + gx]) continue;

                Kind k = kinds[gy * W + gx];
                int w = 1, h = 1;

                if (k != K_RING) {
                    while (gx + w < W && !used[gy * W + gx + w] && kinds[gy * W + gx + w] == k) w++;

                    for (; gy + h < H; h++) {
                        bool ok = true;
                        for (int i = 0; i < w; i++) {
                            if (used[(gy + h) * W + gx + i] || kinds[(gy + h) * W + gx + i] != k) { ok = false; break; }
                        }
                        if (!ok) break;
                    }
                }

                for (int j = 0; j < h; j++)
                    for (int i = 0; i < w; i++)
                        used[(gy + j) * W + gx + i] = 1;

                out.push_back({ gx - pad, gy - pad, w, h, k });
            }
        }

        return out;
    }
}
