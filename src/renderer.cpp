#include "renderer.h"
#include <algorithm>
#include "logger.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/norm.hpp>

//2d
quad& renderer::GetNextFromRQ_2D() {
    quad& ret = *ui_render_queue[0];

    ui_render_queue.erase(ui_render_queue.begin());

    return ret;
}

void renderer::LoadRQ_2D() {
    if (!ui_render_queue.empty()) { logger::warn("Render Queue not empty when loading new items");  ui_render_queue.clear();}

    //lower in queue = renders before previous assets

    std::vector<quad*> order = quads;

    //sort the quads by z depth
    std::stable_sort(order.begin(), order.end(), [](const quad* a, const quad* b) {
        return a->z < b->z;
    });

    //push elements to the render queue
    for (auto &a : order) {
        if (a->visible) {
           ui_render_queue.push_back(a);
        }
    }
}

void renderer::AddToRQ_2D(quad &quad) {
    quads.push_back(&quad);
}

std::vector<quad*> renderer::GetRQ_2D() {
    return ui_render_queue;
}

void renderer::Draw2D(unsigned int program, float aspect) {
    glUseProgram(program);

    //get the renderer ready to draw 2d
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);

    while (!ui_render_queue.empty()) {
        quad* next = &GetNextFromRQ_2D();
        next->draw();
    }
}

//3d
object& renderer::GetNextFromRQ_M() {
    object& ret = *m_render_queue[0];
    m_render_queue.erase(m_render_queue.begin());
    return ret;
}

void renderer::LoadRQ_M() {
    if (!m_render_queue.empty()) { logger::warn("3D Render Queue not empty when loading new items"); m_render_queue.clear(); }

    for (auto* o : objects) {
        if (o->visible) m_render_queue.push_back(o);
    }

    std::stable_sort(m_render_queue.begin(), m_render_queue.end(), [this](const object* a, const object* b) {
        return distance2(a->pos, cam.pos) < distance2(b->pos, cam.pos);
    });
}

void renderer::AddToRQ_M(object& obj) {
    objects.push_back(&obj);
}

std::vector<object*> renderer::GetRQ_3D() {
    return m_render_queue;
}

void renderer::Draw3D(unsigned int program, float aspect) {
    glUseProgram(program);

    mat4 viewProj = cam.proj(aspect) * cam.view();
    int locVP = glGetUniformLocation(program, "uViewProj");
    int locModel = glGetUniformLocation(program, "uModel");
    glUniformMatrix4fv(locVP, 1, GL_FALSE, value_ptr(viewProj));

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    while (!m_render_queue.empty()) {
        object* o = &GetNextFromRQ_M();

        mat4 model = o->modelMatrix();
        glUniformMatrix4fv(locModel, 1, GL_FALSE, value_ptr(model));
        o->draw();
    }
}