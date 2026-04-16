extends Control

const CSS10_ASSET_CANDIDATES := [
	{
		"model_path": "res://piper_plus_assets/models/css10/css10-ja-6lang-fp16.onnx",
		"config_path": "res://piper_plus_assets/models/css10/config.json",
	},
	{
		"model_path": "res://addons/piper_plus/models/css10/css10-ja-6lang-fp16.onnx",
		"config_path": "res://addons/piper_plus/models/css10/config.json",
	},
]
const OPENJTALK_DICT_CANDIDATES := [
	"res://piper_plus_assets/dictionaries/open_jtalk_dic_utf_8-1.11",
	"res://addons/piper_plus/dictionaries/open_jtalk_dic_utf_8-1.11",
	"res://models/openjtalk_dic",
]
const DEFAULT_LANGUAGE_CODE := "ja"
const FREE_INPUT_LABEL := "-- 自由入力 --"
const LANGUAGE_ORDER := ["ja", "en", "zh", "es", "fr", "pt"]
const LANGUAGE_CONFIGS := {
	"ja": {
		"label": "日本語",
		"info_title": "日本語: Rust WASM jpreprocess 音素化",
		"info_description": "内蔵辞書付きの Rust WASM jpreprocess を使った高精度な日本語音素化です。",
		"test_text": "こんにちは、今日はとても良い天気ですね。",
		"templates": [
			"こんにちは、今日はとても良い天気ですね。",
			"これはPiper Plusの音声合成デモです。",
			"ニューラル音声合成はとても自然に聞こえます。",
			"桜の花が美しく咲いています。",
			"東京タワーは東京のシンボルです。",
		],
	},
	"en": {
		"label": "英語",
		"info_title": "英語: ルールベース G2P + Piper 音声合成",
		"info_description": "ルールベースの G2P と Piper によるニューラル音声合成を使います。",
		"test_text": "Hello world! This is a test of the Piper text to speech system.",
		"templates": [
			"Hello world!",
			"This is a test of the Piper text to speech system.",
			"How are you today?",
			"Welcome to the Piper speech synthesis demo.",
			"The quick brown fox jumps over the lazy dog.",
		],
	},
	"zh": {
		"label": "中国語",
		"info_title": "中国語: 文字ベース + Piper 音声合成",
		"info_description": "文字ベースの音素化と Piper によるニューラル音声合成を使います。",
		"test_text": "你好，今天天气非常好。",
		"templates": [
			"你好，今天天气非常好。",
			"欢迎使用语音合成演示。",
			"中文语音合成听起来非常自然。",
			"春天的花开得很美。",
		],
	},
	"es": {
		"label": "スペイン語",
		"info_title": "スペイン語: ルールベース + Piper 音声合成",
		"info_description": "ルールベースの音素化と Piper によるニューラル音声合成を使います。",
		"test_text": "Hola, el tiempo es hermoso, vamos a dar un paseo.",
		"templates": [
			"Hola, el tiempo es hermoso, vamos a dar un paseo.",
			"Bienvenido a la demostración de síntesis de voz.",
			"La síntesis de voz en español suena muy natural.",
			"Las flores de primavera son muy hermosas.",
		],
	},
	"fr": {
		"label": "フランス語",
		"info_title": "フランス語: ルールベース + Piper 音声合成",
		"info_description": "ルールベースの音素化と Piper によるニューラル音声合成を使います。",
		"test_text": "Bonjour, comment allez-vous aujourd'hui?",
		"templates": [
			"Bonjour, comment allez-vous aujourd'hui?",
			"Bienvenue dans la démonstration de synthèse vocale.",
			"La synthèse vocale en français sonne très naturellement.",
			"Les fleurs du printemps sont très belles.",
		],
	},
	"pt": {
		"label": "ポルトガル語",
		"info_title": "ポルトガル語: ルールベース + Piper 音声合成",
		"info_description": "ルールベースの音素化と Piper によるニューラル音声合成を使います。",
		"test_text": "Olá, como você está hoje?",
		"templates": [
			"Olá, como você está hoje?",
			"Bem-vindo à demonstração de síntese de voz.",
			"A síntese de voz em português soa muito natural.",
			"As flores da primavera são muito bonitas.",
		],
	},
}

