
#include "platform.h"

#include "engine.h"
#include "input.h"

#include <stdlib.h> // malloc, free
#include <stdio.h>
#include <stdarg.h>



struct PlatformState
{
    struct WindowState
    {
        HWND handle;
        int width;
        int height;
    } window;

    FILE *log;

    bool want_to_close;

    LARGE_INTEGER start_time_counter;
};
PlatformState *Platform::instance = nullptr;

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    if(ImGui_ImplWin32_WndProcHandler(window, message, wParam, lParam)) return true;

    LRESULT result = 0; 

    switch (message)
    {
        case WM_DESTROY:
        {
            PostQuitMessage(0);
            return 0;
        } break;
        case WM_CLOSE: 
        {
            DestroyWindow(window);
            return 0;
        } break;
        case WM_KEYDOWN:
        {
            Input::record_key_event(wParam, true);
        } break;
        case WM_KEYUP:  
        {
            Input::record_key_event(wParam, false);
        } break;
        case WM_LBUTTONDOWN:
        {
            Input::record_mouse_event(0, true);
        } break;
        case WM_LBUTTONUP:  
        {
            Input::record_mouse_event(0, false);
        } break;
        default:
        {
            result = DefWindowProc(window, message, wParam, lParam);
        } break;
    }

    return result;
}

void Platform::init()
{
    instance = (PlatformState *)Platform::Memory::allocate(sizeof(PlatformState));
    Platform::Memory::memset(instance, 0, sizeof(PlatformState));

    QueryPerformanceCounter(&instance->start_time_counter);

    // Create the window class
    WNDCLASS window_class = {};

    window_class.style = CS_HREDRAW|CS_VREDRAW|CS_OWNDC;
    window_class.lpfnWndProc = WindowProc;
    window_class.hInstance = GetModuleHandle(NULL);
    window_class.hCursor = LoadCursor(NULL, IDC_ARROW);
    window_class.lpszClassName = "Windows Program Class";

    if(!RegisterClass(&window_class))
    {
        log_error("Could not register Windows class!");
        return;
    }

    BOOL result;
    RECT rect;
    result = SystemParametersInfoA(SPI_GETWORKAREA, 0, &rect, 0);
    unsigned window_width = rect.right;
    unsigned window_height = rect.bottom;
    //unsigned window_width = GetSystemMetrics(SM_CXSCREEN);
    //unsigned window_height = GetSystemMetrics(SM_CYSCREEN);


    // Create the window
    instance->window.handle = CreateWindowEx(0,                  // Extended style
            window_class.lpszClassName,        // Class name
            "",                           // Window name
            WS_OVERLAPPEDWINDOW | WS_VISIBLE,  // Style of the window
            0,                                 // Initial X position
            0,                                 // Initial Y position
            window_width,                     // Initial width
            window_height,                    // Initial height
            0,                                 // Handle to the window parent
            0,                                 // Handle to a menu
            GetModuleHandle(NULL),        // Handle to an instance
            0);

    if(!instance->window.handle)
    {
        log_error("Could not open window!");
        return;
    }

    RECT client_rect;
    BOOL success = GetClientRect(instance->window.handle, &client_rect);

    instance->window.width = client_rect.right;
    instance->window.height = client_rect.bottom;

    result = CreateDirectory("output", NULL);
    DWORD create_dir_result = GetLastError();
    if(result || ERROR_ALREADY_EXISTS == create_dir_result)
    {
        // CopyFile(...)
        instance->log = fopen("output/log.txt", "w");
    }
    else
    {
        // Failed to create directory.
    }
}

void Platform::handle_os_events()
{
    MSG message;
    while(PeekMessage(&message, 0, 0, 0, PM_REMOVE))
    {
        if(message.message == WM_QUIT)
        {
            instance->want_to_close = true;
        }

        TranslateMessage(&message);
        DispatchMessage(&message);
    }

    if(instance->want_to_close)
    {
        stop_engine();
    }
}

double Platform::time_since_start()
{
    LARGE_INTEGER counts_per_second;
    QueryPerformanceFrequency(&counts_per_second);

    LARGE_INTEGER current_time;
    QueryPerformanceCounter(&current_time);

    LONGLONG counts_passed = current_time.QuadPart - instance->start_time_counter.QuadPart;

    double dt = (double)counts_passed / counts_per_second.QuadPart;

    return dt;
}

bool Platform::want_to_close()
{
    return instance->want_to_close;
}

HWND Platform::Window::handle()
{
    return instance->window.handle; 
}

HDC Platform::Window::device_context()
{
    return GetDC(instance->window.handle); 
}

int Platform::Window::screen_width()
{
    return instance->window.width;  
}

