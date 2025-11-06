/*
 * Copyright (C) 2024 HTotoo
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

#include "flipper_irfile.hpp"

#include "file_reader.hpp"
#include "string_format.hpp"
#include <string_view>

namespace fs = std::filesystem;
using namespace std::literals;

/*

https://developer.flipper.net/flipperzero/doxygen/infrared_file_format.html

Filetype: IR signals file
Version: 1
#
name: Button_1
type: parsed
protocol: NECext
address: EE 87 00 00
command: 5D A0 00 00
#
name: Button_2
type: raw
frequency: 38000
duty_cycle: 0.330000
data: 504 3432 502 483 500 484 510 502 502 482 501 485 509 1452 504 1458 509 1452 504 481 501 474 509 3420 503
#
name: Button_3
type: parsed
protocol: SIRC
address: 01 00 00 00
command: 15 00 00 00
*/

const std::string filetype_name = "Filetype";
const std::string proto_type_name = "type";
const std::string proto_proto_name = "protocol";
const std::string proto_address_name = "address";
const std::string proto_command_name = "command";

ir_data_t read_flipper_ir_file(File& f) {
    char ch = 0;
    std::string line = "";
    auto fr = f.read(&ch, 1);
    ir_data_t ir_data;
    ir_data.repeat = 2;
    ir_data.protocol = irproto::UNK;
    line.resize(130);
    bool is_type_ok = false;  // if type is ok
    while (!fr.is_error() && fr.value() > 0) {
        if (line.length() < 130 && ch != '\n') line += ch;
        if (ch != '\n') {
            fr = f.read(&ch, 1);
            continue;
        }
        if (line[0] == '#') {
            // separator
            if (ir_data.protocol != irproto::UNK && ir_data.data != 0) {
                // We have valid data parsed. return it
                return ir_data;
            }
            line = "";
            fr = f.read(&ch, 1);
            continue;
        }
        auto it = line.find(':', 0);
        if (it == std::string::npos) {
            fr = f.read(&ch, 1);
            continue;  // Bad line.
        }
        std::string fixed = line.data() + it + 1;
        fixed = trim(fixed);
        std::string head = line.substr(0, it);
        line = "";

        if (fixed.length() <= 1) {
            fr = f.read(&ch, 1);
            continue;
        }

        if (head == filetype_name) {
            if (fixed != "IR signals file") return {irproto::UNK, 1, 0};  // not supported
        } else if (head == proto_type_name) {
            is_type_ok = (fixed == "parsed");
        } else if (head == proto_proto_name) {
            if (fixed == "NEC")
                ir_data.protocol = irproto::NEC;
            else if (fixed == "NECext")
                ir_data.protocol = irproto::NECEXT;
            else if (fixed == "SIRC")
                ir_data.protocol = irproto::SONY;
            else if (fixed == "Samsung32")
                ir_data.protocol = irproto::SAM;
            else if (fixed == "RC5")
                ir_data.protocol = irproto::RC5;
        } else if (head == proto_address_name) {
            uint32_t pd = 0;
            // Parse hex bytes: "EE 87 00 00"
            int pos = 32;
            for (size_t i = 0; i < fixed.length() && i < 11;) {
                if (fixed[i] == ' ') {
                    i++;
                    continue;
                }
                pos -= 4;
                uint8_t byte = 0;
                char c = fixed[i];
                if (c >= '0' && c <= '9')
                    byte = (c - '0');
                else if (c >= 'A' && c <= 'F')
                    byte = (c - 'A' + 10);
                else if (c >= 'a' && c <= 'f')
                    byte = (c - 'a' + 10);
                pd |= (byte << pos);
            }
            uint64_t tmp = pd;
            ir_data.data |= tmp << 32;  // address is upper 32 bits
        } else if (head == proto_command_name) {
            uint32_t pd = 0;
            // Parse hex bytes: "EE 87 00 00"
            int pos = 32;
            for (size_t i = 0; i < fixed.length() && i < 11;) {
                if (fixed[i] == ' ') {
                    i++;
                    continue;
                }
                pos -= 4;
                uint8_t byte = 0;
                char c = fixed[i];
                if (c >= '0' && c <= '9')
                    byte = (c - '0');
                else if (c >= 'A' && c <= 'F')
                    byte = (c - 'A' + 10);
                else if (c >= 'a' && c <= 'f')
                    byte = (c - 'a' + 10);
                pd |= (byte << pos);
            }
            ir_data.data |= pd;  // command is lower 32 bits
        }
        fr = f.read(&ch, 1);
    }
    if (ir_data.protocol != irproto::UNK && ir_data.data != 0 && is_type_ok) {
        // We have valid data parsed. return it
        return ir_data;
    }
    return {irproto::UNK, 2, 0};
};
