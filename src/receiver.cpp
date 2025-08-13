#include "receiver.hpp"

#include <stdlib.h>
#include <stdint.h>
#include <err.h>
#include "e131.h"
#include <unistd.h>
#include <string.h> // For strerror
#include <errno.h>  // For errno
#include <arpa/inet.h> // For inet_ntop

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>

using namespace godot;

#include <godot_cpp/classes/engine.hpp> // Include for Engine::get_singleton()->is_editor_hint()
#include <godot_cpp/classes/os.hpp> // Include for OS::get_singleton()->get_process_id()
#include <godot_cpp/classes/display_server.hpp> // Include for DisplayServer::get_singleton()->window_set_mode()

void SacnReceiver::_bind_methods() {
    ClassDB::bind_method(D_METHOD("activate_universe", "universe_id"), &SacnReceiver::activate_universe);

    ClassDB::bind_method(D_METHOD("set_preview", "enable"), &SacnReceiver::set_preview);
    ClassDB::bind_method(D_METHOD("is_preview"), &SacnReceiver::is_preview);
    ClassDB::add_property("SacnReceiver", PropertyInfo(Variant::BOOL, "preview"), "set_preview", "is_preview");

    ClassDB::add_signal(get_class_static(), MethodInfo("data_received", PropertyInfo(Variant::INT, "universe_id"), PropertyInfo(Variant::PACKED_BYTE_ARRAY, "data")));

    // _process, _notification, and _exit_tree are virtual methods and are automatically bound by Godot.
    // No need to explicitly bind them here.
    ClassDB::bind_method(D_METHOD("_notification", "what"), &SacnReceiver::_notification);
}

SacnReceiver::SacnReceiver() : running(false) {
    sockfd = -1;
}

SacnReceiver::~SacnReceiver() {
    _exit(); // Ensure cleanup on destruction
}

void SacnReceiver::activate_universe(uint16_t universe_id) {
    if (sockfd < 0) {
        UtilityFunctions::print("SacNReceiver: Socket not initialized. Cannot activate universe ", universe_id);
        return;
    }

    if(active_universes.find(universe_id) != active_universes.end()) {
        return;
    }
    if (e131_multicast_join_iface(sockfd, universe_id, 0) < 0) {
        UtilityFunctions::printerr("SacNReceiver: e131_multicast_join_iface failed for universe ", universe_id, ": ", strerror(errno));
    } else {
        UtilityFunctions::print("SacNReceiver: Joined multicast group for universe ", universe_id);
        active_universes.insert(universe_id);
    }
}

void SacnReceiver::set_preview(bool p_enable) {
    if (preview == p_enable) {
        return; // No change, no need to restart
    }

    preview = p_enable;

    // Restart the node to apply preview setting
    if(inited) {
        _exit();
        _ready();
    }
}

bool SacnReceiver::is_preview() const {
    return preview;
}

void SacnReceiver::_ready() {
    set_process(true);
    bool is_editor = Engine::get_singleton()->is_editor_hint();

    if ((is_editor && preview) || (!is_editor && !preview)) {
        UtilityFunctions::print("SacNReceiver: Initializing E1.31 receiver...");
        if ((sockfd = e131_socket()) < 0) {
            UtilityFunctions::printerr("SacNReceiver: e131_socket failed: ", strerror(errno));
            return;
        }

        if (e131_bind(sockfd, E131_DEFAULT_PORT) < 0) {
            UtilityFunctions::printerr("SacNReceiver: e131_bind failed: ", strerror(errno));
            close(sockfd);
            sockfd = -1;
            return;
        }

        // Start the receiver thread
        running = true;
        receiver_thread = std::thread(&SacnReceiver::_receiver_thread_func, this);
        UtilityFunctions::print("SacNReceiver: Receiver thread started.");
    } else {
        UtilityFunctions::print("SacNReceiver: Not initializing receiver based on preview/editor settings.");
    }
    inited = true;
}

void SacnReceiver::_exit_tree() {
    // This method is called when the node is removed from the scene tree.
    // Ensure cleanup happens here to prevent issues on scene reload.
    _exit();
}

