#include "renderer.h"

#include <algorithm>

#include "logger.h"

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

std::vector<quad*> renderer::GetRQ() {
    return ui_render_queue;
}