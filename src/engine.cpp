#include "engine.h"

#include <string>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "stb_image.h"

#include "util.h"

using namespace util;
using namespace logger;

//c

void framebuffer_size_callback(GLFWwindow* win, int width, int height) {
    engine* eng = static_cast<engine*>(glfwGetWindowUserPointer(win));

    eng->setWindowSize(new ivec2(width, height));
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

    info("Loading Glad...");
    //load glad
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        err("Failed to initialize GLAD");
        return -1;
    }

    //start setting up the window and set the window ptr to this engine instance
    glfwSetWindowUserPointer(win, this);
    setWindowSize(winSize);

    //set callbacks
    glfwSetFramebufferSizeCallback(win, framebuffer_size_callback);

    //set image flip verticallly
    stbi_set_flip_vertically_on_load(true);

    //create shaders
    unsigned int vertShader = compileShader(GL_VERTEX_SHADER, "res/main.vert");
    unsigned int fragShader = compileShader(GL_FRAGMENT_SHADER, "res/main.frag");
    if (!vertShader || !fragShader) { return -1; }

    //link shaders
    shader_program = glCreateProgram();
    glAttachShader(shader_program, vertShader);
    glAttachShader(shader_program, fragShader);
    glLinkProgram(shader_program);

    int  lsucc;
    char linfoLog[512];
    glGetProgramiv(shader_program, GL_LINK_STATUS, &lsucc);
    if(!lsucc) {
        glGetProgramInfoLog(shader_program, 512, NULL, linfoLog);
        err(("Failed to link shaders: " + std::to_string(*linfoLog)).c_str());
        return -1;
    }

    glDeleteShader(vertShader);
    glDeleteShader(fragShader);

    texture = loadTexture("res/noTexture.png");
    if (!texture) { err("Failed to load texture"); return -1; }
    q.create(texture);

    glUseProgram(shader_program);
    glUniform1i(glGetUniformLocation(shader_program, "tex"), 0);

    info("Initialized Successfully.");
    return 0;
}

//should NOT be called outside of the loop function
void engine::render() {
    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(shader_program);

    q.draw();

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