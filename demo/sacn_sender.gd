extends SacnSender

var timer = Timer.new()
var sequence_number = 0 # Sequence number is handled by the C++ class

func _ready():
	# Set up the timer
	timer.wait_time = 1.0
	timer.autostart = true
	timer.connect("timeout", Callable(self, "_on_timer_timeout"))
	add_child(timer)

	# Configure the sender (optional, using default values from C++ class)
	# set_destination_address("127.0.0.1")
	# set_universe(1)
	# set_port(5568)
	# set_preview(true)

func _on_timer_timeout():
	# Create a data packet (example: sending 512 channels of data)
	var data = PackedByteArray()
	data.resize(512)
	for i in range(512):
		data[i] = i % 256 # Example data

	# Send the packet using the extended class's method
	send_data(data)

func _exit_tree():
	# Timer will be cleaned up automatically when the node is freed
	pass
