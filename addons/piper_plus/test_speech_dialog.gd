@tool
extends RefCounted

const _META_TARGET := &"_piper_preview_target"
const _META_PREVIEW_TTS := &"_piper_preview_tts"
const _META_PLAYER := &"_piper_preview_player"

const DEFAULT_JA_PREVIEW_TEXT := "こんにちは、Godot から試聴しています。"
const DEFAULT_EN_PREVIEW_TEXT := "Hello from the Piper Plus preview dialog."


static func create_dialog(target: Object = null) -> AcceptDialog:
	var dialog := AcceptDialog.new()
	dialog.title = "Piper Plus - Test Speech"
	dialog.ok_button_text = "Close"
	dialog.exclusive = false
	dialog.set_meta(_META_TARGET, target)

	var root := VBoxContainer.new()
	root.set_anchors_preset(Control.PRESET_FULL_RECT)
	root.add_theme_constant_override("separation", 8)
	dialog.add_child(root)

	var summary := Label.new()
	summary.name = "SummaryLabel"
	summary.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	summary.text = _target_summary(target)
	root.add_child(summary)

	var text_edit := TextEdit.new()
	text_edit.name = "PreviewTextEdit"
	text_edit.custom_minimum_size = Vector2(0, 120)
	text_edit.wrap_mode = TextEdit.LINE_WRAPPING_BOUNDARY
	text_edit.text = _default_preview_text(target)
	root.add_child(text_edit)

	var button_row := HBoxContainer.new()
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
	status_label.text = "PiperTTS ノードの現在設定を使って editor 上で試聴します。"
	root.add_child(status_label)

	preview_btn.pressed.connect(
		_on_preview_pressed.bind(dialog, text_edit, status_label, preview_btn, stop_btn)
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


static func _default_preview_text(target: Object) -> String:
	if target == null:
		return DEFAULT_JA_PREVIEW_TEXT

	var language_code := String(target.get("language_code")).to_lower()
	if language_code.begins_with("en"):
		return DEFAULT_EN_PREVIEW_TEXT
	return DEFAULT_JA_PREVIEW_TEXT


static func _copy_target_properties(preview_tts: Object, target: Object) -> void:
	for property_name in [
		"model_path",
		"config_path",
		"dictionary_path",
		"openjtalk_library_path",
		"custom_dictionary_path",
		"speaker_id",
		"language_id",
		"language_code",
		"speech_rate",
		"noise_scale",
		"noise_w",
		"sentence_silence_seconds",
		"phoneme_silence_seconds",
		"execution_provider",
		"gpu_device_id",
	]:
		preview_tts.set(property_name, target.get(property_name))


static func _on_preview_pressed(
	dialog: AcceptDialog,
	text_edit: TextEdit,
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

	var preview_tts = ClassDB.instantiate("PiperTTS")
	if preview_tts == null:
		status_label.text = "PiperTTS クラスを生成できませんでした。GDExtension の読み込み状態を確認してください。"
		return

	var player := AudioStreamPlayer.new()
	dialog.add_child(preview_tts)
	dialog.add_child(player)
	dialog.set_meta(_META_PREVIEW_TTS, preview_tts)
	dialog.set_meta(_META_PLAYER, player)

	_copy_target_properties(preview_tts, target)

	preview_tts.synthesis_completed.connect(
		_on_preview_completed.bind(dialog, status_label, preview_btn, stop_btn)
	)
	preview_tts.synthesis_failed.connect(
		_on_preview_failed.bind(dialog, status_label, preview_btn, stop_btn)
	)

	var init_error := preview_tts.initialize()
	if init_error != OK:
		status_label.text = "initialize() に失敗しました: %s" % init_error
		_cleanup_preview(dialog)
		return

	var synth_error := preview_tts.synthesize_async(preview_text)
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
