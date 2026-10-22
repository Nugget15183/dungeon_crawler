#ifndef DUNGEONCRAWLER_ENGINE_H
#define DUNGEONCRAWLER_ENGINE_H

#include <glad/glad.h>
#include "GLFW/glfw3.h"

#include <glm/glm.hpp>
#include "logger.h"
#include "renderer.h"

using namespace glm;

struct key {
    bool pressed = false;
    float lastPress = (float)glfwGetTime();
};

class engine {
public:
    int init();
    void loop();

    GLFWwindow* getWindow();
    void setWindowSize(ivec2 *wSize);

    float getTime() {return (float)glfwGetTime();}
    double getDelta() {return delta;}

    void render();
    void update();

    void createLowRes(int w, int h);

    renderer* Renderer;
private:
    GLFWwindow* win;
    ivec2* winSize = new ivec2(800, 600);

    double eStart = glfwGetTime();

    double lastFrame = eStart;
    double lastFUpdate = eStart;

    double delta = 0;

    int framecount = 0;

    unsigned int m_shader_program;
    unsigned int ui_shader_program;

    unsigned int m_texture_atlas;
    unsigned int m_font_atlas;

    unsigned int lFBO = 0, lCol = 0, lDepth = 0;
    int lowW = 0, lowH = 0;
};


#endif