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
private:
    GLFWwindow* win;
    ivec2* winSize = new ivec2(800, 600);

    double eStart = glfwGetTime();

    double lastFrame = eStart;
    double lastFUpdate = eStart;

    double delta = 0;

    int framecount = 0;

    unsigned int shader_program;
    unsigned int texture;

    quad q = quad();
};


#endif