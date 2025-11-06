#include "standaloneviewmirror.hpp"

ui::StandaloneViewMirror* standaloneViewMirror = nullptr;
ui::Context* context = nullptr;
const standalone_application_api_t* _api;

// event 1 == frame sync. called each 1/60th of second, so 6 = 100ms
extern "C" void on_event(const uint32_t& events) {
    if (events & 1) {
        if (standaloneViewMirror)
            standaloneViewMirror->on_framesync();
    }
}

extern "C" void shutdown() {
    delete standaloneViewMirror;
    delete context;
}

extern "C" void PaintViewMirror() {
    ui::Painter painter;
    if (standaloneViewMirror)
        painter.paint_widget_tree(standaloneViewMirror);
}

ui::Widget* touch_widget(ui::Widget* const w, ui::TouchEvent event) {
    if (!w->hidden()) {
        // To achieve reverse depth ordering (last object drawn is
        // considered "top"), descend first.
        for (const auto child : w->children()) {
            const auto touched_widget = touch_widget(child, event);
            if (touched_widget) {
                return touched_widget;
            }
        }

        const auto r = w->screen_rect();
        if (r.contains(event.point)) {
            if (w->on_touch(event)) {
                // This widget responded. Return it up the call stack.
                return w;
            }
        }
    }
    return nullptr;
}

ui::Widget* captured_widget{nullptr};

extern "C" bool OnTouchEvent(int x, int y, uint32_t type) {
    if (standaloneViewMirror) {
        ui::TouchEvent event{{x, y}, static_cast<ui::TouchEvent::Type>(type)};

        if (event.type == ui::TouchEvent::Type::Start) {
            captured_widget = touch_widget(standaloneViewMirror, event);

            if (captured_widget) {
                captured_widget->focus();
                captured_widget->set_dirty();
            }
        }

        if (captured_widget) return captured_widget->on_touch(event);
    }
    return false;
}

extern "C" void OnFocus() {
    if (standaloneViewMirror)
        standaloneViewMirror->focus();
}

extern "C" bool OnKeyEvent(uint8_t key_val) {
    ui::KeyEvent key = (ui::KeyEvent)key_val;
    if (context) {
        auto focus_widget = context->focus_manager().focus_widget();

        if (focus_widget) {
            if (focus_widget->on_key(key))
                return true;

            context->focus_manager().update(standaloneViewMirror, key);

            if (focus_widget != context->focus_manager().focus_widget())
                return true;
            else {
                if (key == ui::KeyEvent::Up || key == ui::KeyEvent::Back || key == ui::KeyEvent::Left) {
                    focus_widget->blur();
                    return false;
                }
            }
        }
    }
    return false;
}

extern "C" bool OnEncoder(int32_t delta) {
    if (context) {
        auto focus_widget = context->focus_manager().focus_widget();

        if (focus_widget)
            return focus_widget->on_encoder((ui::EncoderEvent)delta);
    }

    return false;
}

extern "C" bool OnKeyboad(uint8_t key) {
    if (context) {
        auto focus_widget = context->focus_manager().focus_widget();
        if (focus_widget)
            return focus_widget->on_keyboard((ui::KeyboardEvent)key);
    }

    return false;
}

/* Implementing abort() eliminates requirement for _getpid(), _kill(), _exit(). */
extern "C" void abort() {
    while (true);
}

// replace memory allocations to use heap from chibios
extern "C" void* malloc(size_t size) {
    return _api->malloc(size);
}
extern "C" void* calloc(size_t num, size_t size) {
    return _api->calloc(num, size);
}
extern "C" void* realloc(void* p, size_t size) {
    return _api->realloc(p, size);
}
extern "C" void free(void* p) {
    _api->free(p);
}

// redirect std lib memory allocations (sprintf, etc.)
extern "C" void* __wrap__malloc_r(size_t size) {
    return _api->malloc(size);
}
extern "C" void __wrap__free_r(void* p) {
    _api->free(p);
}

// redirect file I/O

extern "C" FRESULT f_open(FIL* fp, const TCHAR* path, BYTE mode) {
    return _api->f_open(fp, path, mode);
}
extern "C" FRESULT f_close(FIL* fp) {
    return _api->f_close(fp);
}
extern "C" FRESULT f_read(FIL* fp, void* buff, UINT btr, UINT* br) {
    return _api->f_read(fp, buff, btr, br);
}
extern "C" FRESULT f_write(FIL* fp, const void* buff, UINT btw, UINT* bw) {
    return _api->f_write(fp, buff, btw, bw);
}
extern "C" FRESULT f_lseek(FIL* fp, FSIZE_t ofs) {
    return _api->f_lseek(fp, ofs);
}
extern "C" FRESULT f_truncate(FIL* fp) {
    return _api->f_truncate(fp);
}
extern "C" FRESULT f_sync(FIL* fp) {
    return _api->f_sync(fp);
}
extern "C" FRESULT f_opendir(DIR* dp, const TCHAR* path) {
    return _api->f_opendir(dp, path);
}
extern "C" FRESULT f_closedir(DIR* dp) {
    return _api->f_closedir(dp);
}
extern "C" FRESULT f_readdir(DIR* dp, FILINFO* fno) {
    return _api->f_readdir(dp, fno);
}
extern "C" FRESULT f_findfirst(DIR* dp, FILINFO* fno, const TCHAR* path, const TCHAR* pattern) {
    return _api->f_findfirst(dp, fno, path, pattern);
}
extern "C" FRESULT f_findnext(DIR* dp, FILINFO* fno) {
    return _api->f_findnext(dp, fno);
}
extern "C" FRESULT f_mkdir(const TCHAR* path) {
    return _api->f_mkdir(path);
}
extern "C" FRESULT f_unlink(const TCHAR* path) {
    return _api->f_unlink(path);
}
extern "C" FRESULT f_rename(const TCHAR* path_old, const TCHAR* path_new) {
    return _api->f_rename(path_old, path_new);
}
extern "C" FRESULT f_stat(const TCHAR* path, FILINFO* fno) {
    return _api->f_stat(path, fno);
}
extern "C" FRESULT f_utime(const TCHAR* path, const FILINFO* fno) {
    return _api->f_utime(path, fno);
}
extern "C" FRESULT f_getfree(const TCHAR* path, DWORD* nclst, FATFS** fatfs) {
    return _api->f_getfree(path, nclst, fatfs);
}
extern "C" FRESULT f_mount(FATFS* fs, const TCHAR* path, BYTE opt) {
    return _api->f_mount(fs, path, opt);
}
extern "C" int f_putc(TCHAR c, FIL* fp) {
    return _api->f_putc(c, fp);
}
extern "C" int f_puts(const TCHAR* str, FIL* cp) {
    return _api->f_puts(str, cp);
}
extern "C" int f_printf(FIL* fp, const TCHAR* str, ...) {
    return _api->f_printf(fp, str);
}
extern "C" TCHAR* f_gets(TCHAR* buff, int len, FIL* fp) {
    return _api->f_gets(buff, len, fp);
}

void exit_app() {
    _api->exit_app();
}