int Platform::Window::screen_height()
{
    return instance->window.height; 
}

int Platform::Window::monitor_frequency()
{
    int result = 0;
    BOOL devices_result;

    DISPLAY_DEVICEA display_device;
    display_device.cb = sizeof(DISPLAY_DEVICEA);
    DWORD flags = EDD_GET_DEVICE_INTERFACE_NAME;
    DWORD device_num = 0;
    do
    {
        devices_result = EnumDisplayDevicesA(NULL, device_num, &display_device, flags);
        device_num++;
        if(devices_result)
        {
            BOOL settings_result;
            DEVMODEA dev_mode;
            dev_mode.dmSize = sizeof(DEVMODEA);
            dev_mode.dmDriverExtra = 0;

            settings_result = EnumDisplaySettingsA(display_device.DeviceName, ENUM_CURRENT_SETTINGS, &dev_mode);
            if(settings_result)
            {
                int freq = dev_mode.dmDisplayFrequency;
                if(freq > result)
                {
                    result = freq;
                }
            }
        }
    } while(devices_result);

    return result;
}

float Platform::Window::aspect_ratio()
{
    return (float)instance->window.width / instance->window.height; 
}

v2 Platform::Window::mouse_screen_position()
{
    POINT window_client_pos;
    BOOL success = GetCursorPos(&window_client_pos);
    success = ScreenToClient(instance->window.handle, &window_client_pos);

    return v2(window_client_pos.x, window_client_pos.y);
}



void *Platform::Memory::allocate(int bytes)
{
    return ::malloc(bytes);
}

void Platform::Memory::free(void *data)
{
    ::free(data);
}

void Platform::Memory::memset(void *buffer, int value, int bytes)
{
    ::memset(buffer, value, bytes);
}

void Platform::Memory::memcpy(void *dest, void *src, int bytes)
{
    ::memcpy(dest, src, bytes);
}




struct Platform::File
{
    FILE *file;
};
Platform::File *Platform::FileSystem::open(const char *path, FileMode mode)
{
    Platform::File *file = (Platform::File *)Platform::Memory::allocate(sizeof(File));
    char mode_string[8] = {};
    switch(mode)
    {
        case READ:
            strcpy(mode_string, "rb");
            break;
        case WRITE:
            strcpy(mode_string, "wb");
            break;
        case READ_WRITE:
            strcpy(mode_string, "rwb");
            break;
    }

    file->file = fopen(path, mode_string);

    if(file->file == NULL)
    {
        Platform::log_error("Couldn't open file %s", path);
    }

    return file;
}

void Platform::FileSystem::close(Platform::File *file)
{
    fclose(file->file);
    Platform::Memory::free(file);
}

void Platform::FileSystem::read(File *file, void *buffer, int bytes)
{
    fread(buffer, bytes, 1, file->file);
}

void Platform::FileSystem::write(File *file, void *buffer, int bytes)
{
    fwrite(buffer, bytes, 1, file->file);
}

int Platform::FileSystem::size(File *file)
{
    fseek(file->file, 0, SEEK_END);
    int size = ftell(file->file);
    fseek(file->file, 0, SEEK_SET);
    return size;
}

char *Platform::FileSystem::read_file_into_string(const char *path)
{
    FILE *file = fopen(path, "rb");
    if(file == nullptr) return nullptr;

    fseek(file, 0L, SEEK_END);
    int size = ftell(file);
    fseek(file, 0L, SEEK_SET);

    char *buffer = (char *)Platform::Memory::allocate(size + 1);

    fread(buffer, size, 1, file);

    buffer[size] = '\0';

    return buffer;
}

void Platform::log_info_fn(const char *file, int line, const char *format, ...)
{
    fprintf(instance->log, "-- INFO : ");
    fprintf(instance->log, "%s : %d\n", file, line);

    va_list args;
    va_start(args, format);
    vfprintf(instance->log, format, args);
    va_end(args);

    fprintf(instance->log, "\n");

    fflush(instance->log);
}

void Platform::log_warning_fn(const char *file, int line, const char *format, ...)
{
    fprintf(instance->log, "-- WARNING : ");
    fprintf(instance->log, "%s : %d\n", file, line);

    va_list args;
    va_start(args, format);
    vfprintf(instance->log, format, args);
    va_end(args);

    fprintf(instance->log, "\n");

    fflush(instance->log);
}

void Platform::log_error_fn(const char *file, int line, const char *format, ...)
{
    fprintf(instance->log, "-- ERROR : ");
    fprintf(instance->log, "%s : %d\n", file, line);

    va_list args;
    va_start(args, format);
    vfprintf(instance->log, format, args);
    va_end(args);

    fprintf(instance->log, "\n");

    fflush(instance->log);
}


