@tool
extends RefCounted

const PreviewControllerScript = preload("res://addons/piper_plus/preview_controller.gd")
const MultilingualSampleTextCatalog = preload("res://addons/piper_plus/multilingual_sample_text_catalog.gd")

const _META_TARGET := &"_piper_preview_target"
const _META_PREVIEW_TTS := &"_piper_preview_tts"
const _META_PLAYER := &"_piper_preview_player"


static func create_dialog(target: Object = null) -> AcceptDialog:
	var dialog := AcceptDialog.new()
	dialog.title = "Piper Plus - Test Speech"
	dialog.ok_button_text = "Close"
	dialog.exclusive = false
	dialog.set_meta(_META_TARGET, target)

	var root := VBoxContainer.new()
	root.name = "RootVBox"
	root.set_anchors_preset(Control.PRESET_FULL_RECT)
	root.add_theme_constant_override("separation", 8)
	dialog.add_child(root)

	var summary := Label.new()
	summary.name = "SummaryLabel"
	summary.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	summary.text = _target_summary(target)
	root.add_child(summary)

	var language_row := HBoxContainer.new()
	language_row.name = "LanguageRow"
	language_row.add_theme_constant_override("separation", 8)
	root.add_child(language_row)

	var language_label := Label.new()
	language_label.name = "LanguageLabel"
	language_label.text = "Language"
	language_row.add_child(language_label)

	var language_picker := OptionButton.new()
	language_picker.name = "LanguagePicker"
	language_picker.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	for option in MultilingualSampleTextCatalog.get_language_options():
		var language_code := String(option.get("language_code", ""))
		language_picker.add_item(String(option.get("display_name", language_code)))
		language_picker.set_item_metadata(language_picker.item_count - 1, language_code)
	language_row.add_child(language_picker)

	var template_label := Label.new()
	template_label.name = "TemplateLabel"
	template_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	root.add_child(template_label)

	var text_edit := TextEdit.new()
	text_edit.name = "PreviewTextEdit"
	text_edit.custom_minimum_size = Vector2(0, 120)
	text_edit.wrap_mode = TextEdit.LINE_WRAPPING_BOUNDARY
	root.add_child(text_edit)

	var button_row := HBoxContainer.new()
	button_row.name = "ButtonRow"
	button_row.add_theme_constant_override("separation", 6)
	root.add_child(button_row)

	var preview_btn := Button.new()
	preview_btn.name = "PreviewButton"
	preview_btn.text = "Preview"
	preview_btn.disabled = target == null
	button_row.add_child(preview_btn)

	var stop_btn := Button.new()
	stop_btn.name = "StopButton"
	stop_btn.text = "Stop"
	stop_btn.disabled = true
	button_row.add_child(stop_btn)

	var status_label := Label.new()
	status_label.name = "StatusLabel"
	status_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	status_label.text = "PiperTTS ノードの現在設定と選択したテンプレートを使って editor 上で試聴します。"
	root.add_child(status_label)

	var selected_language_code := _initial_language_code(target)
	_apply_language_selection(language_picker, text_edit, template_label, selected_language_code, true)

	language_picker.item_selected.connect(
		_on_language_selected.bind(dialog, language_picker, text_edit, template_label)
	)
	preview_btn.pressed.connect(
		_on_preview_pressed.bind(dialog, text_edit, language_picker, status_label, preview_btn, stop_btn)
	)
	stop_btn.pressed.connect(
		_stop_preview.bind(dialog, status_label, preview_btn, stop_btn)
	)
	dialog.confirmed.connect(_cleanup_preview.bind(dialog))
	dialog.canceled.connect(_cleanup_preview.bind(dialog))

	return dialog


static func _target_summary(target: Object) -> String:
	if target == null:
		return "PiperTTS ノードを選択した状態で開くと、その設定を使って試聴できます。"

	var model_path := String(target.get("model_path"))
	if model_path.is_empty():
		return "現在の PiperTTS に model_path が設定されていません。Inspector の preset か downloader でモデルを用意してください。"

	return "現在のモデル: %s" % model_path


static func _initial_language_code(target: Object) -> String:
	if target == null or not is_instance_valid(target):
		return MultilingualSampleTextCatalog.get_default_language_code()

	var target_language_code := String(target.get("language_code"))
	return MultilingualSampleTextCatalog.resolve_language_code(target_language_code)


static func _language_index_by_code(language_picker: OptionButton, language_code: String) -> int:
	var resolved := MultilingualSampleTextCatalog.resolve_language_code(language_code)
	for index in range(language_picker.item_count):
		if String(language_picker.get_item_metadata(index)) == resolved:
			return index
	return -1


static func _selected_language_code(language_picker: OptionButton) -> String:
	if language_picker == null or language_picker.selected < 0:
		return MultilingualSampleTextCatalog.get_default_language_code()

	var language_code := String(language_picker.get_item_metadata(language_picker.selected))
	if language_code.is_empty():
		language_code = String(language_picker.get_item_text(language_picker.selected))
	return MultilingualSampleTextCatalog.resolve_language_code(language_code)


