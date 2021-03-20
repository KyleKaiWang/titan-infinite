/*
 * Vulkan Renderer Program
 *
 * Copyright (C) 2020 Kyle Wang
 */
#pragma once
#include "app.h"

int main()
{
    App* app = create_application();
    app->init();
    try {
        app->run();
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        system("pause");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
