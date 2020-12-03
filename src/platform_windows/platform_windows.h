
#pragma once

#include "platform.h"

#include <Windows.h>

struct Windows : public Platform
{
    static HWND handle();
    static HDC device_context();
};

