@tool
extends RefCounted
## Model and dictionary downloader for Piper Plus TTS.
## Provides a dialog UI to select and download models/dictionaries
## required for TTS synthesis.

const DownloadCatalog = preload("res://addons/piper_plus/download_catalog.gd")

# ---------------------------------------------------------------------------
# Internal state keys (used as node names / meta)
# ---------------------------------------------------------------------------
const _META_ITEMS := &"_download_items"

# ---------------------------------------------------------------------------
# Factory
# ---------------------------------------------------------------------------

## Creates and returns the download dialog. Caller is responsible for
## adding it to the scene tree and calling popup_centered().
static func create_dialog() -> AcceptDialog:
	var dialog := AcceptDialog.new()
	dialog.title = "Piper Plus - Download Models & Dictionaries"
	dialog.ok_button_text = "Close"
	dialog.exclusive = false

	# --- Root layout ---
	var root_vbox := VBoxContainer.new()
	root_vbox.set_anchors_preset(Control.PRESET_FULL_RECT)
	root_vbox.add_theme_constant_override("separation", 8)
	dialog.add_child(root_vbox)

	# --- Header ---
	var header := Label.new()
	header.text = "Select items to download:"
	root_vbox.add_child(header)

	# --- Scroll area for checkboxes ---
	var scroll := ScrollContainer.new()
	scroll.size_flags_vertical = Control.SIZE_EXPAND_FILL
	scroll.custom_minimum_size = Vector2(0, 200)
	root_vbox.add_child(scroll)

	var items_vbox := VBoxContainer.new()
	items_vbox.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	items_vbox.add_theme_constant_override("separation", 4)
	scroll.add_child(items_vbox)

	var checkboxes: Dictionary = {}  # key -> CheckBox

	for key: String in DownloadCatalog.list_item_keys():
		var item: Dictionary = DownloadCatalog.get_item_definition(key)
		var hbox := HBoxContainer.new()
		hbox.add_theme_constant_override("separation", 8)
		items_vbox.add_child(hbox)

		var cb := CheckBox.new()
		cb.text = key
		cb.tooltip_text = item["description"]
		cb.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		hbox.add_child(cb)

		# Show installed indicator
		var dest_path: String = item["dest"]
		var installed := DownloadCatalog.is_item_installed(key)
		var status_label := Label.new()
		if installed:
			status_label.text = "[installed]"
			status_label.add_theme_color_override("font_color", Color(0.3, 0.8, 0.3))
		else:
			status_label.text = "[not installed]"
			status_label.add_theme_color_override("font_color", Color(0.8, 0.4, 0.4))
		hbox.add_child(status_label)

		var desc_label := Label.new()
		desc_label.text = "  " + item["description"]
		desc_label.add_theme_color_override("font_color", Color(0.7, 0.7, 0.7))
		desc_label.add_theme_font_size_override("font_size", 12)
		items_vbox.add_child(desc_label)

		checkboxes[key] = cb

	# --- Separator ---
	var sep := HSeparator.new()
	root_vbox.add_child(sep)

	# --- Progress section ---
	var progress_bar := ProgressBar.new()
	progress_bar.name = "ProgressBar"
	progress_bar.custom_minimum_size = Vector2(0, 24)
	progress_bar.value = 0.0
	progress_bar.visible = false
	root_vbox.add_child(progress_bar)

	var status_label := Label.new()
	status_label.name = "StatusLabel"
	status_label.text = ""
	status_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	root_vbox.add_child(status_label)

	# --- Download button ---
	var download_btn := Button.new()
	download_btn.name = "DownloadButton"
	download_btn.text = "Download Selected"
	download_btn.custom_minimum_size = Vector2(200, 36)
	download_btn.size_flags_horizontal = Control.SIZE_SHRINK_CENTER
	root_vbox.add_child(download_btn)

	# Store references via meta
	dialog.set_meta(_META_ITEMS, checkboxes)

	# Connect download button
	download_btn.pressed.connect(
		_on_download_pressed.bind(dialog, checkboxes, progress_bar, status_label, download_btn)
	)

	return dialog


static func list_item_keys() -> PackedStringArray:
	return DownloadCatalog.list_item_keys()


static func list_model_item_keys() -> PackedStringArray:
	return DownloadCatalog.list_model_item_keys()


static func get_item_definition(key: String) -> Dictionary:
	return DownloadCatalog.get_item_definition(key)


static func get_primary_model_path(key: String) -> String:
	return DownloadCatalog.get_primary_model_path(key)


static func get_recommended_dictionary_path(key: String) -> String:
	return DownloadCatalog.get_recommended_dictionary_path(key)


static func is_item_installed(key: String) -> bool:
	return DownloadCatalog.is_item_installed(key)


# ---------------------------------------------------------------------------
# Check if an item is already installed
# ---------------------------------------------------------------------------

static func _is_item_installed(key: String, _item: Dictionary) -> bool:
	return DownloadCatalog.is_item_installed(key)


static func _has_compiled_openjtalk_dictionary(path: String) -> bool:
	return DownloadCatalog._has_compiled_openjtalk_dictionary(path)


