#pragma once

#include <vector>
#include <random>
#include <algorithm>

namespace dungeon {

enum Tile : unsigned char { WALL = 0, FLOOR = 1 };

struct Room {
    int x, y, w, h;

    int cx() const { return x + w / 2; }
    int cy() const { return y + h / 2; }

    //keeps walls between rooms
    bool overlaps(const Room& o, int pad = 1) const {
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

    void carveRoom(const Room& r) {
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

    for (int attempt = 0; attempt < maxRooms * 10 && (int)m.rooms.size() < maxRooms; attempt++) {
        Room r;
        r.w = rnd(4, 9);
        r.h = rnd(4, 9);
        r.x = rnd(1, w - r.w - 1);
        r.y = rnd(1, h - r.h - 1);

        bool bad = false;
        for (const Room& o : m.rooms) {
            if (r.overlaps(o)) { bad = true; break; }
        }
        if (bad) continue;

        m.carveRoom(r);

        //connect to prev room
        if (!m.rooms.empty()) {
            const Room& p = m.rooms.back();
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

}