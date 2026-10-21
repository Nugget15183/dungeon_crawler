#include "engine.h"

#include <string>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "stb_image.h"

#include "util.h"

//sudo taskkill /F /IM dungeoncrawler.exe

using namespace util;
using namespace logger;

void framebuffer_size_callback(GLFWwindow* win, int width, int height) {
    engine* eng = static_cast<engine*>(glfwGetWindowUserPointer(win));

    eng->setWindowSize(new ivec2(width, height));
}

std::vector<quad*> StrToUI(unsigned int fontAtlas, vec2 pos, std::string str, float scale = 1.0f) {
    std::vector<quad*> ret;
    int x = 0;
    float sc = 0.1f * scale;
    float spacing = 0.05f * scale;

    for (const char& c : str) {
        x++;
        int baseIndex = static_cast<unsigned char>(c);

        int col = baseIndex % 16;
        int row = baseIndex / 16;

        quad* q = new quad({(float)x*spacing + pos.x, pos.y}, {sc, sc}, tileUV(col, row));
        q->create(fontAtlas);
        ret.push_back(q);
    }

    return ret;
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
    glGetProgramiv(ui_shader_program, GL_LINK_STATUS, &mlsucc);
    if(!mlsucc) {
        glGetProgramInfoLog(ui_shader_program, 512, NULL, mlinfoLog);
        err(("Failed to link shaders: " + std::to_string(*mlinfoLog)).c_str());
        return -1;
    }

    int  ulsucc;
    char ulinfoLog[512];
    glGetProgramiv(ui_shader_program, GL_LINK_STATUS, &ulsucc);
    if(!ulsucc) {
        glGetProgramInfoLog(ui_shader_program, 512, NULL, ulinfoLog);
        err(("Failed to link shaders: " + std::to_string(*ulinfoLog)).c_str());
        return -1;
    }

    glDeleteShader(ui_vertShader);
    glDeleteShader(ui_fragShader);

    m_texture_atlas = loadTexture("res/noTexture.png");//noTexture.png is temporary
    m_font_atlas = loadTexture("res/FontAtlas.png");
    if (!m_texture_atlas) { err("Failed to load main Texture Atlas"); return -1; }
    if (!m_font_atlas) { err("Failed to load font Texture Atlas"); return -1; }

    glUseProgram(ui_shader_program);
    glUniform1i(glGetUniformLocation(ui_shader_program, "tex"), 0);

    glUseProgram(m_shader_program);
    glUniform1i(glGetUniformLocation(m_shader_program, "tex"), 0);

    Renderer = new renderer();

    //test the font atlas
    for (auto q : StrToUI(m_font_atlas, {-1, 0}, "Hello World", 1.5f)) {
        Renderer->AddToRQ_2D(*q);
    }

    //load them to the queue
    Renderer->LoadRQ_2D();

    info("Initialized Successfully.");
    return 0;
}

/*
    //create test quads for rendering

    for (int i=10; i > 0; i--) {
        quad* q = new quad({0.0f, 0.0f}, {0.5f, 0.5f}, tileUV(3, 2));
        q->z = i;
        q->create(m_texture_atlas);
        Renderer->AddToRQ(*q);
    }

    //load them to the queue
    Renderer->LoadRQ();

    //test if they were added correctly
    for (auto q : Renderer->GetRQ()) {
        info(to_string(q->z).c_str());
    }
 */

//should NOT be called outside of the loop function
void engine::render() {
    glClearColor(0.2f, 0.3f, 0.5f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(m_shader_program);

    //load 3d elements

    glUseProgram(ui_shader_program);

    //draw ui elements ontop

    for (auto q : Renderer->GetRQ()) {
        q->draw();
    }

    glfwSwapBuffers(win);
    glfwPollEvents();
}

//should NOT be called outside of the loop function
void engine::update() {
    framecount++;
    double frameT = glfwGetTime();

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