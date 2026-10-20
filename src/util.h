//
// Created by conte on 10/19/2026.
//

#ifndef DUNGEONCRAWLER_UTIL_H
#define DUNGEONCRAWLER_UTIL_H

#include <fstream>
#include <sstream>
#include <ios>
#include <string>
#include "logger.h"

#include "stb_image.h"

using namespace std;
using namespace logger;

namespace util {

//c

inline string readFile(string path) {
    //get the file itself
    ifstream file(path, ios::in | ios::binary);

    //if the file isnt found, return an empty str
    if (!file) return "";

    //grab the contents of the file
    stringstream ss;
    ss << file.rdbuf();

    //return the contents of the file
    return ss.str();
}

struct image {
    int width, height, channels = 0;
    unsigned char* data = nullptr;

    //default constructor
    image() = default;

    //deletes any copied images
    image(const image&) = delete;

    //same as above, deletes copied images
    image& operator = (const image&) = delete;

    //moves data from one image to another via the ptr, so two copies of the data can never be made
    image(image&& o) noexcept : width(o.width), height(o.height), channels(o.channels), data(o.data) { o.data = nullptr; }

    //when the struct is destroyed, clear out the data so its not left hanging in memory
    ~image() { stbi_image_free(data); }

    //check if the image has data loaded
    bool valid() const { return data != nullptr; }
};

unsigned int compileShader(GLenum type, const std::string& path) {
    //read the file, error if not successfull
    std::string src = readFile(path);
    if (src.empty()) { err(("Could not read shader file: " + path).c_str()); return 0; }

    //read and compile the shader
    const char* csrc = src.c_str();
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &csrc, NULL);
    glCompileShader(shader);

    int succ;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &succ);

    //if not success, error
    if (!succ) {
        char infoLog[1024];
        glGetShaderInfoLog(shader, 1024, NULL, infoLog);
        err(("Shader compile failed (" + path + "): " + infoLog).c_str());
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

inline image loadImage(const string& path, int targetChannels = 0) {
    image img;

    //load the data from the file into the image struct
    img.data = stbi_load(path.c_str(), &img.width, &img.height, &img.channels, targetChannels);

    //confirm a valid amount of channels
    if (targetChannels != 0) img.channels = targetChannels;

    return img;
}

inline unsigned int loadTexture(const string& path) {
    image img = loadImage(path);
    if (!img.valid()) return 0;

    GLenum format = GL_RGB;
    if (img.channels == 1) format = GL_RED;
    else if (img.channels == 3) format = GL_RGB;
    else if (img.channels == 4) format = GL_RGBA;

    unsigned int tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // needed for odd width images
    glTexImage2D(GL_TEXTURE_2D, 0, format, img.width, img.height, 0, format, GL_UNSIGNED_BYTE, img.data);
    glGenerateMipmap(GL_TEXTURE_2D);

    return tex;
}

}

#endif //DUNGEONCRAWLER_UTIL_H