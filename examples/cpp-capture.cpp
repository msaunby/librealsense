// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015 Intel Corporation. All Rights Reserved.

#include <librealsense/rs.hpp>
#include "example.hpp"

#include <sstream>
#include <iostream>
#include <iomanip>
#include <thread>
#include <algorithm>
#include <map>
#include <memory>
#include <cstdlib>
#include <string>
#include <vector>

texture_buffer buffers[RS_STREAM_COUNT];

// Split the screen into 640X480 tiles, according to the number of supported streams. Define layout as follows : tiles -> <columds,rows>
const std::map<size_t, std::pair<int, int>> tiles_map = {       { 1,{ 1,1 } },
                                                                { 2,{ 2,1 } },
                                                                { 3,{ 2,2 } },
                                                                { 4,{ 2,2 } },
                                                                { 5,{ 3,2 } },          // E.g. five tiles, split into 3 columns by 2 rows mosaic
                                                                { 6,{ 3,2 } }};

int main(int argc, char * argv[]) try
{
    rs::log_to_console(rs::log_severity::warn);
    //rs::log_to_file(rs::log_severity::debug, "librealsense.log");

    rs::context ctx;
    const int device_count = ctx.get_device_count();
    if (device_count == 0) throw std::runtime_error("No device detected. Is it plugged in?");
    int device_index = 0;

    bool try_color = false;
    for(int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if(arg == "-h" || arg == "--help")
        {
            std::cout << "Usage: cpp-capture [device_index] [--try-color]\n";
            std::cout << "  device_index   Optional camera index, default 0\n";
            std::cout << "  --try-color    Try color-only mode (640x480 yuyv@60) before fallback to depth/infrared\n";
            return EXIT_SUCCESS;
        }
        if(arg == "--try-color")
        {
            try_color = true;
            continue;
        }
        device_index = std::atoi(arg.c_str());
    }

    if(device_index < 0 || device_index >= device_count)
    {
        std::ostringstream err;
        err << "Requested device index " << device_index << " is out of range. "
            << "Detected " << device_count << " device(s).";
        for(int i = 0; i < device_count; ++i)
        {
            rs::device * listed = ctx.get_device(i);
            err << "\n  [" << i << "] " << listed->get_name() << " (serial " << listed->get_serial() << ")";
        }
        throw std::runtime_error(err.str());
    }
    rs::device & dev = *ctx.get_device(device_index);
    std::cout << "Using device index " << device_index << ", serial " << dev.get_serial() << std::endl;

    auto clear_all_streams = [&dev]()
    {
        if(dev.is_streaming()) dev.stop();
        const rs::stream native_streams[] = {
            rs::stream::depth,
            rs::stream::color,
            rs::stream::infrared,
            rs::stream::infrared2,
            rs::stream::fisheye
        };
        for(auto stream : native_streams)
        {
            if(dev.is_stream_enabled(stream)) dev.disable_stream(stream);
        }
    };

    auto configure_depth_ir = [&dev]()
    {
        const int stream_width = 320, stream_height = 240;
        dev.enable_stream(rs::stream::depth, stream_width, stream_height, rs::format::z16, 30);
        dev.enable_stream(rs::stream::infrared, stream_width, stream_height, rs::format::y8, 30);
        try { dev.enable_stream(rs::stream::infrared2, stream_width, stream_height, rs::format::y8, 30); }
        catch(...) { std::cout << "Device does not provide infrared2 stream." << std::endl; }
    };

    std::vector<rs::stream> supported_streams;
    bool color_mode_active = false;
    clear_all_streams();
    if(try_color)
    {
        std::cout << "Trying color-only mode: 640x480 yuyv @ 60..." << std::endl;
        try
        {
            dev.enable_stream(rs::stream::color, 640, 480, rs::format::yuyv, 60);
            dev.start();
            dev.wait_for_frames();
            dev.wait_for_frames();
            std::cout << "Color mode started successfully." << std::endl;
            supported_streams.push_back(rs::stream::color);
            color_mode_active = true;
        }
        catch(const rs::error & e)
        {
            std::cout << "Color probe failed, falling back to depth/infrared." << std::endl;
            std::cout << "  " << e.get_failed_function() << "(" << e.get_failed_args() << "): " << e.what() << std::endl;
            clear_all_streams();
        }
    }

    if(!color_mode_active)
    {
        configure_depth_ir();
        supported_streams = { rs::stream::depth, rs::stream::infrared };
        if (dev.is_stream_enabled(rs::stream::infrared2)) supported_streams.push_back(rs::stream::infrared2);
    }

    // Compute field of view for each enabled stream
    for (auto & stream : supported_streams)
    {
        if (!dev.is_stream_enabled(stream)) continue;
        auto intrin = dev.get_stream_intrinsics(stream);
        std::cout << "Capturing " << stream << " at " << intrin.width << " x " << intrin.height;
        std::cout << std::setprecision(1) << std::fixed << ", fov = " << intrin.hfov() << " x " << intrin.vfov() << ", distortion = " << intrin.model() << std::endl;
    }

    // Start our device when not already started by color probe
    if(!dev.is_streaming()) dev.start();

    // Open a GLFW window
    glfwInit();
    std::ostringstream ss; ss << "CPP Capture Example (" << dev.get_name() << ")";

    int rows = tiles_map.at(supported_streams.size()).second;
    int cols = tiles_map.at(supported_streams.size()).first;
    int tile_w = 640; // pixels
    int tile_h = 480; // pixels
    GLFWwindow * win = glfwCreateWindow(tile_w*cols, tile_h*rows, ss.str().c_str(), 0, 0);
    glfwSetWindowUserPointer(win, &dev);
    glfwSetKeyCallback(win, [](GLFWwindow * win, int key, int scancode, int action, int mods)
    {
        auto dev = reinterpret_cast<rs::device *>(glfwGetWindowUserPointer(win));
        if (action != GLFW_RELEASE) switch (key)
        {
        case GLFW_KEY_E:
            if (dev->supports_option(rs::option::r200_emitter_enabled))
            {
                int value = !dev->get_option(rs::option::r200_emitter_enabled);
                std::cout << "Setting emitter to " << value << std::endl;
                dev->set_option(rs::option::r200_emitter_enabled, value);
            }
            break;
        case GLFW_KEY_A:
            if (dev->supports_option(rs::option::r200_lr_auto_exposure_enabled))
            {
                int value = !dev->get_option(rs::option::r200_lr_auto_exposure_enabled);
                std::cout << "Setting auto exposure to " << value << std::endl;
                dev->set_option(rs::option::r200_lr_auto_exposure_enabled, value);
            }
            break;
        }
    });
    glfwMakeContextCurrent(win);

    while (!glfwWindowShouldClose(win))
    {
        // Wait for new images
        glfwPollEvents();
        dev.wait_for_frames();

        // Clear the framebuffer
        int w, h;
        glfwGetFramebufferSize(win, &w, &h);
        glViewport(0, 0, w, h);
        glClear(GL_COLOR_BUFFER_BIT);

        // Draw the images
        glPushMatrix();
        glfwGetWindowSize(win, &w, &h);
        glOrtho(0, w, h, 0, -1, +1);
        for (size_t i = 0; i < supported_streams.size(); ++i)
        {
            int col = (int)(i % cols);
            int row = (int)(i / cols);
            buffers[i].show(dev, supported_streams[i], col * tile_w, row * tile_h, tile_w, tile_h);
        }
        glPopMatrix();
        glfwSwapBuffers(win);
    }

    glfwDestroyWindow(win);
    glfwTerminate();
    return EXIT_SUCCESS;
}
catch (const rs::error & e)
{
    std::cerr << "RealSense error calling " << e.get_failed_function() << "(" << e.get_failed_args() << "):\n    " << e.what() << std::endl;
    return EXIT_FAILURE;
}
catch (const std::exception & e)
{
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
}
