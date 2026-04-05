@tool
extends RefCounted
## Custom dictionary editor for Piper Plus TTS.
## Provides a dialog UI to view and edit JSON-based custom dictionaries.
## The dictionary maps words (keys) to their readings/pronunciations (values).
##
## Default save format (piper-plus custom_dictionary.cpp compatible):
## {
##   "version": "2.0",
##   "entries": {
##     "word": { "pronunciation": "reading", "priority": 5 }
##   }
## }

const DEFAULT_DICT_PATH := "res://addons/piper_plus/dictionaries/custom_dictionary.json"

# ---------------------------------------------------------------------------
# Factory
# ---------------------------------------------------------------------------

## Creates and returns the dictionary editor dialog.
static func create_dialog() -> AcceptDialog:
	var dialog := AcceptDialog.new()
	dialog.title = "Piper Plus - Custom Dictionary Editor"
	dialog.ok_button_text = "Close"
	dialog.exclusive = false

	# --- Root layout ---
	var root_vbox := VBoxContainer.new()
	root_vbox.set_anchors_preset(Control.PRESET_FULL_RECT)
	root_vbox.add_theme_constant_override("separation", 6)
	dialog.add_child(root_vbox)

	# --- File path row ---
	var path_hbox := HBoxContainer.new()
	path_hbox.add_theme_constant_override("separation", 4)
	root_vbox.add_child(path_hbox)

	var path_label := Label.new()
	path_label.text = "Dictionary file:"
	path_hbox.add_child(path_label)

	var path_edit := LineEdit.new()
	path_edit.name = "PathEdit"
	path_edit.text = DEFAULT_DICT_PATH
	path_edit.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	path_hbox.add_child(path_edit)

	var load_btn := Button.new()
	load_btn.name = "LoadButton"
	load_btn.text = "Load"
	path_hbox.add_child(load_btn)

	var save_btn := Button.new()
	save_btn.name = "SaveButton"
	save_btn.text = "Save"
	path_hbox.add_child(save_btn)

	# --- Column headers ---
	var header_hbox := HBoxContainer.new()
	header_hbox.add_theme_constant_override("separation", 4)
	root_vbox.add_child(header_hbox)

	var pattern_header := Label.new()
	pattern_header.text = "Pattern (word)"
	pattern_header.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	pattern_header.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	header_hbox.add_child(pattern_header)

	var replacement_header := Label.new()
	replacement_header.text = "Replacement (reading)"
	replacement_header.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	replacement_header.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	header_hbox.add_child(replacement_header)

	# Spacer for delete button column
	var spacer := Control.new()
	spacer.custom_minimum_size = Vector2(32, 0)
	header_hbox.add_child(spacer)

	# --- Scroll area for entries ---
	var scroll := ScrollContainer.new()
	scroll.size_flags_vertical = Control.SIZE_EXPAND_FILL
	scroll.custom_minimum_size = Vector2(0, 280)
	root_vbox.add_child(scroll)

	var entries_vbox := VBoxContainer.new()
	entries_vbox.name = "EntriesVBox"
	entries_vbox.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	entries_vbox.add_theme_constant_override("separation", 2)
	scroll.add_child(entries_vbox)

	# --- Add entry button ---
	var add_hbox := HBoxContainer.new()
	add_hbox.add_theme_constant_override("separation", 4)
	root_vbox.add_child(add_hbox)

	var add_btn := Button.new()
	add_btn.name = "AddButton"
	add_btn.text = "+ Add Entry"
	add_btn.size_flags_horizontal = Control.SIZE_SHRINK_CENTER
	add_hbox.add_child(add_btn)

	# --- Status label ---
	var status_label := Label.new()
	status_label.name = "StatusLabel"
	status_label.text = ""
	status_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	root_vbox.add_child(status_label)

	# --- Connect signals ---
	load_btn.pressed.connect(_on_load_pressed.bind(dialog, path_edit, entries_vbox, status_label))
	save_btn.pressed.connect(_on_save_pressed.bind(dialog, path_edit, entries_vbox, status_label))
	add_btn.pressed.connect(_add_entry_row.bind(entries_vbox, "", ""))

	# Auto-load if file exists
	if FileAccess.file_exists(DEFAULT_DICT_PATH):
		# Defer the load so the dialog is fully ready
		load_btn.pressed.emit()

	return dialog


# ---------------------------------------------------------------------------
# Entry row management
# ---------------------------------------------------------------------------

static func _add_entry_row(
	entries_vbox: VBoxContainer,
	pattern: String,
	replacement: String,
) -> void:
	var hbox := HBoxContainer.new()
	hbox.add_theme_constant_override("separation", 4)
	entries_vbox.add_child(hbox)

	var pattern_edit := LineEdit.new()
	pattern_edit.name = "PatternEdit"
	pattern_edit.text = pattern
	pattern_edit.placeholder_text = "word"
	pattern_edit.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	hbox.add_child(pattern_edit)

	var replacement_edit := LineEdit.new()
	replacement_edit.name = "ReplacementEdit"
	replacement_edit.text = replacement
	replacement_edit.placeholder_text = "reading / pronunciation"
	replacement_edit.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	hbox.add_child(replacement_edit)

	var delete_btn := Button.new()
	delete_btn.text = "x"
	delete_btn.custom_minimum_size = Vector2(32, 0)
	delete_btn.tooltip_text = "Remove this entry"
	delete_btn.pressed.connect(hbox.queue_free)
	hbox.add_child(delete_btn)


