#include "engine.h"

#include <string>
#include <cmath>
#include <algorithm>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "stb_image.h"

#include "util.h"
#include "dungeon.h"
#include <random>
#include <utility>

struct TitleState {
    bool inGuide = false;
    button* bgn = nullptr;
    button* guide = nullptr;
    button* back = nullptr;
    std::vector<quad*> guideText;
};

using namespace util;
using namespace logger;

//random class
static std::mt19937 rng{std::random_device{}()};
static float rnd(float a, float b) { return std::uniform_real_distribution(a, b)(rng); }

//global vars
//declare things here instead of using magic numbers
namespace {
    //title screen
    const float titleBobAmp = 0.05f;

    //camera
    const float camFollow = 6.0f;
    vec2 camFocus = {0,0};
    vec2 camNDC = {0,0};
    float sYOFF = 0.1f;
    const float feetDrop = 0.1f;

    //plr
    const int maxPlrHealth = 5;
    const float hurtTime = 0.5f; //iframes after hit
    const float knockbackSpeed = 6.0f;
    int plrHealth = maxPlrHealth;
    float hurtTimer = 0.0f;
    vec2 plrNDC = {0,0};
    vec2 plrKnock = {0,0};
    bool dead = false;

    const float bulletSpeed = 6.0f;
    const float bulletLife = 1.2f;
    const float bulletRad = 0.1f;
    const float bulletLift = 0.12f;
    const vec2 bulletSize = {0.08f, 0.08f};
    const float shootCooldown = 0.3f;
    const int bulletDamage = 1;

    //slimes
    const int sHealth = 3;
    const vec2 sSize = {0.25f, 0.25f};
    const float sRad = 0.25f; //half width
    const float sViewRange = 7.0f;
    const float sWindup = 0.3f;
    const float sAirTime = 0.3f;
    const float sLandTime = 0.1f;
    const float sHopTime = sWindup + sAirTime + sLandTime;
    const float sHopDist = 0.9f;
    const float sHopPause = 0.35f;
    const float sReach = 0.4f;//dist from the plr to hit them
    const float sJumpHeight = 0.08f;//jump arc height
    const int sBase = 2;
    const int sPerLevel = 1;
    const int maxS = 12;

    //world
    const float floorY = -3.5f; //center of the floor blocks
    const float floorTop = floorY + 0.5f;
    const int wallH = 3; //in blocks
    const int hallWidth = 2; //in tiles
    const int fillPad = 30;
    const int baseRooms = 3;
    const int roomsPerLevel = 1;//rooms added each level
    const int maxRooms = 10;
    const int baseMapSize = 26;//26x26
    const int maxMapSize = 44;
    const int mapGrowth = 2;//per level
    const float levelTextTime = 3.6f;
    const float descendTime = 0.6f;
    int level = 0;
    bool descending = false;
    double descendStart = 0;
    vec2 descendFrom = {0,0};
    std::vector<object*> levelObjs;
    std::vector<quad*> levelText;

    //player
    const float walkSpeed = 2.0f;
    const float sprintSpeed = 2.5f;
    const float playerRadius = 0.125f; //collision half width

    //spawn intro (times are in seconds since Begin was pressed)
    const float introFallStart = 0.9f; //player starts falling
    const float introFallTime = 0.7f;
    const float introFallFrom = 1.3f; //screen y the player falls from
    const float introLandT = introFallStart + introFallTime;
    const float introCaveStart = 2.3f; //rocks start falling and the hole closes
    const float introCaveTime = 1.2f;
    const float introEnd = 3.9f; //player gets control
    const float holeY = 0.6f; //where the hole in the ceiling is on screen

    //death screen
    const float deathStartDelay = 0.5f;
    const float deathLetterGap = 0.35f;
    const float deathLetterTime = 0.45f;
    const float deathStartScale = 4.0f;

    std::shared_ptr<TitleState> titleUI;
    decltype(engine::state) menuState{};
    decltype(engine::lighting_setting) menuLighting{};

    bool deathButtonsShown = false;
    double deathStart = 0;
    std::vector<quad*> deathText;
    button* deathRetry = nullptr;
    button* deathMenu = nullptr;

    //things the title screen made
    std::vector<object*> titleObjs;
    std::vector<quad*> titleQuads;
    vector<quad*> hpText;

    //dungeon + player
    dungeon::Map dungeonMap;
    bool inDungeon = false;
    vec2 playerPos = {0, 0}; //where the player stands on the floor, x = world x, y = world z
    vec2 camShake = {0, 0}; //world units, added on top of the camera follow
    quad* playerSprite = nullptr;
    light* playerLight = nullptr; //follows the camera
    bool facingLeft = false;

    //intro state
    bool introActive = false;
    bool introLanded = false;
    bool introRubble = false;
    double introStart = 0;
    light* beamLight = nullptr; //the light shining down through the hole

    std::vector<quad*> introRocks; //rocks falling
    bool deathActive = false;

    const float fadeAlpha = 0.25f; //min alpha of walls
    const float fadeSpeed = 8.0f; //fade speed
    std::vector<std::vector<object*>> wallColumns; //y * w + x
    std::vector<object*> floorBlocks;
    std::vector<float> columnAlpha;
    std::vector<int> fadedTiles;

    bool uiButtonsExist = true;

    struct slime {
        int hp = sHealth;
        float hitTimer = 0.0f;
        quad* sprite = nullptr;
        vec2 pos = {0, 0};//world x, z
        vec2 hopDir = {0, 0};
        float hopT = 0.0f;
        float restT = 0.0f;
        bool hopping = false;
    };
    std::vector<slime> slimes;

    struct bullet {
        quad* sprite = nullptr;
        vec2 pos = {0, 0};   //world x, z
        vec2 vel = {0, 0};
        float life = 0.0f;
    };
    std::vector<bullet> bullets;
    float shootTimer = 0.0f;
    vec2 plrAim = {0, 1};

    vec2 pHPDisplayPos = {-1.0f,.9f};
    int lastHealth = maxPlrHealth;
}

//forward decls
void updateDeathScreen(engine* e);
void startDeathScreen(engine* e);

std::vector<quad*> StrToUI(unsigned int fontAtlas, vec2 pos, std::string str, float scale = 1.0f, int z=0) {
    std::vector<quad*> ret;
    int x = 0;
    float sc = 0.1f * scale;
    float spacing = 0.05f * scale;

    float startX = pos.x - 0.5f * (str.size() - 1) * spacing;

    for (const char& c : str) {
        int baseIndex = static_cast<unsigned char>(c);

        int col = baseIndex % 16;
        int row = baseIndex / 16;

        quad* q = new quad({startX + (float)x * spacing, pos.y}, {sc, sc}, tileUV(col, row));
        q->z=z;
        q->create(fontAtlas);
        ret.push_back(q);

        x++;
    }

    return ret;
}

