@tool
extends EditorPlugin
## Piper Plus TTS editor plugin.
## Adds tool menu items for downloading models and dictionaries,
## and provides a custom dictionary editor.

const ModelDownloaderScript = preload("res://addons/piper_plus/model_downloader.gd")
const DictionaryEditorScript = preload("res://addons/piper_plus/dictionary_editor.gd")

var _download_dialog: AcceptDialog
var _dictionary_editor_dialog: AcceptDialog


func _enter_tree() -> void:
	add_tool_menu_item("Piper Plus: Download Models...", _show_download_dialog)
	add_tool_menu_item("Piper Plus: Dictionary Editor...", _show_dictionary_editor)


func _exit_tree() -> void:
	remove_tool_menu_item("Piper Plus: Download Models...")
	remove_tool_menu_item("Piper Plus: Dictionary Editor...")
	if _download_dialog:
		_download_dialog.queue_free()
		_download_dialog = null
	if _dictionary_editor_dialog:
		_dictionary_editor_dialog.queue_free()
		_dictionary_editor_dialog = null


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
