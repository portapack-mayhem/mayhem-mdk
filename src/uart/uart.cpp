/*
 * Copyright (C) 2024 Bernd Herzog
 *
 * This file is part of PortaPack.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

#include "uart.hpp"

#include <memory>
#include <string>

StandaloneViewMirror *standaloneViewMirror = nullptr;
ui::Context *context = nullptr;

extern "C" void initialize(const standalone_application_api_t &api)
{
    _api = &api;

    context = new ui::Context();
    standaloneViewMirror = new StandaloneViewMirror(*context, {0, 16, 240, 304});
}

#define USER_COMMANDS_START 0x7F01

enum class Command : uint16_t
{
    // UART specific commands
    COMMAND_UART_REQUESTDATA_SHORT = USER_COMMANDS_START,
    COMMAND_UART_REQUESTDATA_LONG,
    COMMAND_UART_BAUDRATEDETECTION_ENABLE,
    COMMAND_UART_BAUDRATEDETECTION_DISABLE,
    COMMAND_UART_BAUDRATEDETECTION_READ,
};

extern "C" void on_event(const uint32_t &events)
{
    (void)events;

    Command cmd = Command::COMMAND_UART_REQUESTDATA_SHORT;
    std::vector<uint8_t> data(5);

    uint8_t more_data_available;
    do
    {
        if (_api->i2c_read((uint8_t *)&cmd, 2, data.data(), data.size()) == false)
            return;

        uint8_t data_len = data[0] & 0x7f;
        more_data_available = data[0] >> 7;
        uint8_t *data_ptr = data.data() + 1;

        if (data_len > 0)
        {
            standaloneViewMirror->get_console().write(std::string((char *)data_ptr, data_len));
        }

        if (more_data_available)
        {
            cmd = Command::COMMAND_UART_REQUESTDATA_LONG;
            data = std::vector<uint8_t>(128);
        }
    } while (more_data_available == 1);
}

extern "C" void shutdown()
{
    delete standaloneViewMirror;
    delete context;
}

extern "C" void PaintViewMirror()
{
    ui::Painter painter;
    if (standaloneViewMirror)
        painter.paint_widget_tree(standaloneViewMirror);
}

ui::Widget *touch_widget(ui::Widget *const w, ui::TouchEvent event)
{
    if (!w->hidden())
    {
        // To achieve reverse depth ordering (last object drawn is
        // considered "top"), descend first.
        for (const auto child : w->children())
        {
            const auto touched_widget = touch_widget(child, event);
            if (touched_widget)
            {
                return touched_widget;
            }
        }

        const auto r = w->screen_rect();
        if (r.contains(event.point))
        {
            if (w->on_touch(event))
            {
                // This widget responded. Return it up the call stack.
                return w;
            }
        }
    }
    return nullptr;
}

ui::Widget *captured_widget{nullptr};

extern "C" void OnTouchEvent(int x, int y, uint32_t type)
{
    if (standaloneViewMirror)
    {
        ui::TouchEvent event{{x, y}, static_cast<ui::TouchEvent::Type>(type)};

        if (event.type == ui::TouchEvent::Type::Start)
        {
            captured_widget = touch_widget(standaloneViewMirror, event);

            if (captured_widget)
            {
                captured_widget->focus();
                captured_widget->set_dirty();
            }
        }

        if (captured_widget)
            captured_widget->on_touch(event);
    }
}

extern "C" void OnFocus()
{
    if (captured_widget)
    {
        captured_widget->focus();
    }
}