static func _apply_language_selection(
	language_picker: OptionButton,
	text_edit: TextEdit,
	template_label: Label,
	language_code: String,
	update_text: bool
) -> void:
	var resolved_language_code := MultilingualSampleTextCatalog.resolve_language_code(language_code)
	var index := _language_index_by_code(language_picker, resolved_language_code)
	if index >= 0 and language_picker.selected != index:
		language_picker.select(index)

	var display_name := MultilingualSampleTextCatalog.get_language_display_name(resolved_language_code)
	var template_text := MultilingualSampleTextCatalog.get_language_template_text(resolved_language_code)

	if text_edit != null and update_text:
		text_edit.text = template_text

	if template_label != null:
		template_label.text = "Template for %s (%s) is loaded into the editor." % [display_name, resolved_language_code]


static func _on_language_selected(
	index: int,
	dialog: AcceptDialog,
	language_picker: OptionButton,
	text_edit: TextEdit,
	template_label: Label,
) -> void:
	var language_code := _selected_language_code(language_picker)
	_apply_language_selection(language_picker, text_edit, template_label, language_code, true)


static func _on_preview_pressed(
	dialog: AcceptDialog,
	text_edit: TextEdit,
	language_picker: OptionButton,
	status_label: Label,
	preview_btn: Button,
	stop_btn: Button,
) -> void:
	var target: Object = dialog.get_meta(_META_TARGET, null)
	if target == null or not is_instance_valid(target):
		status_label.text = "PiperTTS ノードが見つかりません。対象ノードを選び直してください。"
		preview_btn.disabled = true
		stop_btn.disabled = true
		return

	var preview_text := text_edit.text.strip_edges()
	if preview_text.is_empty():
		status_label.text = "試聴テキストを入力してください。"
		return

	_cleanup_preview(dialog)

	var selected_language_code := _selected_language_code(language_picker)
	var session := PreviewControllerScript.create_preview_session(
		dialog,
		target,
		{"language_code": selected_language_code}
	)
	var preview_tts = session.get("tts", null)
	if preview_tts == null:
		status_label.text = String(session.get("error", "PiperTTS クラスを生成できませんでした。"))
		return

	var player = session.get("player", null)
	dialog.set_meta(_META_PREVIEW_TTS, preview_tts)
	dialog.set_meta(_META_PLAYER, player)

	preview_tts.synthesis_completed.connect(
		_on_preview_completed.bind(dialog, status_label, preview_btn, stop_btn)
	)
	preview_tts.synthesis_failed.connect(
		_on_preview_failed.bind(dialog, status_label, preview_btn, stop_btn)
	)

	var init_error: int = preview_tts.initialize()
	if init_error != OK:
		status_label.text = "initialize() に失敗しました: %s" % init_error
		_cleanup_preview(dialog)
		return

	var synth_error: int = preview_tts.synthesize_async(preview_text)
	if synth_error != OK:
		status_label.text = "synthesize_async() に失敗しました: %s" % synth_error
		_cleanup_preview(dialog)
		return

	status_label.text = "音声を生成しています..."
	preview_btn.disabled = true
	stop_btn.disabled = false


static func _on_preview_completed(
	audio,
	dialog: AcceptDialog,
	status_label: Label,
	preview_btn: Button,
	stop_btn: Button,
) -> void:
	var player = dialog.get_meta(_META_PLAYER, null)
	if player != null and is_instance_valid(player):
		player.stream = audio
		player.play()

	status_label.text = "試聴を再生中です。"
	preview_btn.disabled = false
	stop_btn.disabled = false


static func _on_preview_failed(
	error: String,
	dialog: AcceptDialog,
	status_label: Label,
	preview_btn: Button,
	stop_btn: Button,
) -> void:
	status_label.text = "試聴に失敗しました: %s" % error
	preview_btn.disabled = false
	stop_btn.disabled = true
	_cleanup_preview(dialog)


static func _stop_preview(
	dialog: AcceptDialog,
	status_label: Label,
	preview_btn: Button,
	stop_btn: Button,
) -> void:
	_cleanup_preview(dialog)
	status_label.text = "試聴を停止しました。"
	preview_btn.disabled = false
	stop_btn.disabled = true


static func _cleanup_preview(dialog: AcceptDialog) -> void:
	var preview_tts = dialog.get_meta(_META_PREVIEW_TTS, null)
	if preview_tts != null and is_instance_valid(preview_tts):
		preview_tts.stop()
		preview_tts.queue_free()

	var player = dialog.get_meta(_META_PLAYER, null)
	if player != null and is_instance_valid(player):
		player.stop()
		player.queue_free()

	dialog.set_meta(_META_PREVIEW_TTS, null)
	dialog.set_meta(_META_PLAYER, null)
