/*
 * Copyright (c) 2014, Astron Contributors. All rights reserved.
 * Copyright (c) 2023, Max Rodriguez. All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license. You should have received a copy of this license along
 * with this source code in a file named "COPYING".
 *
 * @file Datagram.hxx
 * @author Astron Contributors
 * @date 2023-04-30
 */

#ifndef ASTRON_LIBWASM_DATAGRAM_H
#define ASTRON_LIBWASM_DATAGRAM_H

#include <set>
#include <string>
#include <vector>
#include <string.h> // memcpy
#include "../util/types.hxx"

#define DG_SIZE_TAG_BYTES 2
typedef uint16_t dgsize_t;

namespace astron { // open namespace

    class Datagram {
    private:
        uint8_t *buf;
        dgsize_t buf_cap;
        dgsize_t buf_end;

        void check_add_length(dgsize_t len) {
            if (buf_end + len > buf_cap) {
                uint8_t *tmp_buf = new uint8_t[buf_cap + len + 64];
                memcpy(tmp_buf, buf, buf_cap);
                delete[] buf;
                buf = tmp_buf;
                buf_cap = buf_cap + len + 64;
            }
        }

    public:
        Datagram() : buf(new uint8_t[64]), buf_cap(64), buf_end(0) {
        }

        Datagram(const Datagram &dg) : buf(new uint8_t[dg.size()]),
                                       buf_cap(dg.size()), buf_end(dg.size()) {
            memcpy(buf, dg.buf, dg.size());
        }

        Datagram(const std::vector <uint8_t> &data) : buf(new uint8_t[data.size()]),
                                                      buf_cap(data.size()), buf_end(data.size()) {
            memcpy(buf, &data[0], data.size());
        }

        Datagram(const std::string &data) : buf(new uint8_t[data.length()]),
                                            buf_cap(data.length()), buf_end(data.length()) {
            memcpy(buf, data.c_str(), data.length());
        }

        Datagram(const uint8_t *data, dgsize_t length) : buf(new uint8_t[length]), buf_cap(length), buf_end(length) {
            memcpy(buf, data, length);
        }

        Datagram(uint64_t to_channel, uint64_t from_channel, uint16_t message_type) :
                buf(new uint8_t[64]), buf_cap(64), buf_end(0) {
            add_server_header(to_channel, from_channel, message_type);
        }

        Datagram(const std::set <uint64_t> &to_channels, uint64_t from_channel,
                 uint16_t message_type) : buf(new uint8_t[64]), buf_cap(64), buf_end(0) {
            add_server_header(to_channels, from_channel, message_type);
        }

        Datagram(uint16_t message_type) : buf(new uint8_t[64]), buf_cap(64), buf_end(0) {
            add_control_header(message_type);
        }

        ~Datagram() {
            delete[] buf;
        }

        void add_bool(const bool &v) {
            if (v) add_uint8(1);
            else add_uint8(0);
        }

        void add_uint8(const uint8_t &v) {
            check_add_length(1);
            memcpy(buf + buf_end, &v, 1);
            buf_end += 1;
        }

        void add_uint16(const uint16_t &v) {
            check_add_length(2);
            memcpy(buf + buf_end, &v, 2);
            buf_end += 2;
        }

        void add_uint32(const uint32_t &v) {
            check_add_length(4);
            memcpy(buf + buf_end, &v, 4);
            buf_end += 4;
        }

        void add_uint64(const uint64_t &v) {
            check_add_length(8);
            memcpy(buf + buf_end, &v, 8);
            buf_end += 8;
        }

        void add_channel(const channel_t &v) {
            check_add_length(CHANNEL_SIZE_BYTES);
            memcpy(buf + buf_end, &v, CHANNEL_SIZE_BYTES);
            buf_end += CHANNEL_SIZE_BYTES;
        }

