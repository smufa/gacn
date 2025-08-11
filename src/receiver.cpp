#include "receiver.hpp"

#include <stdlib.h>
#include <stdint.h>
#include <err.h>
#include <e131.h>
#include <unistd.h>

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <mutex>
#include <vector>

using namespace godot;

void SacnReceiver::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_universes", "universe_list"), &SacnReceiver::set_universes);
    ClassDB::bind_method(D_METHOD("get_universes"), &SacnReceiver::get_universes);
    ClassDB::add_property(StringName("SacnReceiver"), PropertyInfo(Variant::PACKED_INT32_ARRAY, "universes"), StringName("set_universes"), StringName("get_universes"));

    ClassDB::bind_method(D_METHOD("get_data"), &SacnReceiver::get_data);
    ClassDB::add_property(StringName("SacnReceiver"), PropertyInfo(Variant::PACKED_BYTE_ARRAY, "data", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_READ_ONLY), StringName(), StringName("get_data"));

    ADD_SIGNAL(MethodInfo("data_received", PropertyInfo(Variant::INT, "universe_id"), PropertyInfo(Variant::PACKED_BYTE_ARRAY, "data")));
}

SacnReceiver::SacnReceiver() {
    // Initialize variables here
}

SacnReceiver::~SacnReceiver() {
    // Clean up resources here
}

void SacnReceiver::_ready() {
    UtilityFunctions::print("Hello from SacnReceiver!");
    receiver_thread = std::thread(&SacnReceiver::_receive_packets, this);
}

void SacnReceiver::_receive_packets() {
    e131_packet_t packet;
    e131_error_t error;
    uint8_t last_seq = 0x00;

    while (running) {
        int sockfd;
        std::vector<int> current_universes;

        // Create and bind socket
        if ((sockfd = e131_socket()) < 0) {
            UtilityFunctions::print("e131_socket failed");
            running = false;
            return;
        }
        if (e131_bind(sockfd, E131_DEFAULT_PORT) < 0) {
            UtilityFunctions::print("e131_bind failed");
            ::close(sockfd);
            running = false;
            return;
        }

        // Join multicast groups
        {
            std::lock_guard<std::mutex> lock(universes_mutex);
            current_universes.clear();
            for (int i = 0; i < universes.size(); ++i) {
                current_universes.push_back(universes[i]);
            }
        }

        for (int universe_id : current_universes) {
            if (e131_multicast_join_iface(sockfd, universe_id, 0) < 0) {
                UtilityFunctions::print(String("e131_multicast_join_iface failed for universe ") + String::num(universe_id));
            } else {
                UtilityFunctions::print(String("Joined multicast group for universe ") + String::num(universe_id));
            }
        }

        // Receive packets loop
        UtilityFunctions::print("waiting for E1.31 packets ...");
        while (running) {
            // Check for universe changes
            {
                std::lock_guard<std::mutex> lock(universes_mutex);
                bool changed = false;
                if (universes.size() != current_universes.size()) {
                    changed = true;
                } else {
                    for (int i = 0; i < universes.size(); ++i) {
                        if (universes[i] != current_universes[i]) {
                            changed = true;
                            break;
                        }
                    }
                }

                if (changed) {
                    UtilityFunctions::print("Universe list changed, reopening socket.");
                    break; // Break inner loop to re-create socket
                }
            }

            if (e131_recv(sockfd, &packet) < 0) {
                 if (running) { // Check running flag to differentiate between error and intentional shutdown
                    UtilityFunctions::print("e131_recv failed");
                 }
                break; // Exit inner loop on recv failure
            }

            if ((error = e131_pkt_validate(&packet)) != E131_ERR_NONE) {
                UtilityFunctions::print(String("e131_pkt_validate: ") + e131_strerror(error));
                continue;
            }
            if (e131_pkt_discard(&packet, last_seq)) {
                UtilityFunctions::print("warning: packet out of order received");
                last_seq = packet.frame.seq_number;
                continue;
            }

            // Store received data in PackedByteArray
            PackedByteArray received_data;
            received_data.resize(packet.dmp.prop_val_cnt);
            for (int i = 1; i <= packet.dmp.prop_val_cnt; ++i) {
                received_data[i - 1] = packet.dmp.prop_val[i];
            }
            data = received_data;

            UtilityFunctions::print("Received packet for universe " + String::num(packet.frame.universe));
            emit_signal("data_received", packet.frame.universe, data);
            last_seq = packet.frame.seq_number;

            // Small sleep to avoid busy-waiting
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        // Close the socket before the outer loop continues
        ::close(sockfd);
    }
}

void SacnReceiver::set_universes(const PackedInt32Array& universe_list) {
    std::lock_guard<std::mutex> lock(universes_mutex);
    universes = universe_list;
}

PackedInt32Array SacnReceiver::get_universes() const {
    return universes;
}

PackedByteArray SacnReceiver::get_data() const {
	return data;
}
