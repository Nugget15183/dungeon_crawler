//
// Created by conte on 10/19/2026.
//

#ifndef DUNGEONCRAWLER_LOGGER_H
#define DUNGEONCRAWLER_LOGGER_H
#include <iostream>
#include <ostream>

//c

namespace logger {
    inline void info(const char* msg) {
        std::cout << "[INFO] " << msg << " [INFO]" << std::endl;
    }
    inline void warn(const char* msg) {
        std::cout << "[WARN] " << msg << " [WARN]" << std::endl;
    }
    inline void err(const char* msg) {
        std::cout << "[ERRO] " << msg << " [ERRO]" << std::endl;
    }
}

#endif //DUNGEONCRAWLER_LOGGER_H