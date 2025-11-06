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

#pragma once

#include "standalone_application.hpp"
#include "ui/ui_widget.hpp"
#include "ui/theme.hpp"
#include "ui/string_format.hpp"
#include "ui/ui_helper.hpp"
#include <string.h>
#include "ui/ui_navigation.hpp"
#include "ui/ui_fileman.hpp"
#include "standaloneviewmirror.hpp"

#include "pp_commands.hpp"
#include "flipper_irfile.hpp"

namespace ui {

class TIRAppView : public ui::View {
   public:
    TIRAppView(ui::NavigationView& nav) : nav_(nav) {
        set_style(ui::Theme::getInstance()->bg_darker);

        add_children({&labels,
                      &button_recv,
                      &button_send,
                      &text_filename,
                      &text_irproto,
                      &text_irdata,
                      &button_browse});

        button_send.on_select = [this](ui::Button&) {
            if (send_mode_on) {  // already sending
                // stop
                send_mode_on = false;
                file.close();
                button_send.set_text("Send ir3");
                return;
            }
            recv_mode_on = false;
            if (current_file_path.empty()) {
                if (ir_to_send.protocol != irproto::UNK) {
                    send_ir_data();  // only repeat last i got
                    button_send.set_text("Send ir4");
                } else {
                    button_send.set_text("Send ir5");
                }
            } else {
                auto res = file.open("/IR/unioff.ir", true, false);
                if (!res.is_valid()) {
                    send_mode_on = true;
                    button_send.set_text("Stop");
                } else {
                    button_send.set_text("Send ir6");
                }
            }
        };

        button_recv.on_select = [this](ui::Button&) {
            if (send_mode_on) return;  // can't recv while sending
            recv_mode_on = true;
            button_recv.set_text("Waiting...");
            text_irproto.set("-");
            text_irdata.set("-");
            text_filename.set("-");
        };

        button_browse.on_select = [this, &nav](ui::Button&) {
            auto open_view = nav_.push<FileLoadView>(".ir");
            open_view->push_dir("IR");
            open_view->on_changed = [this](std::filesystem::path new_file_path) {
                text_filename.set(new_file_path.filename().string());
                current_file_path = new_file_path;
            };
        };
    }

    ~TIRAppView() {
        ui::Theme::destroy();
    }

    void send_ir_data() {
        Command cmd = Command::PPCMD_IRTX_SENDIR;
        std::vector<uint8_t> data(sizeof(ir_data_t));
        memcpy(data.data(), &ir_to_send, sizeof(ir_data_t));
        data.insert(data.begin(), reinterpret_cast<uint8_t*>(&cmd), reinterpret_cast<uint8_t*>(&cmd) + sizeof(cmd));
        if (_api->i2c_read(data.data(), data.size(), nullptr, 0) == false) return;
    }

    void send_next_ir() {
        if (!send_mode_on) return;
        if (current_file_path.empty()) {
            send_mode_on = false;
            return;
        }

        auto irdata = read_flipper_ir_file(file);
        if (irdata.protocol == irproto::UNK) {
            // done or error
            send_mode_on = false;
            file.close();
            button_send.set_text("Send ir2");
            return;
        } else {
            ir_to_send = irdata;
            update_ir_display();
            send_ir_data();
        }
    }

    void on_framesync() override {
        if (need_refresh()) {
            Command cmd = Command::PPCMD_IRTX_GETLASTRCVIR;
            std::vector<uint8_t> data(sizeof(ir_data_t));
            if (_api->i2c_read((uint8_t*)&cmd, 2, data.data(), data.size()) == false) return;
            ir_data_t irdata = *(ir_data_t*)data.data();
            got_data(irdata);
        }
        if (send_mode_on) {
            send_timer++;
            if (send_timer > 20) {  // around 150ms
                send_timer = 0;
                send_next_ir();
            }
        }
    }

    void focus() override {
        button_recv.focus();
    }

    bool need_refresh() {
        rfcnt++;
        if (!recv_mode_on) return false;
        if (rfcnt > 60) {  // 1 sec
            rfcnt = 0;
            return true;
        }
        return false;
    }

    void update_ir_display() {
        std::string proto = "UNK";
        switch (ir_to_send.protocol) {
            case irproto::NEC:
                proto = "NEC";
                break;
            case irproto::NECEXT:
                proto = "NECEXT";
                break;
            case irproto::SONY:
                proto = "SONY";
                break;
            case irproto::SAM:
                proto = "SAM";
                break;
            case irproto::RC5:
                proto = "RC5";
                break;
            default:
                break;
        }
        text_irproto.set(proto);
        text_irdata.set(to_string_hex(ir_to_send.data, 16));
    }

    void got_data(ir_data_t ir) {
        if (ir.protocol == irproto::UNK) return;
        if (!send_mode_on) ir_to_send = ir;
        ir_to_send.repeat = 2;
        text_filename.set("-");
        current_file_path = "";
        recv_mode_on = false;  // no more, got a valid
        button_recv.set_text("Read ir");
        update_ir_display();
    }

   private:
    ui::Button button_browse{{UI_POS_X(0), UI_POS_Y(1), UI_POS_WIDTH(10), UI_POS_HEIGHT(2)}, "Browse"};
    ui::Button button_send{{UI_POS_X(0), UI_POS_Y(3), UI_POS_WIDTH(10), UI_POS_HEIGHT(2)}, "Send ir"};
    ui::Button button_recv{{UI_POS_X(0), UI_POS_Y(5), UI_POS_WIDTH(10), UI_POS_HEIGHT(2)}, "Read ir"};

    ui::Labels labels{
        {{UI_POS_X(0), UI_POS_Y(0)}, "File:", ui::Theme::getInstance()->fg_light->foreground},
        {{UI_POS_X(0), UI_POS_Y(8)}, "------------------------------", ui::Theme::getInstance()->fg_light->foreground},
        {{UI_POS_X(0), UI_POS_Y(9)}, "IR data:", ui::Theme::getInstance()->fg_light->foreground},
        {{UI_POS_X(0), UI_POS_Y_BOTTOM(3)}, "Supported protocols:", ui::Theme::getInstance()->fg_light->foreground},
        {{UI_POS_X(0), UI_POS_Y_BOTTOM(2)}, "Nec, NecEXT, Sony, SAM, RC5", ui::Theme::getInstance()->fg_light->foreground},
    };

    ui::Text text_filename{{UI_POS_X(7), UI_POS_Y(0), UI_POS_WIDTH_REMAINING(7), UI_POS_HEIGHT(1)}, "-"};
    ui::Text text_irproto{{UI_POS_X(0), UI_POS_Y(10), UI_POS_MAXWIDTH, UI_POS_HEIGHT(1)}, "-"};
    ui::Text text_irdata{{UI_POS_X(0), UI_POS_Y(11), UI_POS_MAXWIDTH, UI_POS_HEIGHT(1)}, "-"};

    uint8_t rfcnt = 0;
    bool recv_mode_on = false;
    bool send_mode_on = false;
    uint8_t send_timer = 0;  // our max is around 150ms per ir, so 10 framesync
    std::filesystem::path current_file_path = "";
    ir_data_t ir_to_send{};
    File file{};

    ui::NavigationView& nav_;
};

}  // namespace ui