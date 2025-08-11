#ifndef RECEIVER_HPP
#define RECEIVER_HPP

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>

#include <thread>
#include <atomic>

namespace godot {

class SacnReceiver : public Node {
    GDCLASS(SacnReceiver, Node);

public:
    SacnReceiver();
    ~SacnReceiver();

    void set_universes(const PackedInt32Array& universe_list);
    PackedInt32Array get_universes() const;
    PackedByteArray get_data() const;

public:
    static void _bind_methods();
    void _ready() override;

private:
    void _receive_packets();

    std::thread receiver_thread;
    std::atomic<bool> running {true};
    std::mutex universes_mutex;

    PackedInt32Array universes;
    PackedByteArray data;
};

}

#endif