@onready var tts: PiperTTS = $PiperTTS
@onready var audio_player: AudioStreamPlayer = $AudioStreamPlayer
@onready var language_picker: OptionButton = $VBoxContainer/LanguagePicker
@onready var language_info_label: Label = $VBoxContainer/LanguageInfoLabel
@onready var template_picker: OptionButton = $VBoxContainer/TemplatePicker
@onready var text_input: LineEdit = $VBoxContainer/TextInput
@onready var synthesize_btn: Button = $VBoxContainer/HBoxContainer/SynthesizeButton
@onready var async_btn: Button = $VBoxContainer/HBoxContainer/AsyncButton
@onready var stop_btn: Button = $VBoxContainer/HBoxContainer/StopButton
@onready var status_label: Label = $VBoxContainer/StatusLabel
var _syncing_ui := false
var _needs_initialize := true
var _standalone_smoke_running := false

func _ready() -> void:
	_populate_language_picker()
	language_picker.item_selected.connect(_on_language_selected)
	template_picker.item_selected.connect(_on_template_selected)
	text_input.text_changed.connect(_on_text_changed)
	synthesize_btn.pressed.connect(_on_synthesize_pressed)
	async_btn.pressed.connect(_on_async_pressed)
	stop_btn.pressed.connect(_on_stop_pressed)
	tts.initialized.connect(_on_tts_initialized)
	tts.synthesis_completed.connect(_on_synthesis_completed)
	tts.synthesis_failed.connect(_on_synthesis_failed)
	_select_language(DEFAULT_LANGUAGE_CODE, true)
	if _standalone_smoke_enabled():
		call_deferred("_run_standalone_smoke")

func _get_language_config(language_code: String) -> Dictionary:
	var config: Variant = LANGUAGE_CONFIGS.get(language_code, LANGUAGE_CONFIGS[DEFAULT_LANGUAGE_CODE])
	if typeof(config) == TYPE_DICTIONARY:
		return config
	return {}

func _resource_dir_exists(path: String) -> bool:
	if path.is_empty():
		return false
	var resource_dir := DirAccess.open(path)
	if resource_dir != null:
		return true
	var absolute_path := ProjectSettings.globalize_path(path)
	return absolute_path != path and DirAccess.dir_exists_absolute(absolute_path)

func _resource_file_exists(path: String) -> bool:
	if path.is_empty():
		return false
	if FileAccess.file_exists(path):
		return true
	var absolute_path := ProjectSettings.globalize_path(path)
	return absolute_path != path and FileAccess.file_exists(absolute_path)

func _resolve_css10_assets() -> Dictionary:
	for candidate_variant in CSS10_ASSET_CANDIDATES:
		if typeof(candidate_variant) != TYPE_DICTIONARY:
			continue
		var candidate: Dictionary = candidate_variant
		var model_path := String(candidate.get("model_path", ""))
		var config_path := String(candidate.get("config_path", ""))
		if _resource_file_exists(model_path) and _resource_file_exists(config_path):
			return {
				"model_path": model_path,
				"config_path": config_path,
			}
	return {}

func _resolve_openjtalk_dictionary() -> Dictionary:
	for candidate_variant in OPENJTALK_DICT_CANDIDATES:
		var candidate := String(candidate_variant)
		if _has_compiled_openjtalk_dictionary(candidate):
			return {
				"path": candidate,
				"ready": true,
				"incomplete": false,
			}
		if _resource_dir_exists(candidate):
			return {
				"path": candidate,
				"ready": false,
				"incomplete": true,
			}
	return {
		"path": "",
		"ready": false,
		"incomplete": false,
	}

func _standalone_smoke_enabled() -> bool:
	return OS.get_environment("PIPER_STANDALONE_SMOKE").strip_edges() == "1"

func _populate_language_picker() -> void:
	language_picker.clear()
	for language_code: String in LANGUAGE_ORDER:
		var config := _get_language_config(language_code)
		language_picker.add_item(String(config.get("label", language_code.to_upper())))
		language_picker.set_item_metadata(language_picker.get_item_count() - 1, language_code)

func _get_language_code_at(index: int) -> String:
	if index < 0 or index >= language_picker.get_item_count():
		return DEFAULT_LANGUAGE_CODE
	return String(language_picker.get_item_metadata(index))

func _get_selected_language_code() -> String:
	if language_picker.selected < 0:
		return DEFAULT_LANGUAGE_CODE
	return _get_language_code_at(language_picker.selected)

func _find_language_index(language_code: String) -> int:
	for index in range(language_picker.get_item_count()):
		if _get_language_code_at(index) == language_code:
			return index
	return 0

func _select_language(language_code: String, apply_default_text: bool) -> void:
	var language_index := _find_language_index(language_code)
	language_picker.select(language_index)
	_apply_language_selection(language_index, apply_default_text)

func _update_language_info(language_code: String) -> void:
	var config := _get_language_config(language_code)
	language_info_label.text = "%s\n%s" % [
		String(config.get("info_title", "")),
		String(config.get("info_description", "")),
	]

