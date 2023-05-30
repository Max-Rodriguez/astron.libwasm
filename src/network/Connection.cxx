/*
 * Copyright (c) 2023, Max Rodriguez. All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license. You should have received a copy of this license along
 * with this source code in a file named "COPYING".
 *
 * @file Connection.cxx
 * @author Max Rodriguez
 * @date 2023-05-18
 */

#ifndef __EMSCRIPTEN__
#error Currently astron.libwasm only targets WebAssembly. Please build with Emscripten.
#endif // __EMSCRIPTEN__

#include <emscripten/websocket.h>
#include "Connection.hxx"

namespace astron { // open namespace

    Connection::Connection() : m_log("connection", "Connection") // constructor; creates websocket
    {
        // check websocket support on this browser
        EM_BOOL ws_support = emscripten_websocket_is_supported();

        if (!ws_support) {
            logger().error() << "WebSocket is not supported in your browser. Please upgrade your browser!";
            g_logger->js_flush();
            emscripten_force_exit(1); // exit w/ code 1 (error)
        }
    }

    Connection::~Connection() // destructor
    {
        if (m_socket) {
            disconnect(1000, "Connection instance destructor called with open web socket.");
        }
    }

    void Connection::connect_socket(std::string url)
    {
        logger().info() << "Initializing WebSocket connection.";
        g_logger->js_flush();

        // create a new emscripten websocket
        EmscriptenWebSocketCreateAttributes ws_attributes;
        url.insert(0, "wss://"); // must have 'wss://' prefix
        ws_attributes.url = url.c_str();
        ws_attributes.protocols = "binary";
        ws_attributes.createOnMainThread = 1;
        m_socket = emscripten_websocket_new(&ws_attributes); // returns EMSCRIPTEN_WEBSOCKET_T
    }

    EMSCRIPTEN_RESULT Connection::disconnect(unsigned short code, const char *reason) {
        if (!m_socket) {
            logger().warning() << "Connection::disconnect() called, but m_socket is 0 (no socket).";
            g_logger->js_flush();
            return EMSCRIPTEN_RESULT_SUCCESS;
        }
        EMSCRIPTEN_RESULT res = emscripten_websocket_close(m_socket, code, reason);
        if (res != EMSCRIPTEN_RESULT_SUCCESS) {
            return res; // EMSCRIPTEN_RESULT_SUCCESS == 0
        }
        res = emscripten_websocket_delete(m_socket); // free socket handle from memory
        m_socket = 0; // reset m_socket value
        return res;
    }
} // close namespace astron