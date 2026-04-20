// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015 Intel Corporation. All Rights Reserved.

#include <librealsense/rs.hpp>
#include "example.hpp"

#include <sstream>
#include <iostream>
#include <iomanip>
#include <thread>

texture_buffer buffers[6];

#pragma pack(push, 1)
struct rgb_pixel
{
    uint8_t r,g,b; 
};
#pragma pack(pop)

int main(int argc, char * argv[]) try
{
    rs::log_to_console(rs::log_severity::warn);
    //rs::log_to_file(rs::log_severity::debug, "librealsense.log");

    rs::context ctx;
    if(ctx.get_device_count() == 0) throw std::runtime_error("No device detected. Is it plugged in?");
    rs::device & dev = *ctx.get_device(0);

    const int stream_width = 320, stream_height = 240;
    dev.enable_stream(rs::stream::depth, stream_width, stream_height, rs::format::z16, 30);
    dev.enable_stream(rs::stream::infrared, stream_width, stream_height, rs::format::y8, 30);
    try { dev.enable_stream(rs::stream::infrared2, stream_width, stream_height, rs::format::y8, 30); } catch(...) {}

    bool color_enabled = false;
    dev.start();

    // Open a GLFW window
    glfwInit();
    std::ostringstream ss; ss << "CPP Image Alignment Example (" << dev.get_name() << ")";
    GLFWwindow * win = glfwCreateWindow(1280, 960, ss.str().c_str(), 0, 0);
    glfwMakeContextCurrent(win);

    while (!glfwWindowShouldClose(win))
    {
        // Wait for new images
        glfwPollEvents();
        dev.wait_for_frames();

        // Clear the framebuffer
        int w,h;
        glfwGetFramebufferSize(win, &w, &h);
        glViewport(0, 0, w, h);
        glClear(GL_COLOR_BUFFER_BIT);

        // Draw the images        
        glPushMatrix();
        glfwGetWindowSize(win, &w, &h);
        glOrtho(0, w, h, 0, -1, +1);
        int half_w = w / 2;
        int half_h = h / 2;
        bool has_ir2 = dev.is_stream_enabled(rs::stream::infrared2);

        if(color_enabled)
        {
            buffers[0].show(dev, rs::stream::color, 0, 0, half_w, half_h);
            buffers[1].show(dev, rs::stream::color_aligned_to_depth, half_w, 0, half_w, half_h);
            buffers[2].show(dev, rs::stream::depth_aligned_to_color, 0, half_h, half_w, half_h);
            buffers[3].show(dev, rs::stream::depth, half_w, half_h, half_w, half_h);
        }
        else if(has_ir2)
        {
            buffers[0].show(dev, rs::stream::infrared, 0, 0, half_w, half_h);
            buffers[1].show(dev, rs::stream::infrared2_aligned_to_depth, half_w, 0, half_w, half_h);
            buffers[2].show(dev, rs::stream::depth, 0, half_h, half_w, half_h);
            buffers[3].show(dev, rs::stream::depth_aligned_to_infrared2, half_w, half_h, half_w, half_h);
        }
        else
        {
            buffers[0].show(dev, rs::stream::infrared, 0, 0, half_w, half_h);
            buffers[1].show(dev, rs::stream::depth, half_w, 0, half_w, half_h);
            buffers[2].show(dev, rs::stream::infrared, 0, half_h, half_w, half_h);
            buffers[3].show(dev, rs::stream::depth, half_w, half_h, half_w, half_h);
        }
        glPopMatrix();
        glfwSwapBuffers(win);
    }

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