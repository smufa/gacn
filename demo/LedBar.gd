@tool
@icon("theme://icons/OmniLight3D.svg")
extends Node3D

class_name LedBar
@export var length: int = 100:
	set(value):
		length = value
		if Engine.is_editor_hint():
			_spawn_cubes_and_lights()
@export var universe := 1
@export var strength := 2
@export var spawn_lights: bool = true:
	set(value):
		spawn_lights = value
		if Engine.is_editor_hint():
			_spawn_cubes_and_lights()

func _ready() -> void:
	var receiver = $"../SacnReceiver"
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
			var light = OmniLight3D.new()
			light.light_energy = 2.0
			light.light_color = Color(1, 1, 1)
			light.omni_range = 1 # Smaller range to not hit other cubes
			light.transform.origin = cube.transform.origin
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

	for i in range(0, min(data.size(), length * 3), 3):
		var cube_index = i / 3
		if cube_index >= cubes.size():
			break # No more cubes to color

		var r = float(data[i]) / 255.0
		var g = float(data[i+1]) / 255.0 if i + 1 < data.size() else 0.0
		var b = float(data[i+2]) / 255.0 if i + 2 < data.size() else 0.0
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
		
		if cube_index < lights.size():
			var light = lights[cube_index]
			light.light_color = color
