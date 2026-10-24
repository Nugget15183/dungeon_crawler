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

    std::vector<object*> opaque, transparent;
    for (auto* o : objects) {
        if (!o->visible) continue;
        if (o->alpha < 1.0f) transparent.push_back(o);
        else opaque.push_back(o);
    }

    std::stable_sort(opaque.begin(), opaque.end(), [this](const object* a, const object* b) {
        return distance2(a->pos, cam.pos) < distance2(b->pos, cam.pos);
    });
    std::stable_sort(transparent.begin(), transparent.end(), [this](const object* a, const object* b) {
        return distance2(a->pos, cam.pos) > distance2(b->pos, cam.pos);
    });

    m_render_queue = opaque;
    m_render_queue.insert(m_render_queue.end(), transparent.begin(), transparent.end());
}

void renderer::AddToRQ_M(object& obj) {
    objects.push_back(&obj);
}

std::vector<object*> renderer::GetRQ_3D() {
    return m_render_queue;
}

void renderer::Draw3D(unsigned int program, float aspect, lightingSetting lighting) {
    glUseProgram(program);

    const int MAX_LIGHTS = 16;
    vec3 pos[MAX_LIGHTS], col[MAX_LIGHTS];
    float rad[MAX_LIGHTS];
    int n = 0;

    for (auto* l : lights) {
        if (!l->enabled || n >= MAX_LIGHTS) continue;
        pos[n] = l->pos;
        col[n] = l->color * l->intensity;
        rad[n] = l->radius;
        n++;
    }

    glUniform1i(glGetUniformLocation(program, "uNumLights"), n);
    if (n > 0) {
        glUniform3fv(glGetUniformLocation(program, "uLightPos"), n, value_ptr(pos[0]));
        glUniform3fv(glGetUniformLocation(program, "uLightColor"), n, value_ptr(col[0]));
        glUniform1fv(glGetUniformLocation(program, "uLightRadius"), n, rad);
    }

    if (lighting==OUTSIDE) {
        glUniform3f(glGetUniformLocation(program, "uAmbient"), 1.0f, 0.94f, 0.72f);
    } else if (lighting==DUNGEON) {
        glUniform3f(glGetUniformLocation(program, "uAmbient"), 0.05f, 0.05f, 0.08f);
    } else if (lighting==MOONLIT) {
        glUniform3f(glGetUniformLocation(program, "uAmbient"), 0.72f, 0.91f, 1.0f);
    }

    mat4 viewProj = cam.proj(aspect) * cam.view();
    int locVP = glGetUniformLocation(program, "uViewProj");
    int locModel = glGetUniformLocation(program, "uModel");
    int locAlpha = glGetUniformLocation(program, "uAlpha");

    glUniformMatrix4fv(locVP, 1, GL_FALSE, value_ptr(viewProj));

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    while (!m_render_queue.empty()) {
        object* o = &GetNextFromRQ_M();

        glDepthMask(o->alpha < 1.0f ? GL_FALSE : GL_TRUE);

        mat4 model = o->modelMatrix();
        glUniformMatrix4fv(locModel, 1, GL_FALSE, value_ptr(model));
        glUniform1f(locAlpha, o->alpha);
        o->draw();
    }

    glDepthMask(GL_TRUE);
}

//particles

void renderer::InitParticles(size_t capacity) {
    particles.assign(capacity, Particle());
    pInst.reserve(capacity);

    //quad mesh
    const vec2 corners[4] = {{-.5f,-.5f}, {.5f,-.5f}, {-.5f,.5f}, {.5f,.5f}};

    //gen buffers
    glGenVertexArrays(1, &pVAO);
    glGenBuffers(1, &pQuadVBO);
    glGenBuffers(1, &pInstVBO);
    glBindVertexArray(pVAO);

    //per vert
    glBindBuffer(GL_ARRAY_BUFFER, pQuadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(corners), corners, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(vec2), (void*)0);
    glEnableVertexAttribArray(0);

    //per inst
    glBindBuffer(GL_ARRAY_BUFFER, pInstVBO);
    glBufferData(GL_ARRAY_BUFFER, capacity*sizeof(ParticleInst), nullptr, GL_STREAM_DRAW);

    //vertex attribs
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(ParticleInst), (void*)offsetof(ParticleInst, pos));
    glEnableVertexAttribArray(1);
    glVertexAttribDivisor(1, 1);

    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(ParticleInst), (void*)offsetof(ParticleInst, size));
    glEnableVertexAttribArray(2);
    glVertexAttribDivisor(2, 1);

    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(ParticleInst), (void*)offsetof(ParticleInst, col));
    glEnableVertexAttribArray(3);
    glVertexAttribDivisor(3, 1);

    glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(ParticleInst), (void*)offsetof(ParticleInst, emissive));
    glEnableVertexAttribArray(4);
    glVertexAttribDivisor(4, 1);

    //unbind the vao
    glBindVertexArray(0);
}

