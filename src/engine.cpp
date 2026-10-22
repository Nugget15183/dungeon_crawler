#include "engine.h"

#include <string>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "stb_image.h"

#include "util.h"

//sudo taskkill /F /IM dungeoncrawler.exe

using namespace util;
using namespace logger;


std::vector<quad*> StrToUI(unsigned int fontAtlas, vec2 pos, std::string str, float scale = 1.0f) {
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

void handle_input(engine* e) {
    GLFWwindow* win = e->getWindow();

    if (glfwGetKey(win, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(win, true);

    double delta = e->getDelta();
    double speed = 2.00;

    if (glfwGetKey(win, GLFW_KEY_W) == GLFW_PRESS) e->Renderer->cam.pos += vec3(0,0,-speed*delta);
    if (glfwGetKey(win, GLFW_KEY_A) == GLFW_PRESS) e->Renderer->cam.pos += vec3(-speed*delta,0,0);
    if (glfwGetKey(win, GLFW_KEY_S) == GLFW_PRESS) e->Renderer->cam.pos += vec3(0,0,speed*delta);
    if (glfwGetKey(win, GLFW_KEY_D) == GLFW_PRESS) e->Renderer->cam.pos += vec3(speed*delta,0,0);
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
    win = glfwCreateWindow(winSize->x, winSize->y, "Dungeon Crawler FPS: 999", NULL, NULL);
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

    //link shaders
    ui_shader_program = glCreateProgram();
    glAttachShader(ui_shader_program, ui_vertShader);
    glAttachShader(ui_shader_program, ui_fragShader);
    glLinkProgram(ui_shader_program);

    m_shader_program = glCreateProgram();
    glAttachShader(m_shader_program, m_vertShader);
    glAttachShader(m_shader_program, m_fragShader);
    glLinkProgram(m_shader_program);

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

    glDeleteShader(ui_vertShader);
    glDeleteShader(ui_fragShader);
    glDeleteShader(m_vertShader);
    glDeleteShader(m_fragShader);

    m_texture_atlas = loadTexture("res/mainAtlas.png");
    m_font_atlas = loadTexture("res/FontAtlas.png");
    if (!m_texture_atlas) { err("Failed to load main Texture Atlas"); return -1; }
    if (!m_font_atlas) { err("Failed to load font Texture Atlas"); return -1; }

    glUseProgram(ui_shader_program);
    glUniform1i(glGetUniformLocation(ui_shader_program, "tex"), 0);

    glUseProgram(m_shader_program);
    glUniform1i(glGetUniformLocation(m_shader_program, "tex"), 0);

    Renderer = new renderer();

    //title screen
    int i=0;
    for (auto q : StrToUI(m_font_atlas, {0, 0.7f}, "Dungeon Crawler", 1.5f)) {
        i++;
        q->creationTime-=0.05f*i;

        q->onUpdate = [](quad* q) -> void {
            double t = glfwGetTime() - q->creationTime;
            q->pos = vec2(q->pos.x, q->pos.y+sin(t)/30000);
        };

        Renderer->AddToRQ_2D(*q);
    }

    //test area
    vector<object*> objs = {
        //floor
        new object({0,-3.5f,0}, {10,1,10}, tileUV(0,0)),
        //walls
        new object({5,-2.5f,0}, {1,1,9}, tileUV(0,0)),
        new object({-5,-2.5f,0}, {1,1,9}, tileUV(0,0)),
        new object({0,-2.5f,5}, {9,1,1}, tileUV(0,0)),
        new object({0,-2.5f,-5}, {9,1,1}, tileUV(0,0)),
        //corners
        new object({5,-2.5f,5}, {1,1,1}, tileUV(0,0)),
        new object({-5,-2.5f,-5}, {1,1,1}, tileUV(0,0)),
        new object({5,-2.5f,5}, {1,1,1}, tileUV(0,0)),
        new object({-5,-2.5f,-5}, {1,1,1}, tileUV(0,0)),
    };

    for (auto o : objs) {
        o->create(m_texture_atlas);
        Renderer->AddToRQ_M(*o);
    }

    light* torch = new light(object({0,-2.5f,0}, {.5f,.5f,.5f}, tileUV(0,0)));
    torch->create(m_texture_atlas);
    Renderer->AddLight(*torch);

    info("Initialized Successfully.");
    return 0;
}

//should NOT be called outside of the loop function
void engine::render() {
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(m_shader_program);

    Renderer->LoadRQ_M();
    Renderer->Draw3D(m_shader_program, (float)winSize->x / (float)winSize->y);

    glUseProgram(ui_shader_program);

    Renderer->LoadRQ_2D();
    Renderer->Draw2D(ui_shader_program, (float)winSize->x / (float)winSize->y);

    glfwSwapBuffers(win);
    glfwPollEvents();
}

//should NOT be called outside of the loop function
void engine::update() {
    framecount++;
    double frameT = glfwGetTime();

    handle_input(this);

    for (auto q : Renderer->GetAll3D()) {if (q->visible) q->update();}
    for (auto q : Renderer->GetAll2D()) {if (q->visible) q->update();}

    if (frameT > lastFUpdate + 1) {
        lastFUpdate = frameT;
        glfwSetWindowTitle(win, ("Dungeon Crawler FPS: " + std::to_string(framecount)).c_str());
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
}