#include "platform.h"
#include "engine.h"
#include "input.h"
#include "memory.h"

#include <windows.h>
#include <stdio.h>
#include <stdarg.h>



struct PlatformState
{
    HINSTANCE app_instance;
    HWND window_handle;
    int window_width;
    int window_height;

    FILE *log;

    bool want_to_close;

    LARGE_INTEGER start_time_counter;
};
static HINSTANCE instance;



HWND get_window_handle(PlatformState *platform_state) { return platform_state->window_handle; }

int get_screen_width(PlatformState *platform_state)   { return platform_state->window_width;  }
int get_screen_height(PlatformState *platform_state)  { return platform_state->window_height; }
int get_monitor_frequency(PlatformState *platform_state)
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
float get_aspect_ratio(PlatformState *platform_state) { return (float)platform_state->window_width / platform_state->window_height; }
HDC get_device_context(PlatformState *platform_state) { return GetDC(platform_state->window_handle); }

bool want_to_close(PlatformState *platform_state)
{
    return platform_state->want_to_close;
}



extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    if(ImGui_ImplWin32_WndProcHandler(window, message, wParam, lParam)) return true;

    LRESULT result = 0; 

    PlatformState *platform_state = get_engine_platform_state();
    InputState *input_state = get_engine_input_state();

    switch (message)
    {
        case WM_DESTROY:
        {
            PostQuitMessage(0);
            return 0;
        }
        break;

        case WM_CLOSE: 
        {
            DestroyWindow(window);
            return 0;
        }  
        break;

        /*
        case WM_PAINT:
        {
        ValidateRect(window_handle, 0);
        }
        break;
        */
 
        case WM_KEYDOWN:
        {
            /*
            if(wParam == VK_ESCAPE)
            {
                platform_state->want_to_close = true;
            }
            */
            record_key_event(input_state, wParam, true);
        }
        break;
        case WM_KEYUP:   { record_key_event(input_state, wParam, false); break; }

        case WM_LBUTTONDOWN: { record_mouse_event(input_state, 0, true);  break; }
        case WM_LBUTTONUP:   { record_mouse_event(input_state, 0, false); break; }

        default:
        {
            result = DefWindowProc(window, message, wParam, lParam);
        }
        break;
    }

    return result;
}


PlatformState *init_platform()
{
    PlatformState *platform_state = (PlatformState *)my_allocate(sizeof(PlatformState));
    memset(platform_state, 0, sizeof(PlatformState));

    QueryPerformanceCounter(&platform_state->start_time_counter);

    platform_state->app_instance = instance;

    // Create the window class
    WNDCLASS window_class = {};

    window_class.style = CS_HREDRAW|CS_VREDRAW|CS_OWNDC;
    window_class.lpfnWndProc = WindowProc;
    window_class.hInstance = platform_state->app_instance;
    window_class.hCursor = LoadCursor(NULL, IDC_ARROW);
    window_class.lpszClassName = "Windows Program Class";

    if(!RegisterClass(&window_class)) { return nullptr; }

    BOOL result;
    RECT rect;
    result = SystemParametersInfoA(SPI_GETWORKAREA, 0, &rect, 0);
    unsigned window_width = rect.right;
    unsigned window_height = rect.bottom;
    //unsigned window_width = GetSystemMetrics(SM_CXSCREEN);
    //unsigned window_height = GetSystemMetrics(SM_CYSCREEN);


    // Create the window
    platform_state->window_handle = CreateWindowEx(0,                  // Extended style
            window_class.lpszClassName,        // Class name
            "",                           // Window name
            WS_OVERLAPPEDWINDOW | WS_VISIBLE,  // Style of the window
            0,                                 // Initial X position
            0,                                 // Initial Y position
            window_width,                     // Initial width
            window_height,                    // Initial height
            0,                                 // Handle to the window parent
            0,                                 // Handle to a menu
            platform_state->app_instance,        // Handle to an instance
            0);

    /*
    platform_state->window_handle = CreateWindowEx(0,   // Extended style
            window_class.lpszClassName,        // Class name
            "",                           // Window name
            WS_POPUP | WS_VISIBLE,             // Style of the window
            0,                                 // Initial X position
            0,                                 // Initial Y position
            window_width,                      // Initial width
            window_height,                     // Initial height 
            0,                                 // Handle to the window parent
            0,                                 // Handle to a menu
            platform_state->app_instance,       // Handle to an instance
            0);                                // Pointer to a CLIENTCTREATESTRUCT
    */

    if(!platform_state->window_handle) { return nullptr; }

    RECT client_rect;
    BOOL success = GetClientRect(platform_state->window_handle, &client_rect);

    platform_state->window_width = client_rect.right;
    platform_state->window_height = client_rect.bottom;

    result = CreateDirectory("output", NULL);
    DWORD create_dir_result = GetLastError();
    if(result || ERROR_ALREADY_EXISTS == create_dir_result)
    {
        // CopyFile(...)
        platform_state->log = fopen("output/log.txt", "w");
    }
    else
    {
        // Failed to create directory.
    }

    return platform_state;
}