func _update_template_picker(language_code: String) -> void:
	template_picker.clear()
	template_picker.add_item(FREE_INPUT_LABEL)
	template_picker.set_item_metadata(0, "")

	var config := _get_language_config(language_code)
	var templates_variant: Variant = config.get("templates", [])
	if typeof(templates_variant) != TYPE_ARRAY:
		return

	for template_value in templates_variant:
		var template_text := String(template_value)
		template_picker.add_item(template_text)
		template_picker.set_item_metadata(template_picker.get_item_count() - 1, template_text)

func _sync_template_selection_with_text() -> void:
	var current_text := text_input.text.strip_edges()
	for index in range(1, template_picker.get_item_count()):
		if String(template_picker.get_item_metadata(index)) == current_text:
			template_picker.select(index)
			return
	template_picker.select(0)

func _apply_language_selection(index: int, apply_default_text: bool) -> void:
	var language_code := _get_language_code_at(index)
	var config := _get_language_config(language_code)

	_syncing_ui = true
	_update_language_info(language_code)
	_update_template_picker(language_code)
	if apply_default_text:
		text_input.text = String(config.get("test_text", ""))
		template_picker.select(0)
	else:
		_sync_template_selection_with_text()
	_syncing_ui = false

	_configure_demo_assets()

func _has_compiled_openjtalk_dictionary(path: String) -> bool:
	if not _resource_dir_exists(path):
		return false

	var absolute_path := ProjectSettings.globalize_path(path)
	for required_file in ["sys.dic", "unk.dic", "matrix.bin", "char.bin"]:
		var resource_file := path.path_join(required_file)
		var absolute_file := absolute_path.path_join(required_file)
		if not _resource_file_exists(resource_file) and not FileAccess.file_exists(absolute_file):
			return false

	return true

func _set_demo_assets(model_path: String, config_path: String, dictionary_path: String, language_code: String, status_text: String) -> void:
	var selection_changed := tts.model_path != model_path or tts.config_path != config_path or tts.dictionary_path != dictionary_path or tts.language_code != language_code

	tts.model_path = model_path
	tts.config_path = config_path
	tts.dictionary_path = dictionary_path
	tts.language_code = language_code

	if selection_changed:
		tts.stop()
		audio_player.stop()
		synthesize_btn.disabled = true
		async_btn.disabled = true
		stop_btn.disabled = true
		_needs_initialize = not model_path.is_empty()
	elif model_path.is_empty():
		_needs_initialize = false

	status_label.text = status_text

func _auto_initialize_current_assets() -> void:
	if tts.model_path.is_empty():
		return

	if not _needs_initialize and tts.is_ready():
		return

	status_label.text = "状態: 自動セットアップ中..."
	synthesize_btn.disabled = true
	async_btn.disabled = true
	stop_btn.disabled = true

	var err := tts.initialize()
	if err != OK and not tts.is_ready():
		_needs_initialize = true
		status_label.text = "状態: 自動セットアップに失敗しました（error: %d）" % err

func _ensure_tts_ready() -> bool:
	_configure_demo_assets()
	if tts.model_path.is_empty():
		status_label.text = "状態: 利用できるモデルがありません。先にダウンロードしてください。"
		return false

	_auto_initialize_current_assets()
	return tts.is_ready()

func _configure_demo_assets() -> void:
	var selected_language_code := _get_selected_language_code()
	var selected_language_config := _get_language_config(selected_language_code)
	var selected_language_label := String(selected_language_config.get("label", selected_language_code.to_upper()))
	var css10_assets := _resolve_css10_assets()
	var css10_model_path := String(css10_assets.get("model_path", ""))
	var css10_config_path := String(css10_assets.get("config_path", ""))
	var css10_ready := not css10_model_path.is_empty() and not css10_config_path.is_empty()
	var openjtalk_state := _resolve_openjtalk_dictionary()
	var openjtalk_ready := bool(openjtalk_state.get("ready", false))
	var openjtalk_incomplete := bool(openjtalk_state.get("incomplete", false))
	var openjtalk_path := String(openjtalk_state.get("path", ""))

	if not css10_ready:
		_set_demo_assets(
			"",
			"",
			"",
			"",
			"状態: CSS10 6 言語デフォルトモデルがありません。Piper Plus の Download Models... から取得してください。"
		)
		return

	if selected_language_code == "ja" and not openjtalk_ready:
		var missing_dictionary_status := "状態: 日本語には compiled naist-jdic 辞書が必要です。Piper Plus の Download Models... から取得してください。"
		if openjtalk_incomplete:
			missing_dictionary_status = "状態: compiled naist-jdic 辞書が不完全です。Piper Plus の Download Models... から取得してください。"
		_set_demo_assets("", "", "", "", missing_dictionary_status)
		return

	var dictionary_path := ""
	if openjtalk_ready:
		dictionary_path = openjtalk_path

	_set_demo_assets(
		css10_model_path,
		css10_config_path,
		dictionary_path,
		selected_language_code,
		"状態: 準備中（CSS10 6 言語デフォルトモデル: %s）" % selected_language_label
	)
	_auto_initialize_current_assets()

