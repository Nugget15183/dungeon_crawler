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

/*
todo list
3d
custom model loading
player movement           (started: top-down wasd + tile collision, camera follows the 2d player)
dungeon generation        (started: room + corridor generator in dungeon.h, built when Begin is pressed)
spawn animation (cave collapse, trapping player in first room)   (started: see updateIntro)

2D
settings
 */

using namespace util;
using namespace logger;

//random class, used for particle randomness
static std::mt19937 rng{std::random_device{}()};
static float rnd(float a, float b) { return std::uniform_real_distribution(a, b)(rng); }

//global vars
namespace {
    //title screen
    const float titleBobAmp = 0.01f;

    //world
    const float floorY = -3.5f; //center of the floor blocks
    const float floorTop = floorY + 0.5f;
    const int wallH = 3; //in blocks
    const int hallWidth = 3; //in tiles

    //player
    const float walkSpeed = 2.0f;
    const float sprintSpeed = 4.0f;
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

    //things the title screen made
    std::vector<object*> titleObjs;
    std::vector<quad*> titleQuads;

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

    std::vector<quad*> introRocks; //rocks falling down the shaft
}

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

bool canStand(float x, float z) {
    if (!inDungeon) return true;

    for (float dx : {-playerRadius, playerRadius}) {
        for (float dz : {-playerRadius, playerRadius}) {
            if (!dungeonMap.isFloor((int)std::floor(x + dx), (int)std::floor(z + dz))) return false;
        }
    }
    return true;
}

void syncCamera(engine* e) {
    camera& c = e->Renderer->cam;

    vec3 f = c.foward();
    float t = (floorTop - c.pos.y) / f.y;

    c.pos.x = playerPos.x - f.x * t + camShake.x;
    c.pos.z = playerPos.y - f.z * t + camShake.y;
}

void buildDungeon(engine* e, const dungeon::Map& map) {
    auto addBlock = [&](vec3 pos, auto uv) {
        object* o = new object(pos, {1, 1, 1}, uv);
        o->create(e->m_texture_atlas);
        e->Renderer->AddToRQ_M(*o);
    };

    for (int y = 0; y < map.h; y++) {
        for (int x = 0; x < map.w; x++) {
            //tile (x, y) is world (x, z=y), blocks are centered so offset by .5
            if (map.isFloor(x, y)) {
                addBlock({x + 0.5f, floorY, y + 0.5f}, tileUV(0, 0));
            } else if (map.touchesFloor(x, y)) {
                //only walls next to floor get built, the rest would never be seen
                for (int h = 0; h < wallH; h++) {
                    addBlock({x + 0.5f, floorY + h, y + 0.5f}, tileUV(0, 0));
                }
            }
        }
    }
}

void hideButton(button* b) {
    b->visible = false;
    for (auto t : b->textQuads) t->visible = false;
}

//rocks that fall from the ceiling and stay on the floor around the player
void spawnRubble(engine* e) {
    for (int i = 0; i < 10; i++) {
        float ang = rnd(0.0f, 6.28f);
        float dist = rnd(0.6f, 1.5f);
        float x = playerPos.x + std::cos(ang) * dist;
        float z = playerPos.y + std::sin(ang) * dist;
        if (!canStand(x, z)) continue;//only land on the floor

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
    }
}

//the light shaft, the rocks that fall down it and the rocks that plug the hole
void createIntro(engine* e) {
    //point light above the player, the pool of light on the floor
    beamLight = new light(object({0,0,0}, {.5f,.5f,.5f}, tileUV(0,0)));
    beamLight->visible = false;
    beamLight->pos = vec3(playerPos.x, floorY + 2.0f, playerPos.y);
    beamLight->color = {1.0f, 0.95f, 0.8f};
    beamLight->radius = 6.0f;
    beamLight->intensity = 0.0f;
    e->Renderer->AddLight(*beamLight);
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
        playerSprite->pos.y = introFallFrom * (1.0f - s * s);//speeds up as it falls
    }

    //landing
    if (!introLanded && t >= introLandT) {
        introLanded = true;

        spawnParticles(
            e->Renderer,
            {0,-.2f},
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
                {rnd(-.12f, 2), holeY},
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
            introRocks[i]->visible = dt >= 0 && y > -0.2f;//gone once it hits the floor
        }

        //the player's light
        playerLight->intensity = std::clamp((t - (introCaveStart + 0.6f)) / 1.0f, 0.0f, 1.0f);
    }

    //give the player control
    if (t >= introEnd) {
        beamLight->enabled = false;
        playerLight->intensity = 1.0f;
        playerSprite->pos.y = 0;
        camShake = {0, 0};
        introActive = false;
    }
}