void showLevelText(engine* e, int lvl) {
    for (auto q : StrToUI(e->m_font_atlas_2, {0, 0.85f}, "Level " + std::to_string(lvl), 1.5f, 5)) {
        q->col = {1, 1, 1, 0};
        q->onUpdate = [](quad* self) -> void {
            float age = (float)(glfwGetTime() - self->creationTime);
            float fadeIn = std::clamp(age / 0.5f, 0.0f, 1.0f);
            float fadeOut = std::clamp((levelTextTime - age) / 0.7f, 0.0f, 1.0f);
            self->col.a = fadeIn * fadeOut;
        };
        e->Renderer->AddToRQ_2D(*q);
        levelText.push_back(q);
    }
}

void framebuffer_size_callback(GLFWwindow* win, int width, int height) {
    engine* eng = static_cast<engine*>(glfwGetWindowUserPointer(win));

    eng->setWindowSize(new ivec2(width, height));
}

void spawnParticles(renderer* r, vec2 origin, vec2 direction, vec2 dir_variance, vec2 accel, vec4 colStart, vec4 colEnd, vec2 sizeStart, vec2 sizeEnd, vec2 maxLife, int count) {
    for (int i = 0; i < count; i++) {
        Particle p;
        p.pos     = origin;
        p.vel     = direction + vec2(rnd(dir_variance.x, dir_variance.y), rnd(dir_variance.x, dir_variance.y));
        p.accel   = accel;
        p.col     = colStart;
        p.colEnd  = colEnd;
        p.size    = rnd(sizeStart.x, sizeStart.y);
        p.sizeEnd = rnd(sizeEnd.x, sizeEnd.y);
        p.maxLife = rnd(maxLife.x, maxLife.y);
        r->EmitParticle(p);
    }
}

vec2 safeNorm(vec2 vec) {
    if (length(vec)<=0) return vec2(0,0);
    return normalize(vec);
}

bool canStandR(float x, float z, float r, bool avoidHole = false) {
    for (float dx : {-r,r}) {
        for (float dz : {-r, r}) {
            int tx = (int)std::floor(x + dx);
            int tz = (int)std::floor(z + dz);
            if (!dungeonMap.isFloor(tx, tz)) return false;
            if (avoidHole && dungeonMap.isHole(tx, tz)) return false;
        }
    }
    return true;
}

bool lineOfSight(vec2 a, vec2 b) {
    int steps = (int)(distance(a, b) / 0.25f) + 1;
    for (int i = 0; i < steps; i++) {
        vec2 p = mix(a, b, (float)i / steps);
        if (!dungeonMap.isFloor((int)std::floor(p.x), (int)std::floor(p.y))) return false;
    }
    return true;
}

int spriteZ(float ndcY) {
    return -1000 + (int)((1.0f - ndcY) * 100.0f);
}

vec2 projGround(engine* e, vec2 w, bool* valid = nullptr) {
    if (valid) *valid = false;

    int ww, wh;
    glfwGetWindowSize(e->getWindow(), &ww, &wh);
    if (ww <= 0 || wh <= 0) return vec2(0, 0);

    camera& c = e->Renderer->cam;
    vec4 clip = c.proj((float)ww / (float)wh) * c.view() * vec4(w.x, floorTop, w.y, 1.0f);
    if (clip.w <= 0.0f) return vec2(0, 0);

    if (valid) *valid = true;
    return vec2(clip.x, clip.y) / clip.w;
}

void syncCamera(engine* e, bool snap = false) {
    camera& c = e->Renderer->cam;
    float delta = (float)std::min(e->getDelta(), 0.1);

    //where a fixed ground point is on screen before the camera moves
    vec2 before = projGround(e, playerPos);

    if (snap) camFocus = playerPos;
    else camFocus += (playerPos - camFocus) * (1.0f - std::exp(-camFollow * delta));

    vec3 f = c.foward();
    float t = (floorTop - c.pos.y) / f.y;

    c.pos.x = camFocus.x - f.x * t + camShake.x;
    c.pos.z = camFocus.y - f.z * t + camShake.y;

    vec2 after = projGround(e, playerPos);
    if (!snap) e->Renderer->MoveParticles(after - before);
    plrNDC = after;
}

void fireBullet(engine* e) {
    bullet b;
    b.pos = playerPos + plrAim * 0.2f;
    b.vel = plrAim * bulletSpeed;
    b.life = bulletLife;

    b.sprite = new quad({0, 0}, bulletSize, tileUV(3, 0));
    b.sprite->create(e->m_texture_atlas);
    b.sprite->worldSpace = true;
    b.sprite->visible = false;
    e->Renderer->AddToRQ_2D(*b.sprite);

    bullets.push_back(b);
}

void updateBullets(engine* e, float delta) {
    for (auto& b : bullets) {
        b.life -= delta;

        //move in small steps so a fast ball can't skip through a wall or a slime
        vec2 move = b.vel * delta;
        int sub = (int)(length(move) / 0.1f) + 1;
        vec2 stepv = move / (float)sub;

        for (int i = 0; i < sub && b.life > 0.0f; i++) {
            b.pos += stepv;

            //walls
            if (!dungeonMap.isFloor((int)std::floor(b.pos.x), (int)std::floor(b.pos.y))) {
                b.life = 0.0f;
                break;
            }

            //slimes
            for (auto& s : slimes) {
                if (s.hp <= 0 || distance(b.pos, s.pos) > sRad + bulletRad) continue;

                s.hp -= bulletDamage;
                s.hitTimer = 0.15f;
                s.hopping = false;       //getting hit interrupts a hop
                s.restT = 0.3f;
                s.sprite->setFrame(0);

                vec2 kb = safeNorm(b.vel) * 0.15f;
                if (canStandR(s.pos.x + kb.x, s.pos.y + kb.y, sRad, true)) s.pos += kb;

                b.life = 0.0f;
                break;
            }
            if (b.life <= 0.0f) break;
        }
    }

    //remove spent
    for (size_t i = bullets.size(); i-- > 0;) {
        if (bullets[i].life > 0.0f) continue;

        e->Renderer->RemoveFromRQ_2D(*bullets[i].sprite);
        bullets[i].sprite->destroy();
        delete bullets[i].sprite;
        bullets.erase(bullets.begin() + i);
    }

    //remove dead
    for (size_t i = slimes.size(); i-- > 0;) {
        if (slimes[i].hp > 0) continue;

        spawnParticles(
            e->Renderer,
            projGround(e, slimes[i].pos) + vec2(0, 0.03f),
            vec2(0.0f, 0.1f),
            vec2(-.5f, .5f),
            vec2(0, -1.0f),
            vec4(0.4f, 0.8f, 0.3f, 1.0f),
            vec4(0.1f, 0.3f, 0.1f, 0.0f),
            vec2(0.02f, 0.04f),
            vec2(0.0f, 0.01f),
            vec2(0.3f, 0.6f),
            12
        );

        e->Renderer->RemoveFromRQ_2D(*slimes[i].sprite);
        slimes[i].sprite->destroy();
        delete slimes[i].sprite;
        slimes.erase(slimes.begin() + i);
    }
}

