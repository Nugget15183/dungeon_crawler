#include "engine.h"

#include <string>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "stb_image.h"

#include "util.h"
#include <random>
#include <utility>

/*
todo list
3d
custom model loading
player movement
dungeon generation
spawn animation (cave collapse, trapping player in first room)

2D
title menu
settings


 */

using namespace util;
using namespace logger;

//random class, used for particle randomness
static std::mt19937 rng{std::random_device{}()};
static float rnd(float a, float b) { return std::uniform_real_distribution(a, b)(rng); }

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

void handle_input(engine* e) {
    GLFWwindow* win = e->getWindow();

    if (glfwGetKey(win, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(win, true);

    if (e->state!=IN_GAME) return;

    double delta = e->getDelta();
    double speed = 2.00;

    vec2 walkDir = {0,0};
    if (glfwGetKey(win, GLFW_KEY_W) == GLFW_PRESS) walkDir.y=-1;
    if (glfwGetKey(win, GLFW_KEY_A) == GLFW_PRESS) walkDir.x=-1;
    if (glfwGetKey(win, GLFW_KEY_S) == GLFW_PRESS) walkDir.y=1;
    if (glfwGetKey(win, GLFW_KEY_D) == GLFW_PRESS) walkDir.x=1;

    walkDir=safeNorm(walkDir);

    e->Renderer->cam.pos += vec3(walkDir.x*speed*delta,0,walkDir.y*speed*delta);

    if (length(walkDir)>0) {
        spawnParticles(
            e->Renderer,
            {0,-.3f},
            walkDir*vec2(-1,1)+vec2(0.0,0.2f),
            vec2(-.5f, .5f),
            vec2(0, -1.0f),
            vec4(0.41f, 0.39f, 0.32f, 1.0f),
            vec4(0.08f, 0.08f, 0.04f, 1.0f),
            vec2(0.01f, 0.01f),
            vec2(0.0f, 0.01f),
            vec2(0.1f, 0.3f),
            1
        );
    }
}

void updateUI(engine* e) {
    //handle ui updates before buttons
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

void createButton(engine* e, vec2 pos, vec2 size, string text, float textScale=1.0f, std::function<void(button*)> onClick=nullptr, std::function<void(button*)>onMouseEnter=nullptr, std::function<void(button*)>onMouseExit=nullptr) {
    button* start = new button(quad(pos, size, tileUV(1, 0)));

    start->col = vec4(1, 1, 1, 1);
    start->create(e->m_texture_atlas);
    e->Renderer->AddToRQ_2D(*start);

    for (auto q : StrToUI(e->m_font_atlas, pos, std::move(text), textScale, 1)) {
        q->col = vec4(1, 1, 1, 1);
        e->Renderer->AddToRQ_2D(*q);

        start->textQuads.push_back(q);
    }

    start->onClick = [e, onClick](button* b) {
        if (onClick) onClick(b);
    };

    start->onHoverEnter = [e, onMouseEnter](button* q) -> void {
        q->col = vec4(.8f, .8f, .8f, 1);
        for (auto l : q->textQuads) {
            l->col = vec4(.8f, .8f, .8f, 1);
        }
        if (onMouseEnter) onMouseEnter(q);
    };

    start->onHoverExit = [e, onMouseExit](button* q) -> void {
        q->col = vec4(1, 1, 1, 1);
        for (auto l : q->textQuads) {
            l->col = vec4(1, 1, 1, 1);
        }
        if (onMouseExit) onMouseExit(q);
    };

    e->addButtonToList(start);
}

void createTitleScreen(engine* e) {
    createButton(
        e,
        {-.75f, 0},
        {.4f, .3f},
        "Begin",
        1.0f,
        [e](button* q) ->void {
            e->state = IN_GAME;
            //todo: spawning in animation
        }
    );

    //state=IN_GAME;

    //title screen
    float scale = 1.5f;
    int i=0;
    for (auto q : StrToUI(e->m_font_atlas, {0, 0.7f}, "Buried Alive", scale)) {
        i++;
        double cTimeOffset = 0.05f*i;

        q->creationTime-=cTimeOffset;
        q->col = {0,0,0, 1.0f};

        q->onUpdate = [](quad* q) -> void {
            double age = glfwGetTime() - q->creationTime;
            q->pos.y += sin(age)/30000;
        };

        e->Renderer->AddToRQ_2D(*q);

        float ns = scale * .8f;//80% original scale
        vec2 sc = {0.1f*ns, 0.1f*ns};

        quad* fill = new quad(q->pos, sc, q->uv);
        fill->creationTime=q->creationTime;
        fill->z = 1;
        fill->create(e->m_font_atlas);
        e->Renderer->AddToRQ_2D(*fill);

        fill->onUpdate = [](quad* q) -> void {
            float age = glfwGetTime() - q->creationTime;

            q->pos.y += sin(age)/30000;

            float t = (sin(age * 2.0) + 1.0f) * 0.5f;
            vec4 colA = {1.0f, 0.76f, 0.0f, 1.0f};//orange
            vec4 colB = {1.0f, 0.4f, 0.0f, 1.0f};//red-orange
            q->col = mix(colA, colB, t);
        };
    }

    //title screen
    vector objs = {
        //floor
        new object({0,-3.5f,0}, {100,1,100}, tileUV(0,0)),
        //test collisions obj
        new object({1,-2.5f,1}, {1,1,1}, tileUV(0,0)),
    };

    for (auto o : objs) {
        o->create(e->m_texture_atlas);
        e->Renderer->AddToRQ_M(*o);
    }

    light* torch = new light(object({0,-2.5f,0}, {.5f,.5f,.5f}, tileUV(0,0)));
    torch->create(e->m_texture_atlas);
    e->Renderer->AddLight(*torch);
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
    ui_shader_program = glCreateProgram();
    glAttachShader(ui_shader_program, ui_vertShader);
    glAttachShader(ui_shader_program, ui_fragShader);
    glLinkProgram(ui_shader_program);

    m_shader_program = glCreateProgram();
    glAttachShader(m_shader_program, m_vertShader);
    glAttachShader(m_shader_program, m_fragShader);
    glLinkProgram(m_shader_program);

    p_shader_program = glCreateProgram();
    glAttachShader(p_shader_program, p_vert);
    glAttachShader(p_shader_program, p_frag);
    glLinkProgram(p_shader_program);

    int  mlsucc;
    char mlinfoLog[512];
    glGetProgramiv(m_shader_program, GL_LINK_STATUS, &mlsucc);
    if(!mlsucc) {
        glGetProgramInfoLog(m_shader_program, 512, NULL, mlinfoLog);
        err((to_string(*"Failed to link MAIN shaders: ") + mlinfoLog).c_str());
        return -1;
    }

    int  ulsucc;
    char ulinfoLog[512];
    glGetProgramiv(ui_shader_program, GL_LINK_STATUS, &ulsucc);
    if(!ulsucc) {
        glGetProgramInfoLog(ui_shader_program, 512, NULL, ulinfoLog);
        err((to_string(*"Failed to link UI shaders: ") + ulinfoLog).c_str());
        return -1;
    }

    int  plsucc;
    char plinfoLog[512];
    glGetProgramiv(p_shader_program, GL_LINK_STATUS, &plsucc);
    if(!plsucc) {
        glGetProgramInfoLog(p_shader_program, 512, NULL, plinfoLog);
        err((to_string(*"Failed to link Particle shaders: ") + plinfoLog).c_str());
        return -1;
    }

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
    Renderer->Draw3D(m_shader_program, (float)winSize->x / (float)winSize->y);

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

    for (auto q : Renderer->GetAll3D()) {if (q->visible) q->update();}

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
