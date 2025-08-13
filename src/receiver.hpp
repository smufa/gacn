#ifndef RECEIVER_HPP
#define RECEIVER_HPP

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>

#include <thread>
#include <atomic>
#include <map>
#include <mutex>
#include <condition_variable>
#include <set>

namespace godot {

class SacnReceiver : public Node {
    GDCLASS(SacnReceiver, Node);

private:
    bool preview = false;
    bool inited = false;
    int sockfd = -1;
    std::map<uint16_t, uint8_t> last_seq;
    std::atomic<bool> running = false;
    std::thread receiver_thread;
    std::set<uint16_t> active_universes;
    std::mutex mtx;
    std::condition_variable cv;
    std::map<uint16_t, PackedByteArray> received_data;

    void _receiver_thread_func();

public:
    SacnReceiver();
    ~SacnReceiver();

    void activate_universe(uint16_t universe_id);
    void set_preview(bool p_enable);
    bool is_preview() const; // Add a getter for the preview property

    static void _bind_methods();
    void _ready() override;
    void _exit_tree() override; // Declare _exit_tree
    void _exit();
    void _process(double delta) override;
    void _notification(int p_what);
    void init_sacn();
};

}

#endif