void platform_events(PlatformState *platform_state)
{
    MSG message;
    while(PeekMessage(&message, 0, 0, 0, PM_REMOVE))
    {
        if(message.message == WM_QUIT)
        {
            platform_state->want_to_close = true;
        }

        TranslateMessage(&message);
        DispatchMessage(&message);
    }

    if(want_to_close(platform_state))
    {
        stop_engine();
    }
}


// In seconds
double time_since_start(PlatformState *platform_state)
{
    LARGE_INTEGER counts_per_second;
    QueryPerformanceFrequency(&counts_per_second);

    LARGE_INTEGER current_time;
    QueryPerformanceCounter(&current_time);

    LONGLONG counts_passed = current_time.QuadPart - platform_state->start_time_counter.QuadPart;

    double dt = (double)counts_passed / counts_per_second.QuadPart;

    return dt;
}

v2 mouse_screen_position(PlatformState *platform_state)
{
  POINT window_client_pos;
  BOOL success = GetCursorPos(&window_client_pos);
  success = ScreenToClient(get_window_handle(platform_state), &window_client_pos);

  return v2(window_client_pos.x, window_client_pos.y);
}

char *read_file_as_string(const char *path)
{
    FILE *file = fopen(path, "rb");
    if(file == nullptr) return nullptr;

    fseek(file, 0L, SEEK_END);
    int size = ftell(file);
    fseek(file, 0L, SEEK_SET);

    char *buffer = (char *)my_allocate(size + 1);

    fread(buffer, size, 1, file);

    buffer[size] = '\0';

    return buffer;
}

void log_info_fn(const char *file, int line, const char *format, ...)
{
    PlatformState *platform_state = get_engine_platform_state();

    fprintf(platform_state->log, "-- INFO : ");
    fprintf(platform_state->log, "%s : %d\n", file, line);

    va_list args;
    va_start(args, format);
    vfprintf(platform_state->log, format, args);
    va_end(args);

    fprintf(platform_state->log, "\n");

    fflush(platform_state->log);
}

void log_warning_fn(const char *file, int line, const char *format, ...)
{
    PlatformState *platform_state = get_engine_platform_state();

    fprintf(platform_state->log, "-- WARNING : ");
    fprintf(platform_state->log, "%s : %d\n", file, line);

    va_list args;
    va_start(args, format);
    vfprintf(platform_state->log, format, args);
    va_end(args);

    fprintf(platform_state->log, "\n");

    fflush(platform_state->log);
}

void log_error_fn(const char *file, int line, const char *format, ...)
{
    PlatformState *platform_state = get_engine_platform_state();

    fprintf(platform_state->log, "-- ERROR : ");
    fprintf(platform_state->log, "%s : %d\n", file, line);

    va_list args;
    va_start(args, format);
    vfprintf(platform_state->log, format, args);
    va_end(args);

    fprintf(platform_state->log, "\n");

    fflush(platform_state->log);
}



int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    instance = hInstance;
    start_engine();
}

