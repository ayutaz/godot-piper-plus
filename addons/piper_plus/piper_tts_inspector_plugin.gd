@tool
extends EditorInspectorPlugin

const ModelDownloaderScript = preload("res://addons/piper_plus/model_downloader.gd")
const DictionaryEditorScript = preload("res://addons/piper_plus/dictionary_editor.gd")
const PresetServiceScript = preload("res://addons/piper_plus/preset_service.gd")
const TestSpeechDialogScript = preload("res://addons/piper_plus/test_speech_dialog.gd")
const PiperIcon = preload("res://addons/piper_plus/icon.svg")

var _plugin: EditorPlugin


func _init(plugin: EditorPlugin = null) -> void:
	_plugin = plugin


func _can_handle(object: Object) -> bool:
	return object != null and object.is_class("PiperTTS")


func _parse_begin(object: Object) -> void:
	var panel := PanelContainer.new()
	panel.size_flags_horizontal = Control.SIZE_EXPAND_FILL

	var root := VBoxContainer.new()
	root.add_theme_constant_override("separation", 8)
	panel.add_child(root)

	var header := HBoxContainer.new()
	header.add_theme_constant_override("separation", 8)
	root.add_child(header)

	var icon_rect := TextureRect.new()
	icon_rect.custom_minimum_size = Vector2(20, 20)
	icon_rect.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	icon_rect.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	icon_rect.texture = PiperIcon
	header.add_child(icon_rect)

	var title := Label.new()
	title.text = "Piper Plus"
	title.add_theme_font_size_override("font_size", 16)
	header.add_child(title)

	var summary := Label.new()
	summary.text = "モデル preset の適用、ダウンロード、辞書編集、試聴をここから行えます。"
	summary.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	root.add_child(summary)

	var preset_row := HBoxContainer.new()
	preset_row.add_theme_constant_override("separation", 6)
	root.add_child(preset_row)

	var preset_picker := OptionButton.new()
	preset_picker.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	for key in PresetServiceScript.list_model_presets():
		preset_picker.add_item(key)
	_select_current_preset(preset_picker, String(object.get("model_path")))
	preset_row.add_child(preset_picker)

	var apply_btn := Button.new()
	apply_btn.text = "Apply Preset"
	apply_btn.pressed.connect(_apply_preset.bind(object, preset_picker))
	preset_row.add_child(apply_btn)

	var button_row := HBoxContainer.new()
	button_row.add_theme_constant_override("separation", 6)
	root.add_child(button_row)

	var download_btn := Button.new()
	download_btn.text = "Downloads"
	download_btn.pressed.connect(_open_download_dialog)
	button_row.add_child(download_btn)

	var dict_btn := Button.new()
	dict_btn.text = "Dictionary"
	dict_btn.pressed.connect(_open_dictionary_dialog)
	button_row.add_child(dict_btn)

	var preview_btn := Button.new()
	preview_btn.text = "Test Speech"
	preview_btn.pressed.connect(_open_test_speech_dialog.bind(object))
	button_row.add_child(preview_btn)

	add_custom_control(panel)


func _select_current_preset(preset_picker: OptionButton, current_model_path: String) -> void:
	if preset_picker.item_count <= 0:
		return

	var current_value := current_model_path.strip_edges()
	var matched_key := PresetServiceScript.match_model_path_to_preset(current_value)
	if not matched_key.is_empty():
		for index in range(preset_picker.item_count):
			if preset_picker.get_item_text(index) == matched_key:
				preset_picker.select(index)
				return

	preset_picker.select(0)


func _apply_preset(target: Object, preset_picker: OptionButton) -> void:
	if target == null or preset_picker.selected < 0:
		return

	var preset_key := preset_picker.get_item_text(preset_picker.selected)
	var preset_values := PresetServiceScript.resolve_preset_application(preset_key)

	if _plugin != null:
		var undo_redo = _plugin.get_undo_redo()
		undo_redo.create_action("Apply Piper Plus Preset")
		undo_redo.add_do_property(target, "model_path", preset_values.get("model_path", ""))
		undo_redo.add_undo_property(target, "model_path", target.get("model_path"))
		undo_redo.add_do_property(target, "config_path", preset_values.get("config_path", ""))
		undo_redo.add_undo_property(target, "config_path", target.get("config_path"))
		undo_redo.add_do_property(target, "dictionary_path", preset_values.get("dictionary_path", ""))
		undo_redo.add_undo_property(target, "dictionary_path", target.get("dictionary_path"))
		undo_redo.add_do_property(target, "language_id", int(preset_values.get("language_id", -1)))
		undo_redo.add_undo_property(target, "language_id", target.get("language_id"))
		undo_redo.add_do_property(target, "language_code", String(preset_values.get("language_code", "")))
		undo_redo.add_undo_property(target, "language_code", target.get("language_code"))
		undo_redo.commit_action()
		return

	target.set("model_path", preset_values.get("model_path", ""))
	target.set("config_path", preset_values.get("config_path", ""))
	target.set("dictionary_path", preset_values.get("dictionary_path", ""))
	target.set("language_id", int(preset_values.get("language_id", -1)))
	target.set("language_code", String(preset_values.get("language_code", "")))


func _popup_ephemeral_dialog(dialog: AcceptDialog, size: Vector2i) -> void:
	dialog.confirmed.connect(dialog.queue_free)
	dialog.canceled.connect(dialog.queue_free)
	dialog.close_requested.connect(dialog.queue_free)
	EditorInterface.get_base_control().add_child(dialog)
	dialog.popup_centered(size)


func _open_download_dialog() -> void:
	var dialog := ModelDownloaderScript.create_dialog()
	_popup_ephemeral_dialog(dialog, Vector2i(640, 520))


func _open_dictionary_dialog() -> void:
	var dialog := DictionaryEditorScript.create_dialog()
	_popup_ephemeral_dialog(dialog, Vector2i(700, 500))


func _open_test_speech_dialog(target: Object) -> void:
	var dialog := TestSpeechDialogScript.create_dialog(target)
	_popup_ephemeral_dialog(dialog, Vector2i(720, 420))