static func _clear_entries(entries_vbox: VBoxContainer) -> void:
	for child: Node in entries_vbox.get_children():
		child.queue_free()


# ---------------------------------------------------------------------------
# Load
# ---------------------------------------------------------------------------

static func _on_load_pressed(
	_dialog: AcceptDialog,
	path_edit: LineEdit,
	entries_vbox: VBoxContainer,
	status_label: Label,
) -> void:
	var file_path: String = path_edit.text.strip_edges()
	if file_path.is_empty():
		status_label.text = "Please enter a file path."
		return

	if not FileAccess.file_exists(file_path):
		status_label.text = "File not found: " + file_path
		return

	var file := FileAccess.open(file_path, FileAccess.READ)
	if file == null:
		status_label.text = "Cannot open file: " + file_path
		return

	var json_text := file.get_as_text()
	file.close()

	var json := JSON.new()
	var parse_error := json.parse(json_text)
	if parse_error != OK:
		status_label.text = "JSON parse error at line " + str(json.get_error_line()) + ": " + json.get_error_message()
		return

	var data: Variant = json.data
	if not data is Dictionary:
		status_label.text = "Invalid dictionary format: root must be an object."
		return

	var dict: Dictionary = data
	var loaded_entries: Array[Dictionary] = []

	if dict.has("entries"):
		var raw_entries: Variant = dict["entries"]
		if raw_entries is Dictionary:
			var entries_dict: Dictionary = raw_entries
			for pattern_variant in entries_dict.keys():
				var pattern := str(pattern_variant)
				var entry_value: Variant = entries_dict[pattern_variant]
				if entry_value is Dictionary:
					var entry_dict: Dictionary = entry_value
					loaded_entries.append({
						"pattern": pattern,
						"replacement": str(entry_dict.get("pronunciation", "")),
					})
				elif entry_value is String:
					loaded_entries.append({
						"pattern": pattern,
						"replacement": str(entry_value),
					})
		elif raw_entries is Array:
			for entry_variant in raw_entries:
				if entry_variant is Dictionary:
					var entry_dict: Dictionary = entry_variant
					loaded_entries.append({
						"pattern": str(entry_dict.get("pattern", "")),
						"replacement": str(entry_dict.get("replacement", "")),
					})
	else:
		for key_variant in dict.keys():
			var key := str(key_variant)
			if key in ["version", "description", "metadata", "entries"]:
				continue

			var value: Variant = dict[key_variant]
			if value is String:
				loaded_entries.append({
					"pattern": key,
					"replacement": str(value),
				})

	_clear_entries(entries_vbox)

	# Need to wait one frame for queue_free to take effect before adding new rows
	await entries_vbox.get_tree().process_frame

	for entry in loaded_entries:
		if entry is Dictionary:
			var pattern: String = str(entry.get("pattern", ""))
			var replacement: String = str(entry.get("replacement", ""))
			_add_entry_row(entries_vbox, pattern, replacement)

	status_label.text = "Loaded " + str(loaded_entries.size()) + " entries from " + file_path


# ---------------------------------------------------------------------------
# Save
# ---------------------------------------------------------------------------

static func _on_save_pressed(
	_dialog: AcceptDialog,
	path_edit: LineEdit,
	entries_vbox: VBoxContainer,
	status_label: Label,
) -> void:
	var file_path: String = path_edit.text.strip_edges()
	if file_path.is_empty():
		status_label.text = "Please enter a file path."
		return

	# Ensure parent directory exists
	var global_path: String = ProjectSettings.globalize_path(file_path)
	var parent_dir: String = global_path.get_base_dir()
	DirAccess.make_dir_recursive_absolute(parent_dir)

	# Collect entries from UI
	var entries := {}
	for child: Node in entries_vbox.get_children():
		if child is HBoxContainer:
			var pattern_edit: LineEdit = child.get_node_or_null("PatternEdit")
			var replacement_edit: LineEdit = child.get_node_or_null("ReplacementEdit")
			if pattern_edit and replacement_edit:
				var pattern: String = pattern_edit.text.strip_edges()
				var replacement: String = replacement_edit.text.strip_edges()
				if not pattern.is_empty() and not replacement.is_empty():
					entries[pattern] = {
						"pronunciation": replacement,
						"priority": 5,
					}

	var data := {
		"version": "2.0",
		"description": "Custom dictionary edited in Godot",
		"entries": entries,
	}
	var json_text := JSON.stringify(data, "  ")

	var file := FileAccess.open(file_path, FileAccess.WRITE)
	if file == null:
		status_label.text = "Cannot write to: " + file_path
		return

	file.store_string(json_text)
	file.close()

	status_label.text = "Saved " + str(entries.size()) + " entries to " + file_path

	# Trigger filesystem scan so Godot sees the new/updated file
	if Engine.is_editor_hint():
		EditorInterface.get_resource_filesystem().scan()
