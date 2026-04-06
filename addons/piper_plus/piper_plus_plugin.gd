@tool
extends EditorPlugin
## Piper Plus TTS editor plugin.
## Adds tool menu items for downloading models and dictionaries,
## and provides a custom dictionary editor.

const ModelDownloaderScript = preload("res://addons/piper_plus/model_downloader.gd")
const DictionaryEditorScript = preload("res://addons/piper_plus/dictionary_editor.gd")
const InspectorPluginScript = preload("res://addons/piper_plus/piper_tts_inspector_plugin.gd")
const TestSpeechDialogScript = preload("res://addons/piper_plus/test_speech_dialog.gd")
const PiperIcon = preload("res://addons/piper_plus/icon.svg")

var _download_dialog: AcceptDialog
var _dictionary_editor_dialog: AcceptDialog
var _test_speech_dialog: AcceptDialog
var _inspector_plugin: EditorInspectorPlugin


func _enter_tree() -> void:
	add_tool_menu_item("Piper Plus: Download Models...", _show_download_dialog)
	add_tool_menu_item("Piper Plus: Dictionary Editor...", _show_dictionary_editor)
	add_tool_menu_item("Piper Plus: Test Speech...", _show_test_speech_dialog)
	_inspector_plugin = InspectorPluginScript.new(self)
	add_inspector_plugin(_inspector_plugin)


func _exit_tree() -> void:
	remove_tool_menu_item("Piper Plus: Download Models...")
	remove_tool_menu_item("Piper Plus: Dictionary Editor...")
	remove_tool_menu_item("Piper Plus: Test Speech...")
	if _inspector_plugin:
		remove_inspector_plugin(_inspector_plugin)
		_inspector_plugin = null
	if _download_dialog:
		_download_dialog.queue_free()
		_download_dialog = null
	if _dictionary_editor_dialog:
		_dictionary_editor_dialog.queue_free()
		_dictionary_editor_dialog = null
	if _test_speech_dialog:
		_test_speech_dialog.queue_free()
		_test_speech_dialog = null


func _get_plugin_icon() -> Texture2D:
	return PiperIcon


func _show_download_dialog() -> void:
	if _download_dialog:
		_download_dialog.queue_free()
		_download_dialog = null
	_download_dialog = ModelDownloaderScript.create_dialog()
	EditorInterface.get_base_control().add_child(_download_dialog)
	_download_dialog.popup_centered(Vector2i(640, 520))


func _show_dictionary_editor() -> void:
	if _dictionary_editor_dialog:
		_dictionary_editor_dialog.queue_free()
		_dictionary_editor_dialog = null
	_dictionary_editor_dialog = DictionaryEditorScript.create_dialog()
	EditorInterface.get_base_control().add_child(_dictionary_editor_dialog)
	_dictionary_editor_dialog.popup_centered(Vector2i(700, 500))


func _show_test_speech_dialog(target: Object = null) -> void:
	if _test_speech_dialog:
		_test_speech_dialog.queue_free()
		_test_speech_dialog = null
	if target == null:
		target = _get_selected_piper_tts()
	_test_speech_dialog = TestSpeechDialogScript.create_dialog(target)
	EditorInterface.get_base_control().add_child(_test_speech_dialog)
	_test_speech_dialog.popup_centered(Vector2i(720, 420))


func _get_selected_piper_tts() -> Object:
	var selection := EditorInterface.get_selection()
	if selection == null:
		return null
	for node in selection.get_selected_nodes():
		if node != null and node.is_class("PiperTTS"):
			return node
	return null