void startGame(engine* e) {
    //hide the title screen
    for (auto q : titleQuads) q->visible = false;
    for (auto o : titleObjs) o->visible = false;

    //generate + build the dungeon
    dungeonMap = dungeon::generate(48, 48, 12, std::random_device{}(), hallWidth);
    buildDungeon(e, dungeonMap);
    inDungeon = true;

    //put the player in the center of the first room
    const dungeon::Room& first = dungeonMap.rooms.front();
    playerPos = {first.cx() + 0.5f, first.cy() + 0.5f};
    syncCamera(e);

    //spawn intro, the player falls in and the hole caves over
    createIntro(e);
    introStart = glfwGetTime();
    introActive = true;
    introLanded = false;
    introRubble = false;
    e->isMoving = false;
    playerSprite->visible = false;//shows up when it starts falling
    playerLight->intensity = 0.0f;//only the beam lights the room at first

    e->lighting_setting = DUNGEON;
    e->state = IN_GAME;
}

void handle_input(engine* e) {
    GLFWwindow* win = e->getWindow();

    if (glfwGetKey(win, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(win, true);

    if (e->state!=IN_GAME) return;

    //no movement while the intro plays
    if (introActive) {
        updateIntro(e);
        syncCamera(e);
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

        if (canStand(playerPos.x + step.x, playerPos.y)) playerPos.x += step.x;
        if (canStand(playerPos.x, playerPos.y + step.y)) playerPos.y += step.y;

        spawnParticles(
            e->Renderer,
            {0,-.15f},
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

    syncCamera(e);
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

void createButton(engine* e, vec2 pos, vec2 size, std::string text, float textScale=1.0f, std::function<void(button*)> onClick=nullptr, std::function<void(button*)>onMouseEnter=nullptr, std::function<void(button*)>onMouseExit=nullptr) {
    button* btn = new button(quad(pos, size, tileUV(1, 0)));

    btn->col = vec4(1, 1, 1, 1);
    btn->create(e->m_texture_atlas);
    e->Renderer->AddToRQ_2D(*btn);

    //text follows the button's visibility
    btn->onUpdate = [btn](quad* q) -> void {
        for (auto l : btn->textQuads) {
            l->visible=q->visible;
        }
    };

    for (auto q : StrToUI(e->m_font_atlas, pos, std::move(text), textScale, 1)) {
        q->col = vec4(1, 1, 1, 1);
        e->Renderer->AddToRQ_2D(*q);

        btn->textQuads.push_back(q);
    }

    btn->onClick = onClick;

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
}

//bobbing title
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

//create title scene
void createTitleScene(engine* e) {
    light* camLight = new light(object({0,-2.5f,0}, {.5f,.5f,.5f}, tileUV(0,0)));
    camLight->visible = false;
    camLight->create(e->m_texture_atlas);
    e->Renderer->AddLight(*camLight);
    e->Renderer->AddToRQ_M(*camLight);

    camLight->onUpdate = [e, camLight](object* o) -> void{
        camLight->pos = e->Renderer->cam.pos;
    };

    playerLight = camLight;
}

void createPlayerSprite(engine* e) {
    quad* jimothy = new quad({0,0}, {.3f, .4f}, tileUV(0,2));
    jimothy->create(e->m_texture_atlas);
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
    createTitleText(e, "Buried Alive", {0, 0.6f}, 3.0f);
    createPlayerSprite(e);

    createButton(
        e,
        {0, 0.2f},
        {.4f, .3f},
        "Begin",
        1.0f,
        [e](button* q) ->void {
            hideButton(q);
            startGame(e);
        }
    );
}

//links a program and logs the reason if it fails
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
    if (!m_texture_atlas) { err("Failed to load main Texture Atlas"); return -1; }
    if (!m_font_atlas) { err("Failed to load font Texture Atlas"); return -1; }

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
    //a minimized window is 0x0, which would divide by zero below
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