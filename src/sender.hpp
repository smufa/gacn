#ifndef SENDER_HPP
#define SENDER_HPP

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/core/property_info.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <e131.h>

namespace godot {

class SacnSender : public Node {
    GDCLASS(SacnSender, Node);

private:
    int sockfd;
    e131_packet_t packet;
    e131_addr_t dest;
    String destination_address = "127.0.0.1";
    int universe = 1;
    int port = E131_DEFAULT_PORT;
    bool preview = true;
    uint8_t sequence_number;
    bool use_multicast;

protected:
    static void _bind_methods();

public:
    SacnSender();
    ~SacnSender();

    void set_destination_address(const String& address);
    String get_destination_address() const;

    void set_universe(const int& universe_id);
    int get_universe() const;

    void set_port(const int& port_number);
    int get_port() const;

    void set_preview(const bool& is_preview);
    bool get_preview() const;

    void set_use_multicast(const bool& use_multicast);
    bool get_use_multicast() const;

    void send_data(const PackedByteArray& data);
};

}

#endif