        void add_doid(const doid_t &v) {
            check_add_length(DOID_SIZE_BYTES);
            memcpy(buf + buf_end, &v, DOID_SIZE_BYTES);
            buf_end += DOID_SIZE_BYTES;
        }

        void add_zone(const zone_t &v) {
            check_add_length(ZONE_SIZE_BYTES);
            memcpy(buf + buf_end, &v, ZONE_SIZE_BYTES);
            buf_end += ZONE_SIZE_BYTES;
        }

        void add_location(const doid_t &parent, const zone_t &zone) {
            check_add_length(DOID_SIZE_BYTES + ZONE_SIZE_BYTES);
            memcpy(buf + buf_end, &parent, DOID_SIZE_BYTES);
            buf_end += DOID_SIZE_BYTES;
            memcpy(buf + buf_end, &zone, ZONE_SIZE_BYTES);
            buf_end += ZONE_SIZE_BYTES;
        }

        void add_data(const std::vector <uint8_t> &data) {
            if (data.size()) {
                check_add_length(data.size());
                memcpy(buf + buf_end, &data[0], data.size());
                buf_end += data.size();
            }
        }

        void add_data(const std::string &str) {
            check_add_length(str.length());
            memcpy(buf + buf_end, str.c_str(), str.length());
            buf_end += str.length();
        }

        void add_datagram(const Datagram &dg) {
            check_add_length(dg.buf_end);
            memcpy(buf + buf_end, dg.buf, dg.buf_end);
            buf_end += dg.buf_end;
        }

        void add_string(const std::string &str) {
            add_uint16(str.length());
            check_add_length(str.length());
            memcpy(buf + buf_end, str.c_str(), str.length());
            buf_end += str.length();
        }

        void add_string(const char *c_str) {
            std::string str(c_str); // still using std::string to easily get length
            add_uint16(str.length());
            check_add_length(str.length());
            memcpy(buf + buf_end, str.c_str(), str.length());
            buf_end += str.length();
        }

        void add_blob(const std::vector <uint8_t> &blob) {
            add_uint16(blob.size());
            check_add_length(blob.size());
            memcpy(buf + buf_end, &blob[0], blob.size());
            buf_end += blob.size();
        }

        void add_blob(const Datagram &dg) {
            add_size(dg.buf_end);
            check_add_length(dg.buf_end);
            memcpy(buf + buf_end, dg.buf, dg.buf_end);
            this->buf_end += dg.buf_end;
        }

        // add_buffer reserves a buffer of size "length" at the end of the datagram
        // and returns a pointer to the buffer, so it can be filled manually.
        uint8_t *add_buffer(dgsize_t length) {
            check_add_length(length);
            uint8_t *buf_start = buf + buf_end;
            buf_end += length;
            return buf_start;
        }

        // add_size adds a datagram or field length-tag to the datagram.
        // Note: this method should always be used instead of add_uint16 when adding a length tag
        //       to allow for future support of larger or small length limits.
        void add_size(const dgsize_t &v) {
            check_add_length(sizeof(dgsize_t));
            memcpy(buf + buf_end, &v, sizeof(dgsize_t));
            buf_end += sizeof(dgsize_t);
        }

        void add_server_header(channel_t to, channel_t from, uint16_t message_type) {
            add_uint8(1);
            add_channel(to);
            add_channel(from);
            add_uint16(message_type);
        }

        void add_server_header(const std::set <channel_t> &to, channel_t from, uint16_t message_type) {
            add_uint8(to.size());
            for (auto it = to.begin(); it != to.end(); ++it) {
                add_channel(*it);
            }
            add_channel(from);
            add_uint16(message_type);
        }

        void add_control_header(uint16_t message_type) {
            add_uint8(1);
            add_channel(CONTROL_MESSAGE);
            add_uint16(message_type);
        }

        dgsize_t size() const {
            return buf_end;
        }

        uint8_t *get_data() const {
            return buf;
        }
    };
} // close namespace

#endif //ASTRON_LIBWASM_DATAGRAM_H