func _on_language_selected(index: int) -> void:
	_apply_language_selection(index, true)

func _on_template_selected(index: int) -> void:
	if _syncing_ui:
		return

	var template_text := String(template_picker.get_item_metadata(index))
	if template_text.is_empty():
		return

	_syncing_ui = true
	text_input.text = template_text
	_syncing_ui = false

func _on_text_changed(_new_text: String) -> void:
	if _syncing_ui:
		return
	_sync_template_selection_with_text()

func _on_tts_initialized(success: bool) -> void:
	if success:
		_needs_initialize = false
		status_label.text = "状態: 準備完了"
		synthesize_btn.disabled = _standalone_smoke_running
		async_btn.disabled = _standalone_smoke_running
	else:
		_needs_initialize = not tts.model_path.is_empty()
		status_label.text = "状態: 自動セットアップに失敗しました"

func _run_standalone_smoke() -> void:
	_standalone_smoke_running = true
	var failures: Array[String] = []
	var passes: Array[String] = []

	for language_code: String in LANGUAGE_ORDER:
		_select_language(language_code, true)
		if not _ensure_tts_ready():
			failures.append("%s initialize failed: %s" % [language_code, status_label.text])
			continue

		var text := String(_get_language_config(language_code).get("test_text", ""))
		var audio: AudioStreamWAV = tts.synthesize_request({
			"text": text,
			"language_code": language_code,
		})
		var audio_bytes := 0
		if audio != null:
			audio_bytes = audio.data.size()
		if audio == null or audio_bytes <= 0:
			var last_error: Dictionary = tts.get_last_error()
			failures.append("%s synthesize failed: %s" % [
				language_code,
				String(last_error.get("message", status_label.text)),
			])
			continue

		passes.append("%s:%d" % [language_code, audio_bytes])
		tts.stop()

	if failures.is_empty():
		var summary := ""
		for index in range(passes.size()):
			if index > 0:
				summary += ","
			summary += passes[index]
		print("STANDALONE_SMOKE_PASS %s" % summary)
		get_tree().quit(0)
		return

	for failure in failures:
		push_error("STANDALONE_SMOKE_FAIL %s" % failure)
	get_tree().quit(1)

# Synchronous synthesis (blocks the main thread)
func _on_synthesize_pressed() -> void:
	var text = text_input.text
	if text.is_empty():
		status_label.text = "状態: テキストを入力してください"
		return
	if not _ensure_tts_ready():
		return

	status_label.text = "状態: 音声合成中（同期）..."
	synthesize_btn.disabled = true
	async_btn.disabled = true

	var audio = tts.synthesize(text)
	if audio:
		audio_player.stream = audio
		audio_player.play()
		status_label.text = "状態: 音声を再生中"
	else:
		status_label.text = "状態: 音声合成に失敗しました"

	synthesize_btn.disabled = false
	async_btn.disabled = false

# Asynchronous synthesis (non-blocking)
func _on_async_pressed() -> void:
	var text = text_input.text
	if text.is_empty():
		status_label.text = "状態: テキストを入力してください"
		return
	if not _ensure_tts_ready():
		return

	status_label.text = "状態: 音声合成中（非同期）..."
	synthesize_btn.disabled = true
	async_btn.disabled = true
	stop_btn.disabled = false

	var err = tts.synthesize_async(text)
	if err != OK:
		status_label.text = "状態: 非同期音声合成の開始に失敗しました（error: %d）" % err
		synthesize_btn.disabled = false
		async_btn.disabled = false
		stop_btn.disabled = true

func _on_stop_pressed() -> void:
	tts.stop()
	status_label.text = "状態: 停止しました"
	synthesize_btn.disabled = false
	async_btn.disabled = false
	stop_btn.disabled = true

func _on_synthesis_completed(audio: AudioStreamWAV) -> void:
	audio_player.stream = audio
	audio_player.play()
	status_label.text = "状態: 音声を再生中（非同期）"
	synthesize_btn.disabled = false
	async_btn.disabled = false
	stop_btn.disabled = true

func _on_synthesis_failed(error: String) -> void:
	status_label.text = "状態: 音声合成に失敗しました - %s" % error
	synthesize_btn.disabled = false
	async_btn.disabled = false
	stop_btn.disabled = true