# ---------------------------------------------------------------------------
# Download logic
# ---------------------------------------------------------------------------

static func _on_download_pressed(
	dialog: AcceptDialog,
	checkboxes: Dictionary,
	progress_bar: ProgressBar,
	status_label: Label,
	download_btn: Button,
) -> void:
	# Collect selected items
	var selected_keys: Array[String] = []
	for key: String in checkboxes:
		var cb: CheckBox = checkboxes[key]
		if cb.button_pressed:
			selected_keys.append(key)

	if selected_keys.is_empty():
		status_label.text = "No items selected."
		return

	download_btn.disabled = true
	progress_bar.visible = true
	progress_bar.value = 0.0

	# Count total files to download
	var total_files := 0
	for key: String in selected_keys:
		var item: Dictionary = DownloadCatalog.get_item_definition(key)
		total_files += item["files"].size()

	var completed_files := 0

	for key: String in selected_keys:
		var item: Dictionary = DownloadCatalog.get_item_definition(key)
		var dest: String = item["dest"]
		var global_dest: String = ProjectSettings.globalize_path(dest)

		# Ensure destination directory exists
		DirAccess.make_dir_recursive_absolute(global_dest)

		status_label.text = "Downloading: " + key + "..."

		for file_entry: Dictionary in item["files"]:
			var url: String = file_entry["url"]
			var filename: String = file_entry["filename"]
			var extract: bool = file_entry.get("extract", false)
			var save_path: String = global_dest + filename

			status_label.text = "Downloading: " + filename + "..."

			var error := await _download_file(dialog, url, save_path)
			if error != OK:
				status_label.text = "Failed to download: " + filename + " (error " + str(error) + ")"
				download_btn.disabled = false
				progress_bar.visible = false
				return

			# Extract zip if needed
			if extract:
				status_label.text = "Extracting: " + filename + "..."
				var extract_error := _extract_zip(save_path, global_dest)
				if extract_error != OK:
					status_label.text = "Failed to extract: " + filename
					download_btn.disabled = false
					progress_bar.visible = false
					return
				# Remove the zip after extraction
				DirAccess.remove_absolute(save_path)

			completed_files += 1
			progress_bar.value = float(completed_files) / float(total_files) * 100.0

	status_label.text = "All downloads completed successfully!"
	progress_bar.value = 100.0
	download_btn.disabled = false

	# Trigger filesystem scan so Godot sees the new files
	if Engine.is_editor_hint():
		EditorInterface.get_resource_filesystem().scan()


# ---------------------------------------------------------------------------
# Single file download using HTTPRequest
# ---------------------------------------------------------------------------

static func _download_file(parent: Node, url: String, save_path: String) -> Error:
	var http_request := HTTPRequest.new()
	http_request.download_file = save_path
	http_request.use_threads = true
	http_request.timeout = 300  # 5 minute timeout for large files
	parent.add_child(http_request)

	var err := http_request.request(url)
	if err != OK:
		http_request.queue_free()
		return err

	# Wait for completion
	var result: Array = await http_request.request_completed
	http_request.queue_free()

	# result = [result: int, response_code: int, headers: PackedStringArray, body: PackedByteArray]
	var http_result: int = result[0]
	var response_code: int = result[1]

	if http_result != HTTPRequest.RESULT_SUCCESS:
		return FAILED

	if response_code < 200 or response_code >= 300:
		# Handle redirects - HTTPRequest should follow them automatically,
		# but check for non-success codes
		return FAILED

	return OK


# ---------------------------------------------------------------------------
# ZIP extraction using ZIPReader (Godot 4.x)
# ---------------------------------------------------------------------------

static func _extract_zip(zip_path: String, dest_dir: String) -> Error:
	var reader := ZIPReader.new()
	var err := reader.open(zip_path)
	if err != OK:
		return err

	# Normalize dest_dir for path comparison
	var safe_dest := ProjectSettings.globalize_path(dest_dir).simplify_path()

	var files := reader.get_files()
	for file_path: String in files:
		# Skip directory entries
		if file_path.ends_with("/"):
			var dir_path := dest_dir.path_join(file_path)
			# ZipSlip check
			var global_dir := ProjectSettings.globalize_path(dir_path).simplify_path()
			if not global_dir.begins_with(safe_dest):
				continue
			DirAccess.make_dir_recursive_absolute(dir_path)
			continue

		# Ensure parent directory exists
		var full_path := dest_dir.path_join(file_path)
		# ZipSlip check: ensure extracted path stays within destination
		var global_path := ProjectSettings.globalize_path(full_path).simplify_path()
		if not global_path.begins_with(safe_dest):
			push_warning("Skipping suspicious ZIP entry: " + file_path)
			continue

		var parent_dir := full_path.get_base_dir()
		DirAccess.make_dir_recursive_absolute(parent_dir)

		# Read and write file
		var data := reader.read_file(file_path)
		var out_file := FileAccess.open(full_path, FileAccess.WRITE)
		if out_file == null:
			reader.close()
			return FAILED
		out_file.store_buffer(data)
		out_file.close()

	reader.close()
	return OK
