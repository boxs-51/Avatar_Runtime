#include "independent_snapshot.hpp"
#include "PurismCore.h"

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include <fcntl.h>
#include <io.h>

namespace {
using ars::moc3::Drawable;
using ars::moc3::Parameter;
using ars::moc3::Result;
using ars::moc3::Session;
using ars::moc3::Snapshot;

void bytes(std::uint64_t& hash, const void* source, std::size_t length) {
    const auto* data = static_cast<const unsigned char*>(source);
    for (std::size_t i = 0; i < length; ++i)
        hash = (hash ^ data[i]) * 1099511628211ull;
}

std::uint64_t digest(const Snapshot& snapshot) {
    std::uint64_t hash = 14695981039346656037ull;
    for (const Drawable& draw : snapshot.drawables) {
        bytes(hash, draw.id.data(), draw.id.size());
        bytes(hash, &draw.texture_index, sizeof(draw.texture_index));
        bytes(hash, &draw.draw_order, sizeof(draw.draw_order));
        bytes(hash, &draw.render_order, sizeof(draw.render_order));
        bytes(hash, &draw.blend_mode, sizeof(draw.blend_mode));
        bytes(hash, &draw.opacity, sizeof(draw.opacity));
        bytes(hash, &draw.inverted_mask, sizeof(draw.inverted_mask));
        bytes(hash, &draw.double_sided, sizeof(draw.double_sided));
        bytes(hash, &draw.visible, sizeof(draw.visible));
        bytes(hash, &draw.multiply, sizeof(draw.multiply));
        bytes(hash, &draw.screen, sizeof(draw.screen));
        for (const auto& vertex : draw.vertices) bytes(hash, &vertex, sizeof(vertex));
        for (const auto index : draw.indices) bytes(hash, &index, sizeof(index));
        for (const auto mask : draw.masks) bytes(hash, &mask, sizeof(mask));
    }
    return hash;
}

bool count_line(const std::string& line, std::size_t& count) {
    std::istringstream stream(line);
    std::string command, extra;
    if (!(stream >> command >> count) || command != "FRAME" || count > 4096 ||
        stream >> extra) return false;
    return true;
}

bool parameter_line(const std::string& line, Parameter& out) {
    if (line.size() > 512) return false;
    std::istringstream stream(line);
    std::string extra;
    if (!(stream >> out.id >> out.value) || stream >> extra ||
        out.id.size() > 255 || !std::isfinite(out.value)) return false;
    return true;
}
} // namespace

int main(int argc, char** argv) {
    const bool packaged = argc == 2 && std::string(argv[1]) == "--package";
    if (!packaged && argc != 3) return 2;
    if (packaged) _setmode(_fileno(stdin), _O_BINARY);
    std::unique_ptr<Session> session;
    if (!packaged) {
      const Result opened = Session::open(std::filesystem::path(argv[1]),
                                        std::filesystem::path(argv[2]),
                                        ars::moc3::Budget{}, session);
    if (opened != Result::ok || !session || !session->current()) {
        std::cout << "ERR " << static_cast<int>(opened) << std::endl;
        return 1;
      }
    }
    auto current = session ? session->current() : nullptr;
    std::shared_ptr<const Snapshot> bookmark;
    if (packaged) std::cout << "BOOT" << std::endl;
    else std::cout << "READY " << current->generation << ' ' << digest(*current) << std::endl;
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.size() > 64) {
            std::cout << "ERR protocol" << std::endl;
            continue;
        }
        if (line == "QUIT") {
            std::cout << "BYE" << std::endl;
            return 0;
        }
        if (packaged && line.rfind("LOAD ", 0) == 0) {
            std::istringstream input(line);
            std::string command, extra;
            std::size_t size = 0, atlases = 0;
            int fault = -1;
            if (!(input >> command >> size >> atlases >> fault) || input >> extra ||
                size > ars::moc3::Budget{}.max_moc_bytes || fault < -1) return 2;
            try {
                std::vector<std::uint8_t> blob(size);
                if (!std::cin.read(reinterpret_cast<char*>(blob.data()), size)) return 2;
                const auto opened = Session::open_blob(blob, atlases, ars::moc3::Budget{},
                                                       session, fault);
                if (opened != Result::ok) {
                    std::cout << "ERR " << static_cast<int>(opened) << std::endl;
                    continue;
                }
                current = session->current();
                std::cout << "READY " << current->generation << ' ' << digest(*current) << std::endl;
            } catch (const std::bad_alloc&) {
                // Framing cannot safely continue after a failed blob allocation.
                return 3;
            }
            continue;
        }
        if (!session) { std::cout << "ERR no_session" << std::endl; continue; }
        if (line == "MARK") {
            bookmark = session->current();
            std::cout << "MARKED" << std::endl;
            continue;
        }
        if (line == "COMPARE") {
            std::cout << (bookmark && bookmark->drawables == session->current()->drawables
                              ? "SAME" : "DIFFERENT") << std::endl;
            continue;
        }
        if (line == "STATE") {
            current = session->current();
            std::cout << "STATE " << current->generation << ' ' << digest(*current) << std::endl;
            continue;
        }
        if (line == "INFO") {
            std::size_t masked = 0, inverted = 0, additive = 0, multiply = 0;
            int max_texture = -1;
            for (const auto& draw : session->current()->drawables) {
                masked += !draw.masks.empty(); inverted += draw.inverted_mask;
                additive += draw.blend_mode == csmColorBlendType_AddCompatible;
                multiply += draw.blend_mode == csmColorBlendType_MultiplyCompatible;
                if (draw.texture_index > max_texture) max_texture = draw.texture_index;
            }
            std::cout << "INFO " << session->current()->drawables.size() << ' '
                      << masked << ' ' << inverted << ' ' << additive << ' '
                      << multiply << ' ' << max_texture << std::endl;
            continue;
        }
        if (line.rfind("FAULT ", 0) == 0) {
            std::istringstream input(line.substr(6));
            int value; std::string extra;
            if (!(input >> value) || value < -1 || input >> extra) {
                std::cout << "ERR protocol" << std::endl; continue;
            }
            session->fail_snapshot_checkpoint_after(value);
            std::cout << "FAULT_SET" << std::endl;
            continue;
        }
        std::size_t count = 0;
        if (!count_line(line, count)) {
            std::cout << "ERR protocol" << std::endl;
            continue;
        }
        try {
            std::vector<Parameter> parameters;
            parameters.reserve(count);
            bool valid = true;
            for (std::size_t i = 0; i < count; ++i) {
                if (!std::getline(std::cin, line)) return 2;
                Parameter parameter;
                if (!parameter_line(line, parameter)) valid = false;
                parameters.push_back(std::move(parameter));
            }
            if (!valid) {
                std::cout << "ERR protocol" << std::endl;
                continue;
            }
            const Result result = session->update_complete(parameters);
            if (result != Result::ok || !session->current()) {
                std::cout << "ERR " << static_cast<int>(result) << std::endl;
                continue;
            }
            current = session->current();
            std::cout << "OK " << current->generation << ' ' << digest(*current) << std::endl;
        } catch (const std::bad_alloc&) {
            std::cout << "ERR allocation" << std::endl;
        } catch (const std::length_error&) {
            std::cout << "ERR budget" << std::endl;
        }
    }
    return 0;
}