void SacnReceiver::_exit() {
    if (running.load()) {
        running = false;
        if (receiver_thread.joinable()) {
            receiver_thread.join();
        }
        UtilityFunctions::print("SacNReceiver: Receiver thread stopped.");
    }

    if (sockfd >= 0) {
        // No need to explicitly leave multicast groups, closing the socket handles it.
        close(sockfd);
        sockfd = -1;
        UtilityFunctions::print("SacNReceiver: Socket closed.");
    }
}

void SacnReceiver::_process(double delta) {
    std::unique_lock<std::mutex> lock(mtx);
    for (auto const& [universe_id, data] : received_data) {
        emit_signal("data_received", universe_id, data);
    }
    received_data.clear();
}

void SacnReceiver::_notification(int p_what) {
    if (p_what == NOTIFICATION_WM_CLOSE_REQUEST) {
        // This notification is sent when the editor is closing a scene or the project
        // or when the game window is closing.
        // Ensure cleanup happens.
        _exit();
    }
}

void SacnReceiver::_receiver_thread_func() {
    e131_packet_t packet;
    e131_error_t error;
    e131_addr_t sender_addr;
    socklen_t sender_addr_len = sizeof(sender_addr);
    char sender_ip[INET_ADDRSTRLEN];

    while (running.load()) {
        // Use a timeout to allow the thread to check the 'running' flag periodically
        // and avoid blocking indefinitely on e131_recv.
        // A small timeout (e.g., 100ms) is usually sufficient.
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 100000; // 100 ms

        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(sockfd, &read_fds);

        int select_ret = select(sockfd + 1, &read_fds, NULL, NULL, &tv);

        if (select_ret < 0) {
            if (errno == EINTR) {
                // Interrupted system call, continue loop
                continue;
            }
            UtilityFunctions::printerr("SacNReceiver: select failed: ", strerror(errno));
            running = false; // Stop the thread on error
            break;
        }

        if (select_ret == 0) {
            // Timeout, no data received, check running flag again
            continue;
        }

        ssize_t bytes_received = recvfrom(sockfd, (void *)packet.raw, sizeof(packet.raw), 0, (struct sockaddr *)&sender_addr, &sender_addr_len);
        if (bytes_received < 0) {
            if (errno == EINTR) {
                // Interrupted system call, continue loop
                continue;
            }
            UtilityFunctions::printerr("SacNReceiver: recvfrom failed: ", strerror(errno));
            running = false; // Stop the thread on error
            break;
        }

        // Print sender IP and port for debugging
        inet_ntop(AF_INET, &(sender_addr.sin_addr), sender_ip, INET_ADDRSTRLEN);
        // UtilityFunctions::print("SacNReceiver: Received packet from ", String(sender_ip), ":", ntohs(sender_addr.sin_port));
        // UtilityFunctions::print("SacNReceiver: Universe from packet (host byte order): ", ntohs(packet.frame.universe)); // Print universe in host byte order for debugging

        if ((error = e131_pkt_validate(&packet)) != E131_ERR_NONE) {
            // UtilityFunctions::printerr("SacNReceiver: e131_pkt_validate: ", e131_strerror(error), ". Raw universe was: ", ntohs(packet.frame.universe));
            continue;
        }

        uint16_t universe_id = ntohs(packet.frame.universe); // This conversion is already correct here

        // Check if this universe is one we are actively listening for
        if (active_universes.find(universe_id) == active_universes.end()) {
            // Not an active universe, discard packet
            UtilityFunctions::print("SacNReceiver: Received packet for non-active universe ", universe_id, ". Discarding.");
            continue;
        }

        // Discard out-of-order packets
        // last_seq map needs to be protected by mutex if accessed from multiple threads
        std::unique_lock<std::mutex> lock(mtx);
        uint8_t current_last_seq = last_seq[universe_id];
        if (e131_pkt_discard(&packet, current_last_seq)) {
            UtilityFunctions::print("SacNReceiver: warning: packet out of order received for universe ", universe_id);
            last_seq[universe_id] = packet.frame.seq_number;
            continue;
        }
        last_seq[universe_id] = packet.frame.seq_number;

        // Extract DMX data
        PackedByteArray dmx_data;
        dmx_data.resize(ntohs(packet.dmp.prop_val_cnt) - 1); // -1 because the first byte is the start code (0x00)
        for (size_t i = 0; i < dmx_data.size(); ++i) {
            dmx_data.set(i, packet.dmp.prop_val[i + 1]); // Skip start code
        }

        // Store received data
        received_data[universe_id] = dmx_data;
    }
}
