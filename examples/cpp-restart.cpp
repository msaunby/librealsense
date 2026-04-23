// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015 Intel Corporation. All Rights Reserved.

#include <librealsense/rs.hpp>
#include "example.hpp"

#define _USE_MATH_DEFINES
#include <math.h>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <thread>
#include <string>
#include <vector>

texture_buffer buffers[RS_STREAM_COUNT];

struct color_mode_case
{
    int width;
    int height;
    rs::format format;
    int fps;
    const char * label;
};

static const char * format_to_string(rs::format format)
{
    switch(format)
    {
    case rs::format::yuyv: return "yuyv";
    case rs::format::rgb8: return "rgb8";
    case rs::format::bgr8: return "bgr8";
    case rs::format::rgba8: return "rgba8";
    case rs::format::bgra8: return "bgra8";
    default: return "unknown";
    }
}

static int run_color_diagnostics(rs::device & dev)
{
    const std::vector<color_mode_case> test_modes = {
        {640, 480, rs::format::yuyv, 30, "VGA yuyv 30"},
        {640, 480, rs::format::yuyv, 60, "VGA yuyv 60"},
        {640, 480, rs::format::rgb8, 30, "VGA rgb8 30"},
        {640, 480, rs::format::rgb8, 60, "VGA rgb8 60"},
        {640, 480, rs::format::bgr8, 30, "VGA bgr8 30"},
        {640, 480, rs::format::bgr8, 60, "VGA bgr8 60"},
        {640, 480, rs::format::rgba8, 30, "VGA rgba8 30"},
        {640, 480, rs::format::bgra8, 30, "VGA bgra8 30"},
        {1920, 1080, rs::format::yuyv, 30, "1080p yuyv 30"},
        {1920, 1080, rs::format::rgb8, 30, "1080p rgb8 30"}
    };

    std::cout << "Running color diagnostics on serial " << dev.get_serial() << std::endl;
    int passed = 0;

    for(size_t i = 0; i < test_modes.size(); ++i)
    {
        const auto & mode = test_modes[i];
        std::cout << "[" << i + 1 << "/" << test_modes.size() << "] " << mode.label
                  << " (" << mode.width << "x" << mode.height << " "
                  << format_to_string(mode.format) << " @ " << mode.fps << " fps): ";
        try
        {
            if(dev.is_streaming()) dev.stop();
            for(int s = 0; s < 4; ++s)
            {
                auto stream = (rs::stream)s;
                if(dev.is_stream_enabled(stream)) dev.disable_stream(stream);
            }

            dev.enable_stream(rs::stream::color, mode.width, mode.height, mode.format, mode.fps);
            dev.start();
            dev.wait_for_frames();
            dev.wait_for_frames();
            std::cout << "PASS" << std::endl;
            ++passed;
        }
        catch(const rs::error & e)
        {
            std::cout << "FAIL" << std::endl;
            std::cout << "    function: " << e.get_failed_function() << "(" << e.get_failed_args() << ")" << std::endl;
            std::cout << "    message:  " << e.what() << std::endl;
        }
    }

    if(dev.is_streaming()) dev.stop();
    std::cout << "Color diagnostics summary: " << passed << " / " << test_modes.size() << " modes passed." << std::endl;
    return passed > 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

int main(int argc, char * argv[]) try
{
    rs::log_to_console(rs::log_severity::warn);
    //rs::log_to_file(rs::log_severity::debug, "librealsense.log");

    rs::context ctx;
    if(ctx.get_device_count() == 0) throw std::runtime_error("No device detected. Is it plugged in?");

    int device_index = 0;
    bool color_only = false;
    bool color_diag = false;
    if(argc > 1) device_index = std::atoi(argv[1]);
    if(argc > 2 && std::string(argv[2]) == "color") color_only = true;
    if(argc > 2 && std::string(argv[2]) == "diag-color") color_diag = true;
    if(device_index < 0 || device_index >= ctx.get_device_count()) throw std::runtime_error("Requested device index is out of range");

    rs::device & dev = *ctx.get_device(device_index);
    std::cout << "Using device index " << device_index << ", serial " << dev.get_serial() << std::endl;

    if(color_diag)
    {
        return run_color_diagnostics(dev);
    }

    // Open a GLFW window
    glfwInit();
    std::ostringstream ss; ss << "CPP Restart Example (" << dev.get_name() << ")";
    GLFWwindow * win = glfwCreateWindow(1280, 960, ss.str().c_str(), 0, 0);
    glfwMakeContextCurrent(win);

    int first_mode = 0;
    int last_mode = color_only ? 5 : 19;
    for(int i = first_mode; i <= last_mode; ++i)
    {
        try
        {
            if(dev.is_streaming()) dev.stop();

            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            for(int j=0; j<4; ++j)
            {
                auto s = (rs::stream)j;
                if(dev.is_stream_enabled(s)) dev.disable_stream(s);
            }

            switch(i)
            {
            case 0:
                dev.enable_stream(rs::stream::color, 640, 480, rs::format::yuyv, 60);
                break;
            case 1:
                dev.enable_stream(rs::stream::color, 640, 480, rs::format::rgb8, 60);
                break;
            case 2:
                dev.enable_stream(rs::stream::color, 640, 480, rs::format::bgr8, 60);
                break;
            case 3:
                dev.enable_stream(rs::stream::color, 640, 480, rs::format::rgba8, 60);
                break;
            case 4:
                dev.enable_stream(rs::stream::color, 640, 480, rs::format::bgra8, 60);
                break;
            case 5:
                dev.enable_stream(rs::stream::color, 1920, 1080, rs::format::rgb8, 0);
                break;
            case 6:
                dev.enable_stream(rs::stream::depth, rs::preset::largest_image);
                break;
            case 7:
                dev.enable_stream(rs::stream::depth, 480, 360, rs::format::z16, 60);
                break;
            case 8:
                dev.enable_stream(rs::stream::depth, 320, 240, rs::format::z16, 60);
                break;
            case 9:
                dev.enable_stream(rs::stream::infrared, rs::preset::largest_image);
                break;
            case 10:
                dev.enable_stream(rs::stream::infrared, 492, 372, rs::format::y8, 60);
                break;
            case 11:
                dev.enable_stream(rs::stream::infrared, 320, 240, rs::format::y8, 60);
                break;
            case 12:
                dev.enable_stream(rs::stream::infrared, 0, 0, rs::format::y16, 60);
                break;
            case 13:
                dev.enable_stream(rs::stream::infrared, 0, 0, rs::format::y8, 60);
                dev.enable_stream(rs::stream::infrared2, 0, 0, rs::format::y8, 60);
                break;
            case 14:
                dev.enable_stream(rs::stream::infrared, 0, 0, rs::format::y16, 60);
                dev.enable_stream(rs::stream::infrared2, 0, 0, rs::format::y16, 60);
                break;
            case 15:
                dev.enable_stream(rs::stream::depth, rs::preset::best_quality);
                dev.enable_stream(rs::stream::infrared, 0, 0, rs::format::y8, 0);
                break;
            case 16:
                dev.enable_stream(rs::stream::depth, rs::preset::best_quality);
                dev.enable_stream(rs::stream::infrared, 0, 0, rs::format::y16, 0);
                break;
            case 17:
                dev.enable_stream(rs::stream::depth, rs::preset::best_quality);
                dev.enable_stream(rs::stream::color, rs::preset::best_quality);
                break;
            case 18:
                dev.enable_stream(rs::stream::depth, rs::preset::best_quality);
                dev.enable_stream(rs::stream::color, rs::preset::best_quality);
                dev.enable_stream(rs::stream::infrared, rs::preset::best_quality);
                break;
            case 19:
                dev.enable_stream(rs::stream::depth, rs::preset::best_quality);
                dev.enable_stream(rs::stream::color, rs::preset::best_quality);
                dev.enable_stream(rs::stream::infrared, rs::preset::best_quality);
                dev.enable_stream(rs::stream::infrared2, rs::preset::best_quality);
                break;
            }

            dev.start();
            for(int j=0; j<120; ++j)
            {
                // Wait for new images
                glfwPollEvents();
                if(glfwWindowShouldClose(win)) goto done;
                dev.wait_for_frames();

                // Clear the framebuffer
                int w,h;
                glfwGetWindowSize(win, &w, &h);
                glViewport(0, 0, w, h);
                glClear(GL_COLOR_BUFFER_BIT);

                // Draw the images
                glPushMatrix();
                glfwGetWindowSize(win, &w, &h);
                glOrtho(0, w, h, 0, -1, +1);
                buffers[0].show(dev, rs::stream::color, 0, 0, w/2, h/2);
                buffers[1].show(dev, rs::stream::depth, w/2, 0, w-w/2, h/2);
                buffers[2].show(dev, rs::stream::infrared, 0, h/2, w/2, h-h/2);
                buffers[3].show(dev, rs::stream::infrared2, w/2, h/2, w-w/2, h-h/2);
                glPopMatrix();
                glfwSwapBuffers(win);
            }
        }
        catch(const rs::error & e)
        {
            std::cerr << "RealSense error calling " << e.get_failed_function() << "(" << e.get_failed_args() << "):\n    " << e.what() << std::endl;
            std::cout << "Skipping mode " << i << std::endl;
        }
    }
done:
    glfwDestroyWindow(win);
    glfwTerminate();
    return EXIT_SUCCESS;
}
catch(const rs::error & e)
{
    std::cerr << "RealSense error calling " << e.get_failed_function() << "(" << e.get_failed_args() << "):\n    " << e.what() << std::endl;
    return EXIT_FAILURE;
}
catch(const std::exception & e)
{
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
}
