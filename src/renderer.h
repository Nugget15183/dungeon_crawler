#ifndef DUNGEONCRAWLER_RENDERER_H
#define DUNGEONCRAWLER_RENDERER_H

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

using namespace glm;

struct vert2d {
    vec2 pos;
    vec2 uv;
};

struct vert3d {
    vec3 pos;
    vec2 uv;
    vec4 rect; //atlas tile
    vec3 norm;
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
    quad(vec2 pos, vec2 size, UVRect uv = {0.0f, 0.0f, 1.0f, 1.0f}, double cTimeOffset = 0) {
        float hx = 0.5f * size.x;
        float hy = 0.5f * size.y;

        verts[0] = {{ hx,  hy}, {uv.u1, uv.v1}}; // top right
        verts[1] = {{ hx, -hy}, {uv.u1, uv.v0}}; // bottom right
        verts[2] = {{-hx, -hy}, {uv.u0, uv.v0}}; // bottom left
        verts[3] = {{-hx,  hy}, {uv.u0, uv.v1}}; // top left

        creationTime = glfwGetTime()+cTimeOffset;

        this->pos = pos;
        this->uv = uv;
    }

    std::vector<UVRect> frames;
    int curFrame=0;

    UVRect uv;
    vert2d verts[4];

    unsigned int idx[6] = {0, 1, 3, 1, 2, 3};
    unsigned int VAO = 0, VBO = 0, EBO = 0;
    unsigned int texture = 0;

    bool visible = true;

    int z = 0;

    double creationTime = 0;

    vec2 pos;
    vec4 col = {1,1,1, 1};

    std::function<void(quad*)> onUpdate = nullptr;

    void setUV(const UVRect& newuv) {
        uv=newuv;
        verts[0].uv = {uv.u1, uv.v1};
        verts[1].uv = {uv.u1, uv.v0};
        verts[2].uv = {uv.u0, uv.v0};
        verts[3].uv = {uv.u0, uv.v1};

        if (VBO) {
            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);
        }
    }

    void addFrame(const UVRect& r) {frames.push_back(r);}
    void addFrame(int tx, int ty, int ATLAS_TILES=16){ frames.push_back(tileUV(tx, ty, ATLAS_TILES));}

    void setFrame(int i) {
        if (frames.empty()) { return; }

        //wrap i around existing frame count
        i%=frames.size();
        if (i<0)i+=frames.size();

        //dont waste time updating
        if (i==curFrame)return;

        //update gpu
        curFrame=i;
        setUV(frames.at(i));
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

        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(vert2d), (void*)offsetof(vert2d, pos));
        glEnableVertexAttribArray(0);

        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(vert2d), (void*)offsetof(vert2d, uv));
        glEnableVertexAttribArray(1);

        glBindVertexArray(0);
    }

    void update() {
        if (onUpdate != nullptr) onUpdate(this);
    }

    void draw() const {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);

        glBindVertexArray(VAO);
        glVertexAttrib2f(2, pos.x, pos.y);
        glVertexAttrib4f(3, col.r, col.g, col.b, col.a);

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }

    void destroy() {
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
        glDeleteBuffers(1, &EBO);
    }
};

//data
struct Particle {
    vec2 pos{0.0f}, vel{0.0f}, accel{0.0f};
    vec4 col={1.0f,1.0f,1.0f,1.0f};
    vec4 colEnd={1.0f,1.0f,1.0f,1.0f};
    float size = 0.02f, sizeEnd = 0.0f;
    float life=0.0f;
    float maxLife=1.0f;
    float emissive=0.0f;
};

//sent to gpu
struct ParticleInst {
    vec2 pos;
    float size;
    vec4 col;
    float emissive;
};

//for 3d elements
struct object {
    object(vec3 pos, vec3 size, UVRect uv = {0.0f, 0.0f, 1.0f, 1.0f}, double cTimeOffset = 0) {
        this->pos = pos;
        this->size = size;
        creationTime = glfwGetTime() + cTimeOffset;

        vec3 normals[6] = {
            { 0, 0, 1}, { 0, 0,-1},   // +Z, -Z
            { 1, 0, 0}, {-1, 0, 0},   // +X, -X
            { 0, 1, 0}, { 0,-1, 0},   // +Y, -Y
        };

        vec3 corners[6][4] = {
            {{-.5f,-.5f, .5f}, { .5f,-.5f, .5f}, { .5f, .5f, .5f}, {-.5f, .5f, .5f}}, // +Z
            {{ .5f,-.5f,-.5f}, {-.5f,-.5f,-.5f}, {-.5f, .5f,-.5f}, { .5f, .5f,-.5f}}, // -Z
            {{ .5f,-.5f, .5f}, { .5f,-.5f,-.5f}, { .5f, .5f,-.5f}, { .5f, .5f, .5f}}, // +X
            {{-.5f,-.5f,-.5f}, {-.5f,-.5f, .5f}, {-.5f, .5f, .5f}, {-.5f, .5f,-.5f}}, // -X
            {{-.5f, .5f, .5f}, { .5f, .5f, .5f}, { .5f, .5f,-.5f}, {-.5f, .5f,-.5f}}, // +Y
            {{-.5f,-.5f,-.5f}, { .5f,-.5f,-.5f}, { .5f,-.5f, .5f}, {-.5f,-.5f, .5f}}, // -Y
        };

        const vec2 uvs[4] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
        vec4 rect = {uv.u0, uv.v0, uv.u1 - uv.u0, uv.v1 - uv.v0};