void placeSprites(engine* e) {
    playerSprite->pos = {plrNDC.x, plrNDC.y + sYOFF};
    playerSprite->z = spriteZ(plrNDC.y);

    for (auto& s : slimes) {
        bool seen = lineOfSight(playerPos, s.pos);

        bool projected = false;
        vec2 n = seen ? projGround(e, s.pos, &projected) : vec2(0, 0);
        bool onScreen = projected && std::abs(n.x) < 1.3f && std::abs(n.y) < 1.3f;

        s.sprite->visible = seen && onScreen;
        if (!s.sprite->visible) continue;

        float air = (s.hopT - sWindup) / sAirTime;
        float arc = (s.hopping && air > 0.0f && air < 1.0f) ? std::sin(air * 3.14159f) * sJumpHeight : 0.0f;

        s.sprite->pos = {n.x, n.y + sSize.y * 0.5f - feetDrop + arc};
        s.sprite->z = spriteZ(n.y);
    }

    for (auto& b : bullets) {
        bool ok = false;
        vec2 n = projGround(e, b.pos, &ok);

        b.sprite->visible = ok && std::abs(n.x) < 1.3f && std::abs(n.y) < 1.3f;
        if (!b.sprite->visible) continue;

        b.sprite->pos = {n.x, n.y + bulletLift};
        b.sprite->z = spriteZ(n.y);
    }
}

void spawnSlimes(engine* e) {
    if (dungeonMap.rooms.size() < 2) return;

    int count = std::min(sBase + (level - 1) * sPerLevel, maxS);
    for (int i = 0; i < count; i++) {
        const dungeon::Room& r = dungeonMap.rooms[std::uniform_int_distribution<size_t>(1, dungeonMap.rooms.size() - 1)(rng)];

        for (int tries = 0; tries < 10; tries++) {
            vec2 p = {rnd(r.x + 0.5f, r.x + r.w - 0.5f), rnd(r.y + 0.5f, r.y + r.h - 0.5f)};
            if (!canStandR(p.x, p.y, sRad, true) || distance(p, playerPos) < 5.0f) continue;

            slime s;
            s.pos = p;
            s.restT = rnd(0.2f, 0.8f); //so they dont all hop in sync

            s.sprite = new quad({0, 0}, sSize, tileUV(0, 3));
            s.sprite->create(e->m_texture_atlas);
            s.sprite->worldSpace = true;
            for (int f = 0; f <= 4; f++) s.sprite->addFrame(f, 3);
            s.sprite->visible = false;
            e->Renderer->AddToRQ_2D(*s.sprite);

            slimes.push_back(s);
            break;
        }
    }
}

void updateEnemies(engine* e, float delta) {
    for (auto& s : slimes) {
        s.hitTimer = std::max(0.0f, s.hitTimer - delta);
        s.sprite->col = s.hitTimer > 0.0f ? vec4(1.0f, 0.4f, 0.4f, 1.0f) : vec4(1.0f);

        vec2 toPlayer = playerPos - s.pos;
        float dist = length(toPlayer);

        if (s.hopping) {
            s.hopT += delta;
            float t = s.hopT;

            int frame;
            if (t < sWindup) frame = (int)(t / sWindup * 3.0f);
            else if (t < sWindup + sAirTime) frame = 3;
            else frame = 4;

            if (t >= sWindup && t < sWindup + sAirTime) {
                vec2 step = s.hopDir * (sHopDist / sAirTime * delta);
                if (canStandR(s.pos.x + step.x, s.pos.y, sRad, true)) s.pos.x += step.x;
                if (canStandR(s.pos.x, s.pos.y + step.y, sRad, true)) s.pos.y += step.y;
            }

            if (t >= sHopTime) {
                s.hopping = false;
                s.restT = sHopPause;
                s.sprite->setFrame(0);
            } else {
                s.sprite->setFrame(frame);
            }
        } else {
            s.sprite->setFrame(0);
            s.restT -= delta;

            if (s.restT <= 0.0f) {
                if (dist < sViewRange && lineOfSight(s.pos, playerPos)) {
                    s.hopDir = safeNorm(toPlayer);
                    s.hopT = 0.0f;
                    s.hopping = true;
                } else {
                    s.restT = 0.2f;
                }
            }
        }

        //attack
        if (dist < sReach && hurtTimer <= 0.0f) {
            plrHealth--;
            hurtTimer = hurtTime;
            plrKnock = safeNorm(toPlayer) * knockbackSpeed;
            if (plrHealth <= 0) dead = true;

            s.hopping = false;
            s.restT = 0.8f;
            s.sprite->setFrame(0);
        }
    }

    //keep slimes from stacking on top of each other
    for (size_t i = 0; i < slimes.size(); i++) {
        for (size_t j = i + 1; j < slimes.size(); j++) {
            vec2 d = slimes[i].pos - slimes[j].pos;
            float l = length(d);
            if (l >= sRad * 2.0f || l < 0.0001f) continue;

            vec2 push = d / l * ((sRad * 2.0f - l) * 0.5f);
            if (canStandR(slimes[i].pos.x + push.x, slimes[i].pos.y + push.y, sRad, true)) slimes[i].pos += push;
            if (canStandR(slimes[j].pos.x - push.x, slimes[j].pos.y - push.y, sRad, true)) slimes[j].pos -= push;
        }
    }
}

void buildDungeon(engine* e, const dungeon::Map& map) {
    wallColumns.assign((size_t)map.w * map.h, std::vector<object*>());
    floorBlocks.assign((size_t)map.w * map.h, nullptr);
    columnAlpha.assign((size_t)map.w * map.h, 1.0f);
    fadedTiles.clear();

    auto addBox = [&](vec3 pos, vec3 size, auto uv) -> object* {
        object* o = new object(pos, size, uv);
        o->create(e->m_texture_atlas);
        e->Renderer->AddToRQ_M(*o);
        levelObjs.push_back(o);
        return o;
    };

    const float upperH = (float)(wallH - 1);
    const float upperCenterY = floorY + 0.5f + upperH * 0.5f;

    for (const dungeon::Rect& r : dungeon::greedyMesh(map, fillPad)) {
        float cx = r.x + r.w * 0.5f;
        float cz = r.y + r.h * 0.5f;

        if (r.kind == dungeon::K_FLOOR) {
            addBox({cx, floorY, cz}, {(float)r.w, 1.0f, (float)r.h}, tileUV(0, 0));
        } else if (r.kind == dungeon::K_RING) {
            int idx = r.y * map.w + r.x;
            floorBlocks[idx] = addBox({cx, floorY, cz}, {1.0f, 1.0f, 1.0f}, tileUV(0, 0));
            wallColumns[idx].push_back(addBox({cx, upperCenterY, cz}, {1.0f, upperH, 1.0f}, tileUV(0, 0)));
        } else {
            addBox({cx, floorY, cz}, {(float)r.w, 1.0f, (float)r.h}, tileUV(0, 0));
            addBox({cx, upperCenterY, cz}, {(float)r.w, upperH, (float)r.h}, tileUV(0, 0));
        }
    }
}

