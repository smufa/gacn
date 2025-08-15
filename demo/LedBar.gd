@tool
@icon("theme://icons/OmniLight3D.svg")
extends Node3D

class_name LedBar
@export var length: int = 100:
	set(value):
		length = value
		if Engine.is_editor_hint():
			_spawn_cubes_and_lights()
@export var universe := 1:
	set(value):
		universe = value
		if receiver != null:
			receiver.activate_universe(universe)
@export var strength := 2
@export var spawn_lights: bool = true:
	set(value):
		spawn_lights = value
		if Engine.is_editor_hint():
			_spawn_cubes_and_lights()
var receiver = null

func _ready() -> void:
	receiver = $"../SacnReceiver"
	receiver.activate_universe(universe)
	receiver.connect("data_received", Callable(self, "_on_data_received"))
	_spawn_cubes_and_lights()

func _spawn_cubes_and_lights() -> void:
	# Clear existing children (cubes and lights)
	for child in get_children():
		if child is MeshInstance3D or child is OmniLight3D:
			child.queue_free()

	var shared_box_mesh = BoxMesh.new()
	shared_box_mesh.size = Vector3(0.05, 0.05, 0.05) # Smaller cube

	for i in range(length):
		var cube = MeshInstance3D.new()
		cube.mesh = shared_box_mesh # Share the same mesh
		cube.transform.origin = Vector3(i * 0.05, 0, 0) # Arrange right next to each other
		add_child(cube)

	if spawn_lights:
		var light_chunk_size = 10 # One light for every 10 cubes
		for i in range(0, length, light_chunk_size):
			var light = OmniLight3D.new()
			light.light_energy = 0.2
			light.light_color = Color(1, 1, 1)
			light.omni_range = 2 # Smaller range to not hit other cubes
			light.omni_attenuation = 0.4
			# light.distance_fade_enabled = true
			# light.distance_fade_begin = 0.1
			# Position light in the middle of the chunk
			light.transform.origin = Vector3((i + light_chunk_size / 2.0) * 0.05, 0, 0)
			add_child(light)

func _on_data_received(universe_id: int, data: PackedByteArray):
	if universe_id != universe:
		return # Only process data for this LedBar's universe

	var cubes = []
	var lights = []
	for child in get_children():
		if child is MeshInstance3D:
			cubes.append(child)
		elif child is OmniLight3D:
			lights.append(child)

	var light_chunk_size = 10 # One light for every 10 cubes
	var num_chunks = ceil(float(length) / light_chunk_size)

	for chunk_index in range(num_chunks):
		var avg_r = 0.0
		var avg_g = 0.0
		var avg_b = 0.0
		var cube_count_in_chunk = 0

		for i in range(light_chunk_size):
			var cube_index = chunk_index * light_chunk_size + i
			if cube_index >= cubes.size():
				break

			var dmx_start_index = cube_index * 3
			if dmx_start_index >= data.size():
				break

			var r = float(data[dmx_start_index]) / 255.0
			var g = float(data[dmx_start_index+1]) / 255.0 if dmx_start_index + 1 < data.size() else 0.0
			var b = float(data[dmx_start_index+2]) / 255.0 if dmx_start_index + 2 < data.size() else 0.0
			var color = Color(r, g, b)

			var cube = cubes[cube_index]
			if not cube.mesh:
				cube.mesh = BoxMesh.new()
				(cube.mesh as BoxMesh).size = Vector3(0.05, 0.05, 0.05) # Ensure consistent size

			var material = cube.get_active_material(0)
			if not material:
				material = StandardMaterial3D.new()
				cube.set_surface_override_material(0, material)
			
			if material is StandardMaterial3D:
				material.albedo_color = color # Still set albedo for base color
				material.emission_enabled = true
				material.emission = color
				material.emission_energy_multiplier = strength # Adjust as needed for brightness
			
			avg_r += r
			avg_g += g
			avg_b += b
			cube_count_in_chunk += 1
		
		if cube_count_in_chunk > 0 and chunk_index < lights.size():
			var avg_color = Color(avg_r / cube_count_in_chunk, avg_g / cube_count_in_chunk, avg_b / cube_count_in_chunk)
			var light: OmniLight3D = lights[chunk_index]
			light.light_color = avg_color