        for (int f = 0; f < 6; f++) {
            unsigned int b = f * 4;
            for (int c = 0; c < 4; c++) verts[b + c] = {corners[f][c], uvs[c], rect, normals[f]};

            unsigned int* i = &idx[f * 6];
            i[0] = b; i[1] = b + 1; i[2] = b + 2;
            i[3] = b; i[4] = b + 2; i[5] = b + 3;
        }
        applyUVScale();
    }

    vert3d verts[24];
    unsigned int idx[36];

    vec3 size = {1, 1, 1};
    vec3 pos = {0, 0, 0};
    vec3 rot = {0, 0, 0};

    unsigned int VAO = 0, VBO = 0, EBO = 0;
    unsigned int texture = 0;

    bool visible = true;
    double creationTime = 0;

    std::function<void(object*)> onUpdate = nullptr;

    mat4 modelMatrix() {
        mat4 m = translate(mat4(1.0f), pos);
        m = rotate(m, rot.y, vec3(0, 1, 0));
        m = rotate(m, rot.x, vec3(1, 0, 0));
        m = rotate(m, rot.z, vec3(0, 0, 1));
        return scale(m, size);
    }

    void setFaceUV(int face, const UVRect& uv) {
        vec4 rect = {uv.u0, uv.v0, uv.u1 - uv.u0, uv.v1 - uv.v0};
        unsigned int b = face * 4;
        for (int c = 0; c < 4; c++) verts[b + c].rect = rect;

        if (VBO) {
            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            glBufferSubData(GL_ARRAY_BUFFER, b * sizeof(vert3d), 4 * sizeof(vert3d), &verts[b]);
        }
    }

    void setTile(int face, int tx, int ty, int atlasTiles = 16) {
        setFaceUV(face, tileUV(tx, ty, atlasTiles));
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

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vert3d), (void*)offsetof(vert3d, pos));
        glEnableVertexAttribArray(0);

        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(vert3d), (void*)offsetof(vert3d, uv));
        glEnableVertexAttribArray(1);

        glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(vert3d), (void*)offsetof(vert3d, rect));
        glEnableVertexAttribArray(2);

        glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(vert3d), (void*)offsetof(vert3d, norm));
        glEnableVertexAttribArray(3);

        glBindVertexArray(0);
    }

    void applyUVScale() {
        const vec2 span[6] = {
            {size.x, size.y}, {size.x, size.y},      // +Z, -Z
            {size.z, size.y}, {size.z, size.y},  // +X, -X
            {size.x, size.z}, {size.x, size.z},  // +Y, -Y
        };

        const vec2 uvs[4] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};

        for (int f = 0; f < 6; f++) {
            for (int c = 0; c < 4; c++) {
                verts[f * 4 + c].uv = uvs[c] * span[f];
            }
        }

        if (VBO) {
            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);
        }
    }

    void update() {
        if (onUpdate != nullptr) onUpdate(this);
    }

    // renderer sets the umodel unif prior
    void draw() const {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);

        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }

    void destroy() {
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
        glDeleteBuffers(1, &EBO);
        VAO = VBO = EBO = 0;
    }
};

struct light : object {
    light(const object& o) : object(o) {}
    float intensity = 1.0f;
    float radius = 8.0f;
    vec3 color = {1.0f, 0.8f, 0.5f}; //orange
    bool enabled = true;
};

struct camera {
    vec3 pos = {0,0,0};

    float yaw = 0.0f;
    float pitch = -70.0f;
    float fov = 70.0f;

    float nearp = 0.1f;
    float farp = 100.0f;

    vec3 foward() {
        float y = radians(yaw);
        float p = radians(pitch);
        return normalize(vec3(cos(y) * cos(p), sin(p), sin(y) * cos(p)));
    }

    vec3 right() {
        return normalize(cross(foward(), vec3(0, 1, 0)));
    }

    mat4 view() {
        return lookAt(pos, pos + foward(), vec3(0, 1, 0));
    }

    mat4 proj(float aspect) {
        return perspective(radians(fov), aspect, nearp, farp);
    }
};

enum lightingSetting {
    OUTSIDE,
    DUNGEON,
    MOONLIT,
};

class renderer {
public:
    camera cam;

    //2d renderer functions
    quad& GetNextFromRQ_2D();
    void LoadRQ_2D();
    void AddToRQ_2D(quad& quad);
    std::vector<quad*> GetRQ_2D();
    void Draw2D(unsigned int program, float aspect);

    //3d renderer functions
    object& GetNextFromRQ_M();
    void LoadRQ_M();
    void AddToRQ_M(object& obj);
    void Draw3D(unsigned int program, float aspect, lightingSetting lighting);
    std::vector<object*> GetRQ_3D();

    std::vector<quad*> GetAll2D() {return quads;}
    std::vector<object*> GetAll3D() {return objects;}
    void AddLight(light& l) { lights.push_back(&l); }

    void InitParticles(size_t capacity = 4096);
    void EmitParticle(const Particle& p);
    void UpdateParticles(float dt);
    void DrawParticles(unsigned int program, float aspect);
    void DestroyParticles();
private:
    //for 2d elements
    std::vector<quad*> quads;
    std::vector<quad*> ui_render_queue;

    //for 3d elements
    std::vector<object*> objects;
    std::vector<object*> m_render_queue;
    std::vector<light*> lights;

    //particles
    std::vector<Particle> particles;
    std::vector<ParticleInst> pInst;
    size_t pNext = 0;
    unsigned int pVAO = 0, pQuadVBO = 0, pInstVBO = 0;
};


#endif