void clearLevel(engine* e) {
    for (object* o : levelObjs) {
        e->Renderer->RemoveFromRQ_M(*o);
        o->destroy();
        delete o;
    }
    levelObjs.clear();
    wallColumns.clear();
    floorBlocks.clear();
    columnAlpha.clear();
    fadedTiles.clear();

    for (quad* q : levelText) {
        e->Renderer->RemoveFromRQ_2D(*q);
        q->destroy();
        delete q;
    }
    levelText.clear();

    for (auto& b : bullets) {
        e->Renderer->RemoveFromRQ_2D(*b.sprite);
        b.sprite->destroy();
        delete b.sprite;
    }
    bullets.clear();

    for (auto& s : slimes) {
        e->Renderer->RemoveFromRQ_2D(*s.sprite);
        s.sprite->destroy();
        delete s.sprite;
    }
    slimes.clear();
}

//makes walls between the player and the camera transparent
void updateWallFade(engine* e) {
    float delta = (float)std::min(e->getDelta(), 0.1);
    camera& c = e->Renderer->cam;
    float wallTop = floorY + wallH - 0.5f;

    float yaw = radians(c.yaw);
    vec2 up(std::cos(yaw), std::sin(yaw));
    vec2 right(-std::sin(yaw), std::cos(yaw));

    std::vector<int> blocked;
    for (float a : {-0.5f, 0.0f, 0.5f}) {
        for (float b : {-0.35f, 0.0f, 0.35f}) {
            vec2 p2 = playerPos + up * a + right * b;
            vec3 target(p2.x, floorTop, p2.y);

            const int steps = 24;
            for (int i = 0; i <= steps; i++) {
                vec3 p = mix(c.pos, target, (float)i / steps);
                if (p.y > wallTop || p.y < floorTop) continue;//above the walls or under the floor

                int tx = (int)std::floor(p.x);
                int tz = (int)std::floor(p.z);
                if (!dungeonMap.inBounds(tx, tz) || dungeonMap.isFloor(tx, tz)) continue;

                int idx = tz * dungeonMap.w + tx;
                if (wallColumns[idx].empty()) continue;
                if (std::find(blocked.begin(), blocked.end(), idx) == blocked.end()) blocked.push_back(idx);
            }
        }
    }

    for (int idx : blocked) {
        if (std::find(fadedTiles.begin(), fadedTiles.end(), idx) == fadedTiles.end()) fadedTiles.push_back(idx);
    }

    for (size_t i = 0; i < fadedTiles.size();) {
        int idx = fadedTiles[i];
        bool isBlocked = std::find(blocked.begin(), blocked.end(), idx) != blocked.end();

        float target = isBlocked ? fadeAlpha : 1.0f;
        float step = fadeSpeed * delta;
        float a = columnAlpha[idx] + std::clamp(target - columnAlpha[idx], -step, step);

        columnAlpha[idx] = a;
        for (auto o : wallColumns[idx]) o->alpha = a;

        if (object* fb = floorBlocks[idx]) {
            float k = (1.0f - a) / (1.0f - fadeAlpha);
            fb->alpha = 1.0f - k;
            fb->visible = fb->alpha > 0.01f;
        }

        if (a >= 1.0f) {
            fadedTiles[i] = fadedTiles.back();
            fadedTiles.pop_back();
        } else {
            i++;
        }
    }
}

void hideButton(button* b) {
    b->visible = false;
    for (auto t : b->textQuads) t->visible = false;
}

void showButton(button* b) {
    b->visible = true;
    for (auto t : b->textQuads) t->visible = true;
}

//rocks in intro
void spawnRubble(engine* e) {
    for (int i = 0; i < 10; i++) {
        float ang = rnd(0.0f, 6.28f);
        float dist = rnd(0.6f, 1.5f);
        float x = playerPos.x + std::cos(ang) * dist;
        float z = playerPos.y + std::sin(ang) * dist;
        if (!canStandR(x, z, playerRadius)) continue;//only land on the floor

        float s = rnd(0.12f, 0.3f);
        float landY = floorTop + s * 0.5f;

        //creationTime is used as the random fall delay
        object* o = new object({x, 0.5f, z}, {s, s, s}, tileUV(0, 0), rnd(0.0f, 0.5f));
        o->rot = {rnd(0.0f, 3.14f), rnd(0.0f, 3.14f), rnd(0.0f, 3.14f)};
        o->visible = false;
        o->create(e->m_texture_atlas);

        o->onUpdate = [e, landY, vy = 0.0f](object* self) mutable -> void {
            if (glfwGetTime() < self->creationTime || self->pos.y <= landY) return;

            float delta = (float)e->getDelta();
            self->visible = true;
            vy += 9.8f * delta;
            self->pos.y = std::max(self->pos.y - vy * delta, landY);
        };

        e->Renderer->AddToRQ_M(*o);
        levelObjs.push_back(o);
    }
}

//intro anim
void createIntro(engine* e) {
    if (!beamLight) {
        beamLight = new light(object({0,0,0}, {.5f,.5f,.5f}, tileUV(0,0)));
        beamLight->visible = false;
        beamLight->color = {1.0f, 0.95f, 0.8f};
        beamLight->radius = 6.0f;
        e->Renderer->AddLight(*beamLight);
    }
    beamLight->pos = vec3(playerPos.x, floorY + 2.0f, playerPos.y);
    beamLight->intensity = 0.0f;
    beamLight->enabled = true;
}

