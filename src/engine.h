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

struct button : quad {
    button(const quad& q) : quad(q) {}

    bool isHovering = false;
    bool isPressed = false;

    std::function<void(button*)> onClick;
    std::function<void(button*)> onHoverEnter;
    std::function<void(button*)> onHoverExit;

    std::vector<quad*> textQuads;

    bool contains(vec2 ndc) const {
        vec2 h = verts[0].pos;
        return fabs(ndc.x - pos.x) <= h.x && fabs(ndc.y - pos.y) <= h.y;
    }
};

enum gameState {
    MAIN_MENU,
    IN_GAME,
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

    void addButtonToList(button* btn) {buttons.push_back(btn);}
    std::vector<button*>getButtons() {return buttons;}

    renderer* Renderer;
    unsigned int m_texture_atlas;
    unsigned int m_font_atlas;

    vec2 mousePos = {0,0};

    bool mouseWasDown = false;
    bool isMoving = false;

    gameState state = MAIN_MENU;
    lightingSetting lighting_setting = OUTSIDE;
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
    unsigned int p_shader_program;

    unsigned int lFBO = 0, lCol = 0, lDepth = 0;
    int lowW = 0, lowH = 0;

    std::vector<button*> buttons;
};


#endif