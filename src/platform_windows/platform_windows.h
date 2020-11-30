
#pragma once

#include "platform.h"

#include <windows.h>

struct Windows : public Platform
{
    static HWND handle();
    static HDC device_context();
};