//update function but only for intro
void updateIntro(engine* e) {
    float t = (float)(glfwGetTime() - introStart);

    //sun getting cutoff
    float closing = std::clamp((t - introCaveStart) / introCaveTime, 0.0f, 1.0f);
    float beam = std::clamp(t / 0.8f, 0.0f, 1.0f) * (1.0f - closing);
    beamLight->intensity = 1.5f * beam;

    //player falling
    if (t >= introFallStart) {
        float s = std::clamp((t - introFallStart) / introFallTime, 0.0f, 1.0f);
        playerSprite->visible = true;
        sYOFF = introFallFrom * (1.0f - s * s) + 0.1f;//speeds up as it falls
    }

    //landing
    if (!introLanded && t >= introLandT) {
        introLanded = true;

        spawnParticles(
            e->Renderer,
            projGround(e, playerPos) + vec2(0, -.1f),
            vec2(0.0f, 0.1f),
            vec2(-.8f, .8f),
            vec2(0, -0.5f),
            vec4(0.41f, 0.39f, 0.32f, 1.0f),
            vec4(0.08f, 0.08f, 0.04f, 0.0f),
            vec2(0.02f, 0.04f),
            vec2(0.0f, 0.01f),
            vec2(0.4f, 0.9f),
            30
        );

        for (auto q : hpText) {
            q->visible = true;
        }
    }

    //camera shake
    float shake = 0.0f;
    if (t >= introLandT) shake = 0.12f * std::exp(-(t - introLandT) * 8.0f);
    if (t >= introCaveStart && t < introCaveStart + introCaveTime) shake = std::max(shake, 0.03f);
    camShake = shake > 0.001f ? vec2(rnd(-shake, shake), rnd(-shake, shake)) : vec2(0, 0);

    //cave in
    if (t >= introCaveStart) {
        if (!introRubble) {
            introRubble = true;
            spawnRubble(e);
        }

        //dust shaking
        if (rnd(0.0f, 1.0f) < 0.5f) {
            spawnParticles(
                e->Renderer,
                {rnd(-2.0f, 2.0f), 2.0f},
                vec2(0.0f, -0.2f),
                vec2(-.1f, .1f),
                vec2(0, -0.3f),
                vec4(0.41f, 0.39f, 0.32f, 1.0f),
                vec4(0.08f, 0.08f, 0.04f, 0.0f),
                vec2(0.02f, 0.1f),
                vec2(0.0f, 0.01f),
                vec2(0.3f, 0.6f),
                1
            );
        }

        //rocks falling
        for (size_t i = 0; i < introRocks.size(); i++) {
            float dt = t - (introCaveStart + i * 0.07f);
            float y = holeY - 0.5f * 2.5f * dt * dt;

            introRocks[i]->pos.y = y;
            introRocks[i]->visible = dt >= 0 && y > -0.2f;
        }

        //the players light
        playerLight->intensity = std::clamp((t - (introCaveStart + 0.6f)) / 1.0f, 0.0f, 1.0f);
    }

    //give the player control
    if (t >= introEnd) {
        beamLight->enabled = false;
        playerLight->intensity = 1.0f;
        sYOFF = 0.1f;
        camShake = {0, 0};
        introActive = false;
    }
}

void loadLevel(engine* e, int lvl) {
    level = lvl;
    clearLevel(e);

    int rooms = std::min(baseRooms + (lvl - 1) * roomsPerLevel, maxRooms);
    int size = std::min(baseMapSize + (lvl - 1) * mapGrowth, maxMapSize);

    dungeonMap = dungeon::generate(size, size, rooms, std::random_device{}(), hallWidth);
    buildDungeon(e, dungeonMap);
    inDungeon = true;

    const dungeon::Room& first = dungeonMap.rooms.front();
    playerPos = {first.cx() + 0.5f, first.cy() + 0.5f};
    camShake = {0, 0};

    syncCamera(e, true);

    //reset plr stats
    if (lvl == 1) plrHealth = maxPlrHealth;
    hurtTimer = 0.0f;
    plrKnock = {0, 0};
    dead = false;
    sYOFF = 0.1f;
    playerSprite->col = vec4(1.0f);
    shootTimer = 0.0f;
    float aimYaw = radians(e->Renderer->cam.yaw);
    plrAim = {-std::sin(aimYaw), std::cos(aimYaw)};//start aiming screen right

    spawnSlimes(e);

    //spawn intro
    createIntro(e);
    introStart = glfwGetTime();
    introActive = true;
    introLanded = false;
    introRubble = false;
    descending = false;
    e->isMoving = false;
    playerSprite->visible = false;
    playerLight->intensity = 0.0f;

    showLevelText(e, lvl);

    e->lighting_setting = DUNGEON;
    e->state = IN_GAME;
}

void startGame(engine* e) {
    //hide the title screen
    for (auto q : titleQuads) q->visible = false;
    for (auto o : titleObjs) o->visible = false;

    loadLevel(e, 1);
}

void updateDescent(engine* e) {
    float s = std::clamp((float)(glfwGetTime() - descendStart) / descendTime, 0.0f, 1.0f);

    vec2 center = {dungeonMap.holeX + 0.5f, dungeonMap.holeY + 0.5f};
    playerPos = mix(descendFrom, center, std::min(s * 3.0f, 1.0f));
    sYOFF = 0.1f - s * s * 0.6f;
    playerLight->intensity = 1.0f - s;

    if (s >= 1.0f) loadLevel(e, level + 1);
}

