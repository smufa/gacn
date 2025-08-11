#include "sender.hpp"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <cstring>
#include <unistd.h>
#include <err.h>

namespace godot {

void SacnSender::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_destination_address", "address"), &SacnSender::set_destination_address);
    ClassDB::bind_method(D_METHOD("get_destination_address"), &SacnSender::get_destination_address);
    ClassDB::add_property("SacnSender", PropertyInfo(Variant::STRING, "destination_address"), "set_destination_address", "get_destination_address");

    ClassDB::bind_method(D_METHOD("set_universe", "universe_id"), &SacnSender::set_universe);
    ClassDB::bind_method(D_METHOD("get_universe"), &SacnSender::get_universe);
    ClassDB::add_property("SacnSender", PropertyInfo(Variant::INT, "universe"), "set_universe", "get_universe");

    ClassDB::bind_method(D_METHOD("set_port", "port_number"), &SacnSender::set_port);
    ClassDB::bind_method(D_METHOD("get_port"), &SacnSender::get_port);
    ClassDB::add_property("SacnSender", PropertyInfo(Variant::INT, "port"), "set_port", "get_port");

    ClassDB::bind_method(D_METHOD("set_preview", "is_preview"), &SacnSender::set_preview);
    ClassDB::bind_method(D_METHOD("get_preview"), &SacnSender::get_preview);
    ClassDB::add_property("SacnSender", PropertyInfo(Variant::BOOL, "preview"), "set_preview", "get_preview");

    ClassDB::bind_method(D_METHOD("set_use_multicast", "use_multicast"), &SacnSender::set_use_multicast);
    ClassDB::bind_method(D_METHOD("get_use_multicast"), &SacnSender::get_use_multicast);
    ClassDB::add_property("SacnSender", PropertyInfo(Variant::BOOL, "use_multicast"), "set_use_multicast", "get_use_multicast");

    ClassDB::bind_method(D_METHOD("send_data", "data"), &SacnSender::send_data);
}

SacnSender::SacnSender() : sequence_number(0), use_multicast(true) {
    // create a socket for E1.31
    if ((sockfd = e131_socket()) < 0){
        UtilityFunctions::print("e131_socket failed");
    }

    // configure socket to use the default network interface for outgoing multicast data
    if (e131_multicast_iface(sockfd, 0) < 0){
        UtilityFunctions::print("e131_multicast_iface failed");
    }
}

SacnSender::~SacnSender() {
    close(sockfd);
}

void SacnSender::set_destination_address(const String& address) {
    destination_address = address;
}

String SacnSender::get_destination_address() const {
    return destination_address;
}

void SacnSender::set_universe(const int& universe_id) {
    universe = universe_id;
}

int SacnSender::get_universe() const {
    return universe;
}

void SacnSender::set_port(const int& port_number) {
    port = port_number;
}

int SacnSender::get_port() const {
    return port;
}

void SacnSender::set_preview(const bool& is_preview) {
    preview = is_preview;
}

bool SacnSender::get_preview() const {
    return preview;
}

void SacnSender::set_use_multicast(const bool& use_multicast) {
    this->use_multicast = use_multicast;
}

bool SacnSender::get_use_multicast() const {
    return use_multicast;
}

void SacnSender::send_data(const PackedByteArray& data) {
    // Increment sequence number
    sequence_number++;

    // initialize the new E1.31 packet
    e131_pkt_init(&packet, universe, data.size());
    std::memcpy(&packet.frame.source_name, "Godot sACN Sender", 18);
    e131_set_option(&packet, E131_OPT_PREVIEW, preview);
    packet.frame.seq_number = sequence_number;

    // set remote system destination
    if (use_multicast) {
        if (e131_multicast_dest(&dest, universe, port) < 0) {
            UtilityFunctions::print("e131_multicast_dest failed");
            return;
        }
    } else {
        if (e131_unicast_dest(&dest, destination_address.utf8().get_data(), port) < 0){
            UtilityFunctions::print("e131_unicast_dest failed");
            return;
        }
    }

    // copy data to packet
    for (int i = 0; i < data.size(); ++i) {
        packet.dmp.prop_val[i + 1] = data[i];
    }

    if (e131_send(sockfd, &packet, &dest) < 0){
        UtilityFunctions::print("e131_send failed");
        return;
    }
}

}
