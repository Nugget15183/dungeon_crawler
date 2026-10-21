#ifndef DUNGEONCRAWLER_RENDERER_H
#define DUNGEONCRAWLER_RENDERER_H

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
using namespace glm;

struct vertex {
    vec3 pos;
    vec2 uv;
};

struct UVRect {
    float u0, v0, u1, v1;
};

inline UVRect tileUV(int tx, int ty, int ATLAS_TILES = 16) {
    float TILE = 1.0f/(float)ATLAS_TILES;

    float u0 = (float)tx * TILE;
    float v1 = 1.0f - (float)ty * TILE;

    return { u0, v1 - TILE, u0 + TILE, v1 };
}

//for 2d elements
struct quad {
    quad(vec2 pos, vec2 size, UVRect uv = {0.0f, 0.0f, 1.0f, 1.0f}) {
        float hx = 0.5f * size.x;
        float hy = 0.5f * size.y;

        verts[0] = {{ hx + pos.x,  hy + pos.y, 0.0f}, {uv.u1, uv.v1}}; // top right
        verts[1] = {{ hx + pos.x, -hy + pos.y, 0.0f}, {uv.u1, uv.v0}}; // bottom right
        verts[2] = {{-hx + pos.x, -hy + pos.y, 0.0f}, {uv.u0, uv.v0}}; // bottom left
        verts[3] = {{-hx + pos.x,  hy + pos.y, 0.0f}, {uv.u0, uv.v1}}; // top left

        creationTime = glfwGetTime();
    }

    vertex verts[4];
    unsigned int idx[6] = {0, 1, 3, 1, 2, 3};

    unsigned int VAO = 0, VBO = 0, EBO = 0;
    unsigned int texture = 0;
    bool visible = true;
    int z = 0;
    double creationTime = 0;

    void setUV(const UVRect& uv) {
        verts[0].uv = {uv.u1, uv.v1};
        verts[1].uv = {uv.u1, uv.v0};
        verts[2].uv = {uv.u0, uv.v0};
        verts[3].uv = {uv.u0, uv.v1};

        if (VBO) {
            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);
        }
    }

    void setTile(int tx, int ty, int atlasTiles = 16) {
        setUV(tileUV(tx, ty, atlasTiles));
    }

    void create(unsigned int tex) {
        texture = tex;

        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);

        glBindVertexArray(VAO);

        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_DYNAMIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(idx), idx, GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*)offsetof(vertex, pos));
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*)offsetof(vertex, uv));
        glEnableVertexAttribArray(1);

        glBindVertexArray(0);
    }

    void draw() const {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }

    void destroy() {
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
        glDeleteBuffers(1, &EBO);
    }
};

struct object {

};

class renderer {
public:
    //2d renderer functions
    quad& GetNextFromRQ_2D();
    void LoadRQ_2D();
    void AddToRQ_2D(quad& quad);

    //3d renderer functions, TBI
    quad& GetNextFromRQ_M();
    void LoadRQ_M();
    void AddToRQ_M(object& obj);

    std::vector<quad*> GetRQ();
private:
    //for 2d elements
    std::vector<quad*> quads;
    std::vector<quad*> ui_render_queue;

    //for 3d elements
    std::vector<object*> objects;
    std::vector<object*> m_render_queue;
};


#endif