void handle_input(engine* e) {
    GLFWwindow* win = e->getWindow();

    if (glfwGetKey(win, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(win, true);

    if (e->state!=IN_GAME) return;

    if (deathActive) {
        updateDeathScreen(e);
        return;
    }

    //no movement while the intro plays
    if (introActive) {
        updateIntro(e);
        syncCamera(e);
        placeSprites(e);
        return;
    }

    if (descending) {
        updateDescent(e);
        syncCamera(e);
        placeSprites(e);
        return;
    }

    float delta = (float)std::min(e->getDelta(), 0.1);//clamp so a hitch cant push you through a wall
    float speed = walkSpeed;

    if (glfwGetKey(win, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) speed = sprintSpeed;

    vec2 walkDir = {0,0};
    if (glfwGetKey(win, GLFW_KEY_W) == GLFW_PRESS) walkDir.x += 1;
    if (glfwGetKey(win, GLFW_KEY_S) == GLFW_PRESS) walkDir.x -= 1;
    if (glfwGetKey(win, GLFW_KEY_A) == GLFW_PRESS) walkDir.y -= 1;
    if (glfwGetKey(win, GLFW_KEY_D) == GLFW_PRESS) walkDir.y += 1;

    e->isMoving = length(walkDir)>0;
    walkDir=safeNorm(walkDir);

    if (walkDir.y < 0) facingLeft = true;
    else if (walkDir.y > 0) facingLeft = false;

    if (e->isMoving) {
        float yaw = radians(e->Renderer->cam.yaw);
        vec2 up(std::cos(yaw), std::sin(yaw));
        vec2 right(-std::sin(yaw), std::cos(yaw));
        vec2 step = (up * walkDir.x + right * walkDir.y) * (speed * delta);

        plrAim = safeNorm(up * walkDir.x + right * walkDir.y);

        if (canStandR(playerPos.x + step.x, playerPos.y, playerRadius)) playerPos.x += step.x;
        if (canStandR(playerPos.x, playerPos.y + step.y, playerRadius)) playerPos.y += step.y;

        spawnParticles(
            e->Renderer,
            projGround(e, playerPos) + vec2(0, -.05f),//at the player's feet
            vec2(walkDir.y, walkDir.x)*vec2(-1,-1)+vec2(0.0,0.2f),
            vec2(-.5f, .5f),
            vec2(0, -1.0f),
            vec4(0.61f, 0.59f, 0.52f, 1.0f),
            vec4(0.08f, 0.08f, 0.04f, 1.0f),
            vec2(0.01f, 0.01f),
            vec2(0.0f, 0.01f),
            vec2(0.1f, 0.3f),
            1
        );
    }

    //knockback from hits
    if (length(plrKnock) > 0.01f) {
        vec2 kb = plrKnock * delta;
        if (canStandR(playerPos.x + kb.x, playerPos.y, playerRadius)) playerPos.x += kb.x;
        if (canStandR(playerPos.x, playerPos.y + kb.y, playerRadius)) playerPos.y += kb.y;
        plrKnock *= std::exp(-8.0f * delta);
    } else {
        plrKnock = {0, 0};
    }

    //blink red
    hurtTimer = std::max(0.0f, hurtTimer - delta);
    bool blink = hurtTimer > 0.0f && ((int)(glfwGetTime() * 16.0) % 2 == 0);
    playerSprite->col = blink ? vec4(1.0f, 0.3f, 0.3f, 1.0f) : vec4(1.0f);

    shootTimer = std::max(0.0f, shootTimer - delta);
    if (glfwGetKey(win, GLFW_KEY_E) == GLFW_PRESS && shootTimer <= 0.0f) {
        shootTimer = shootCooldown;
        fireBullet(e);
    }

    updateBullets(e, delta);
    updateEnemies(e, delta);

    if (dead) {startDeathScreen(e); return;}

    //stepping on the hole = jump down
    if (dungeonMap.isHole((int)std::floor(playerPos.x), (int)std::floor(playerPos.y))) {
        descending = true;
        descendStart = glfwGetTime();
        descendFrom = playerPos;
        e->isMoving = false;
    }

    syncCamera(e);
    placeSprites(e);
    updateWallFade(e);
}

void updateUI(engine* e) {
    //handle ui updates
    e->Renderer->UpdateParticles((float)std::min(e->getDelta(), 0.1));//clamp so particles dont explode
    for (auto q : e->Renderer->GetAll2D()) {if (q->visible) q->update();}

    GLFWwindow* win = e->getWindow();

    double mx, my;
    int ww, wh;
    glfwGetCursorPos(win, &mx, &my);
    glfwGetWindowSize(win, &ww, &wh);

    //handle ui buttons
    if (!uiButtonsExist)return;
    if (ww > 0 && wh > 0) {
        e->mousePos = vec2(mx, my);
        vec2 ndc = { (float)(mx / ww * 2.0 - 1.0), (float)(1.0 - my / wh * 2.0) };

        bool down = glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        bool pressedNow  = down && !e->mouseWasDown;
        bool releasedNow = !down && e->mouseWasDown;
        e->mouseWasDown = down;

        std::vector<button*> btns = e->getButtons();

        button* top = nullptr;
        for (auto it = btns.rbegin(); it != btns.rend(); ++it) {
            if ((*it)->visible && (*it)->contains(ndc)) { top = *it; break; }
        }

        for (auto b : btns) {
            bool hit = (b == top);

            if (hit && !b->isHovering) {
                b->isHovering = true;
                if (b->onHoverEnter) b->onHoverEnter(b);
            } else if (!hit && b->isHovering) {
                b->isHovering = false;
                if (b->onHoverExit) b->onHoverExit(b);
            }

            if (pressedNow && hit) b->isPressed = true;

            if (releasedNow) {
                if (b->isPressed && hit && b->onClick) b->onClick(b);
                b->isPressed = false;
            }
        }
    }
}

void updateHealthUI(engine* e) {
    if (plrHealth != lastHealth) {
        for (auto q : hpText) {
            q->visible=false;
            e->Renderer->RemoveFromRQ_2D(*q);
        }

        string hpStr = "Health " + to_string( (int)round((float) plrHealth / (float)maxPlrHealth * 100)) + "%";
        for (auto q : StrToUI(e->m_font_atlas_2, pHPDisplayPos, hpStr)) {
            e->Renderer->AddToRQ_2D(*q);
            hpText.push_back(q);
        }

        lastHealth = plrHealth;
    }
}

button* createButton(engine* e, vec2 pos, vec2 size, std::string text, float textScale=1.0f, std::function<void(button*)> onClick=nullptr, std::function<void(button*)>onMouseEnter=nullptr, std::function<void(button*)>onMouseExit=nullptr) {
    button* btn = new button(quad(pos, size, tileUV(1, 0)));

    btn->col = vec4(1, 1, 1, 1);
    btn->create(e->m_texture_atlas);
    e->Renderer->AddToRQ_2D(*btn);

    //text keeps buttons visibility
    btn->onUpdate = [btn](quad* q) -> void {
        for (auto l : btn->textQuads) {
            l->visible=q->visible;
        }
    };

    for (auto q : StrToUI(e->m_font_atlas_2, pos, std::move(text), textScale, 1)) {
        q->col = vec4(1, 1, 1, 1);
        e->Renderer->AddToRQ_2D(*q);

        btn->textQuads.push_back(q);
    }

    btn->onClick = std::move(onClick);

    btn->onHoverEnter = [onMouseEnter](button* q) -> void {
        q->col = vec4(.8f, .8f, .8f, 1);
        for (auto l : q->textQuads) {
            l->col = vec4(.8f, .8f, .8f, 1);
        }
        if (onMouseEnter) onMouseEnter(q);
    };

    btn->onHoverExit = [onMouseExit](button* q) -> void {
        q->col = vec4(1, 1, 1, 1);
        for (auto l : q->textQuads) {
            l->col = vec4(1, 1, 1, 1);
        }
        if (onMouseExit) onMouseExit(q);
    };

    e->addButtonToList(btn);
    return btn;
}

void createTitleText(engine* e, const std::string& text, vec2 pos, float scale) {
    float ns = scale * .8f;//80% original scale
    vec2 sc = {0.1f*ns, 0.1f*ns};

    int i = 0;
    for (auto q : StrToUI(e->m_font_atlas, pos, text, scale)) {
        float baseY = q->pos.y;

        q->creationTime -= 0.05 * i++;//stagger the wave per letter
        q->col = {0,0,0, 1.0f};
        q->onUpdate = [baseY](quad* self) -> void {
            float age = (float)(glfwGetTime() - self->creationTime);
            self->pos.y = baseY + std::sin(age) * titleBobAmp;
        };
        e->Renderer->AddToRQ_2D(*q);
        titleQuads.push_back(q);

        quad* fill = new quad(q->pos, sc, q->uv);
        fill->creationTime=q->creationTime;
        fill->z = 1;
        fill->create(e->m_font_atlas);
        fill->onUpdate = [baseY](quad* self) -> void {
            float age = (float)(glfwGetTime() - self->creationTime);

            self->pos.y = baseY + std::sin(age) * titleBobAmp;

            float t = (std::sin(age * 2.0f) + 1.0f) * 0.5f;
            vec4 colA = {1.0f, 0.76f, 0.0f, 1.0f};//orange
            vec4 colB = {1.0f, 0.4f, 0.0f, 1.0f};//red-orange
            self->col = mix(colA, colB, t);
        };
        e->Renderer->AddToRQ_2D(*fill);
        titleQuads.push_back(fill);
    }
}

//death screen
void closeDeathScreen(engine* e) {
    for (quad* q : deathText) {
        e->Renderer->RemoveFromRQ_2D(*q);
        q->destroy();
        delete q;
    }
    deathText.clear();

    hideButton(deathRetry);
    hideButton(deathMenu);
    dead=false;
    deathActive = false;
    deathButtonsShown=false;
}

void createDeathButtons(engine* e) {
    if (deathRetry) return;

    deathRetry = createButton(e, {0, -.1f}, {1.1f, 0.3f}, "Retry", 1,[e](button* q) -> void {
        closeDeathScreen(e);
        uiButtonsExist=false;
        loadLevel(e, 1);
    });

    deathMenu = createButton(e, {0, -.45f}, {1.1f, 0.3f}, "Back to Main Menu", 1,[e](button* q) -> void {
        closeDeathScreen(e);
        clearLevel(e);
        inDungeon=false;
        playerSprite->visible = false;

        for (auto q : hpText) q->visible = false;
        for (auto q : titleQuads) q->visible = true;
        for (auto q : titleObjs) q->visible = true;

        if (titleUI) {
            titleUI->inGuide=false;
            showButton(titleUI->bgn);
            showButton(titleUI->guide);
            hideButton(titleUI->back);
        }

        e->isMoving=false;
        e->state=menuState;
        e->lighting_setting=menuLighting;
    });

    hideButton(deathRetry);
    hideButton(deathMenu);
}

void startDeathScreen(engine* e) {
    createDeathButtons(e);

    dead = false;
    deathActive = true;
    deathButtonsShown=false;
    e->isMoving=false;
    deathStart=glfwGetTime();

    int i=0;
    for (auto q : StrToUI(e->m_font_atlas_2, {0, 0.35f}, "YOU DIED", 3.0f, 10)) {
        vec2 baseSize = q->size;

        q->col = {0.8f, 0.05f, 0.05f, 0.0f};
        q->creationTime = deathStart + deathStartDelay + i * deathLetterGap;   //per-letter start time
        q->onUpdate = [baseSize](quad* self) -> void {
            float age = (float)(glfwGetTime() - self->creationTime);
            float t = std::clamp(age / deathLetterTime, 0.0f, 1.0f);
            float ease = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);

            self->setSize(baseSize * (1.0f + (deathStartScale - 1.0f) * (1.0f - ease)));
            self->col.a = std::clamp(age / 0.1f, 0.0f, 1.0f);
        };

        e->Renderer->AddToRQ_2D(*q);
        deathText.push_back(q);
        i++;
    }
}

void updateDeathScreen(engine* e) {
    if (deathButtonsShown)return;

    float total = deathStartDelay + (deathText.size() - 1) * deathLetterGap + deathLetterTime;
    if (glfwGetTime() - deathStart < total) return;

    deathButtonsShown=true;
    showButton(deathRetry);
    showButton(deathMenu);
    uiButtonsExist=true;
}

//create title scene
void createTitleScene(engine* e) {
    light* camLight = new light(object({0,-2.5f,0}, {.5f,.5f,.5f}, tileUV(0,0)));
    camLight->visible = false;
    camLight->create(e->m_texture_atlas);
    e->Renderer->AddLight(*camLight);
    e->Renderer->AddToRQ_M(*camLight);

    camLight->onUpdate = [e, camLight](object* o) -> void{
        camLight->pos = e->Renderer->cam.pos + vec3(playerPos.x - camFocus.x, 0.0f, playerPos.y - camFocus.y);
    };

    playerLight = camLight;
}

void createPlayerSprite(engine* e) {
    quad* jimothy = new quad({0,0.2f}, {.3f, .4f}, tileUV(0,2));
    jimothy->create(e->m_texture_atlas);
    jimothy->visible=false;
    jimothy->worldSpace = true;
    jimothy->onUpdate = [e](quad* q) -> void {
        if (e->isMoving) {
            q->setFrame((int)(glfwGetTime() * 16));
        } else {
            q->setFrame(0);
        }

        //flip horizontally
        UVRect r = q->frames.at(q->curFrame);
        if (facingLeft) std::swap(r.u0, r.u1);

        //reset the uv if facing normally
        if (q->uv.u0 != r.u0 || q->uv.u1 != r.u1) q->setUV(r);
    };

    jimothy->addFrame(0, 2);
    jimothy->addFrame(1, 2);
    jimothy->addFrame(2, 2);
    jimothy->addFrame(3, 2);
    jimothy->addFrame(4, 2);
    jimothy->addFrame(5, 2);

    e->Renderer->AddToRQ_2D(*jimothy);

    playerSprite = jimothy;
}

void createTitleScreen(engine* e) {
    createTitleScene(e);
    createTitleText(e, "Buried Alive", {0, 0.75f}, 3.0f);
    createPlayerSprite(e);

    auto st = std::make_shared<TitleState>();

    std::vector<std::string> guideTextLines = {
        "Welcome to Buried Alive!",
        "Your objective is to survive for as long as possible!",
        "To play the controls are simple",
        "W A S D to move",
        "E is to shoot projectiles",
        "Shift is to sprint",
        "But be careful of the slimes, they attack on sight",
        "To advance to the next level, look for a hole in the floor and jump down!",
        "Good luck!"
    };

    for (size_t i = 0; i < guideTextLines.size(); i++) {
        float y = 0.5f - 0.12f * (float)i;
        for (auto q : StrToUI(e->m_font_atlas_2, {0, y}, guideTextLines[i], 0.7f, 2)) {
            q->visible = false;
            e->Renderer->AddToRQ_2D(*q);
            st->guideText.push_back(q);
        }
    }

    st->bgn = createButton(e, {0, 0.2f}, {.4f, .3f}, "Begin", 1.0f,
        [e, st](button* q) {
            if (st->inGuide) return;
            hideButton(q);
            hideButton(st->guide);
            hideButton(st->back);
            startGame(e);
            uiButtonsExist = false;
        });

    st->guide = createButton(e, {0, -.1f}, {.4f, .3f}, "Guide", 1.0f,
        [st](button* q) {
            hideButton(q);
            hideButton(st->bgn);
            showButton(st->back);
            st->inGuide = true;
            for (auto t : st->guideText) t->visible = true;
        });

    st->back = createButton(e, {0, -.7f}, {.4f, .3f}, "Back", 1.0f,
        [st](button* q) {
            if (!st->inGuide) return;
            hideButton(q);
            showButton(st->bgn);
            showButton(st->guide);
            st->inGuide = false;
            for (auto t : st->guideText) t->visible = false;
        });
    hideButton(st->back);

    for (auto q : StrToUI(e->m_font_atlas_2, pHPDisplayPos, "Health 100%")) {
        q->visible = false;
        e->Renderer->AddToRQ_2D(*q);
        hpText.push_back(q);
    }

    uiButtonsExist=true;
    titleUI = st;
    menuState = e->state;
    menuLighting = e->lighting_setting;
}

//links a shader program and logs the reason if it fails
unsigned int linkProgram(unsigned int vert, unsigned int frag, const char* name) {
    unsigned int prog = glCreateProgram();
    glAttachShader(prog, vert);
    glAttachShader(prog, frag);
    glLinkProgram(prog);

    int succ;
    char infoLog[512];
    glGetProgramiv(prog, GL_LINK_STATUS, &succ);
    if(!succ) {
        glGetProgramInfoLog(prog, 512, NULL, infoLog);
        err((std::string("Failed to link ") + name + " shaders: " + infoLog).c_str());
        glDeleteProgram(prog);
        return 0;
    }
    return prog;
}

int engine::init() {
    info("Initializing glfw...");

    //initalize glfw
    glfwInit();

    info("Creating window...");

    //window hints
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    //setup the window + context
    win = glfwCreateWindow(winSize->x, winSize->y, "Buried Alive FPS: 999", NULL, NULL);
    if (!win) {
        err("Failed to create GLFW window");
        return -1;
    }
    glfwMakeContextCurrent(win);
    glfwSwapInterval(0);//0 = vsync off

    info("Loading Glad...");
    //load glad
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        err("Failed to initialize GLAD");
        return -1;
    }

    //enable transparency
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    //start setting up the window and set the window ptr to this engine instance
    glfwSetWindowUserPointer(win, this);
    setWindowSize(winSize);

    //set callbacks
    glfwSetFramebufferSizeCallback(win, framebuffer_size_callback);

    //set image flip verticallly
    stbi_set_flip_vertically_on_load(true);

    //create shaders
    unsigned int ui_vertShader = compileShader(GL_VERTEX_SHADER, "res/ui.vert");
    unsigned int ui_fragShader = compileShader(GL_FRAGMENT_SHADER, "res/ui.frag");
    if (!ui_vertShader || !ui_fragShader) { err("Failed to load ui shaders."); return -1; }

    unsigned int m_vertShader = compileShader(GL_VERTEX_SHADER, "res/main.vert");
    unsigned int m_fragShader = compileShader(GL_FRAGMENT_SHADER, "res/main.frag");
    if (!m_vertShader || !m_fragShader) { err("Failed to load main shaders."); return -1; }

    unsigned int p_vert = compileShader(GL_VERTEX_SHADER, "res/particle.vert");
    unsigned int p_frag = compileShader(GL_FRAGMENT_SHADER, "res/particle.frag");
    if (!p_vert || !p_frag) { err("Failed to load particle shaders."); return -1; }

    //link shaders
    ui_shader_program = linkProgram(ui_vertShader, ui_fragShader, "UI");
    m_shader_program = linkProgram(m_vertShader, m_fragShader, "MAIN");
    p_shader_program = linkProgram(p_vert, p_frag, "Particle");
    if (!ui_shader_program || !m_shader_program || !p_shader_program) return -1;

    glDeleteShader(ui_vertShader);
    glDeleteShader(ui_fragShader);
    glDeleteShader(m_vertShader);
    glDeleteShader(m_fragShader);
    glDeleteShader(p_vert);
    glDeleteShader(p_frag);

    m_texture_atlas = loadTexture("res/mainAtlas.png");
    m_font_atlas = loadTexture("res/FontAtlas.png");
    m_font_atlas_2 = loadTexture("res/DejaVu Sans Mono.png");
    if (!m_texture_atlas) { err("Failed to load main Texture Atlas"); return -1; }
    if (!m_font_atlas) { err("Failed to load font Texture Atlas"); return -1; }
    if (!m_font_atlas_2) { err("Failed to load font Texture Atlas 2"); return -1; }

    glUseProgram(ui_shader_program);
    glUniform1i(glGetUniformLocation(ui_shader_program, "tex"), 0);

    glUseProgram(m_shader_program);
    glUniform1i(glGetUniformLocation(m_shader_program, "tex"), 0);

    Renderer = new renderer();

    Renderer->InitParticles();
    createTitleScreen(this);

    createLowRes((int)(180.0f * winSize->x / winSize->y), 180);//180x180px with aspect ratio
    info("Initialized Successfully.");
    return 0;
}

//should NOT be called outside of the loop function
void engine::render() {
    //bind low res fbo
    glBindFramebuffer(GL_FRAMEBUFFER, lFBO);
    glViewport(0, 0, lowW, lowH);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    //draw 3d
    glUseProgram(m_shader_program);
    Renderer->LoadRQ_M();
    Renderer->Draw3D(m_shader_program, (float)winSize->x / (float)winSize->y, lighting_setting);

    //upscale FBO for pixelation
    glBindFramebuffer(GL_READ_FRAMEBUFFER, lFBO);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBlitFramebuffer(0, 0, lowW, lowH, 0, 0, winSize->x, winSize->y,
                      GL_COLOR_BUFFER_BIT, GL_NEAREST);

    //convert back to regular scale for ui
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, winSize->x, winSize->y);

    //draw particles ontop of 3d geometry + below ui
    Renderer->DrawParticles(p_shader_program, (float)winSize->x / (float)winSize->y);

    //draw 2d
    glUseProgram(ui_shader_program);
    Renderer->LoadRQ_2D();
    Renderer->Draw2D(ui_shader_program, (float)winSize->x / (float)winSize->y);

    //swap bufs
    glfwSwapBuffers(win);
    glfwPollEvents();
}