void renderer::EmitParticle(const Particle &p) {
    //find empty particle slot
    //if full then drop the particle
    for (size_t i = 0; i < particles.size(); i++) {
        size_t idx = (pNext+i) % particles.size();
        if (particles[idx].life <= 0.0f) {
            particles[idx] = p;
            particles[idx].life = particles[idx].maxLife = std::max(p.maxLife, 0.0001f);
            pNext = (idx + 1) % particles.size();
            return;
        }
    }
}

void renderer::UpdateParticles(float dt) {
    pInst.clear();

    for (auto &p : particles) {
        if (p.life <= 0.0f) continue;
        p.life -= dt;
        if (p.life <= 0.0f) continue;

        p.vel += p.accel * dt;
        p.pos += p.vel * dt;

        float t = 1.0f - p.life / p.maxLife; //0 - 1 over lifetime
        pInst.push_back({ p.pos, mix(p.size, p.sizeEnd, t), mix(p.col, p.colEnd, t), p.emissive });
    }
}

void renderer::DrawParticles(unsigned int program, float aspect) {
    if (pInst.empty()) return;

    //enable correct shader program, set unifs
    glUseProgram(program);
    glUniform1f(glGetUniformLocation(program, "uAspect"), aspect);

    const int MAX_LIGHTS = 16;
    vec2 lpos[MAX_LIGHTS];
    vec3 lcol[MAX_LIGHTS];
    float lrad[MAX_LIGHTS];

    int n=0;

    mat4 vp = cam.proj(aspect)*cam.view();
    float tanHalf = tan(radians(cam.fov)*0.5f);

    for (auto &l : lights) {
        //is light enabled?
        if (!l->enabled || n > MAX_LIGHTS) continue;

        //is the light out of view?
        vec4 clip = vp * vec4(l->pos, 1.0f);
        if (clip.w <= 0.0f) continue;

        //store light values
        lpos[n] = vec2(clip.x, clip.y) / clip.w;
        lrad[n] = l->radius / (clip.w*tanHalf);
        lcol[n] = l->color * l->intensity;

        n++;
    }

    //set light values in uniform
    glUniform1i(glGetUniformLocation(program, "uNumLights"), n);
    if (n > 0) {
        glUniform2fv(glGetUniformLocation(program, "uLightPos"),    n, value_ptr(lpos[0]));
        glUniform3fv(glGetUniformLocation(program, "uLightColor"),  n, value_ptr(lcol[0]));
        glUniform1fv(glGetUniformLocation(program, "uLightRadius"), n, lrad);
    }
    glUniform3f(glGetUniformLocation(program, "uAmbient"), 0.05f, 0.05f, 0.08f);

    //disable 3d rendering specific settings
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);

    //bind vao and draw instanced
    glBindVertexArray(pVAO);
    glBindBuffer(GL_ARRAY_BUFFER, pInstVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, pInst.size()*sizeof(ParticleInst), pInst.data());
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)pInst.size());

    //unbind vao
    glBindVertexArray(0);
}

void renderer::DestroyParticles() {
    glDeleteVertexArrays(1, &pVAO);
    glDeleteBuffers(1, &pQuadVBO);
    glDeleteBuffers(1, &pInstVBO);
    pVAO = pQuadVBO = pInstVBO = 0;
}