//should NOT be called outside of the loop function
void engine::update() {
    framecount++;
    double frameT = glfwGetTime();

    handle_input(this);
    updateHealthUI(this);
    updateUI(this);

    for (auto q : Renderer->GetAll3D()) {q->update();}

    if (frameT > lastFUpdate + 1) {
        lastFUpdate = frameT;
        glfwSetWindowTitle(win, ("Buried Alive FPS: " + std::to_string(framecount)).c_str());
        framecount = 0;
    }

    delta = frameT - lastFrame;
    lastFrame = frameT;
}

void engine::loop() {
    while (!glfwWindowShouldClose(win)) {
        update();
        render();
    }
    glfwTerminate();
}

GLFWwindow *engine::getWindow() {
    return win;
}

void engine::setWindowSize(ivec2 *wSize) {
    if (wSize->x <= 0 || wSize->y <= 0) return;

    winSize = wSize;
    glViewport(0, 0, wSize->x, wSize->y);
    if (lFBO) createLowRes((int)(180.0f * wSize->x / wSize->y), 180);//180x180px with aspect ratio
}

void engine::createLowRes(int w, int h) {
    if (lFBO) {
        glDeleteFramebuffers(1, &lFBO);
        glDeleteTextures(1, &lCol);
        glDeleteRenderbuffers(1, &lDepth);
    }

    //set screen dims
    lowW = w;
    lowH = h;

    //generate frame buffers
    glGenFramebuffers(1, &lFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, lFBO);

    //low quality texture
    glGenTextures(1, &lCol);
    glBindTexture(GL_TEXTURE_2D, lCol);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, lCol, 0);

    //depth buffers
    glGenRenderbuffers(1, &lDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, lDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, lDepth);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        err("framebuffer incomplete");

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}