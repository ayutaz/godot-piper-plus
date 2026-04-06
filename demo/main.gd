extends Control

const DOWNLOADED_JA_MODEL_PATH := "res://addons/piper_plus/models/ja_JP-test-medium/ja_JP-test-medium.onnx"
const DOWNLOADED_JA_CONFIG_PATH := "res://addons/piper_plus/models/ja_JP-test-medium/ja_JP-test-medium.onnx.json"
const DOWNLOADED_JA_DICT_PATH := "res://addons/piper_plus/dictionaries/open_jtalk_dic_utf_8-1.11"
const BUNDLED_TEST_MODEL_PATH := "res://test/models/multilingual-test-medium.onnx"
const BUNDLED_TEST_CONFIG_PATH := "res://test/models/multilingual-test-medium.onnx.json"
const DEFAULT_JA_TEXT := "こんにちは、世界"
const DEFAULT_EN_TEXT := "Hello from Piper Plus."

@onready var tts: PiperTTS = $PiperTTS
@onready var audio_player: AudioStreamPlayer = $AudioStreamPlayer
@onready var text_input: LineEdit = $VBoxContainer/TextInput
@onready var synthesize_btn: Button = $VBoxContainer/HBoxContainer/SynthesizeButton
@onready var async_btn: Button = $VBoxContainer/HBoxContainer/AsyncButton
@onready var stop_btn: Button = $VBoxContainer/HBoxContainer/StopButton
@onready var init_btn: Button = $VBoxContainer/HBoxContainer2/InitButton
@onready var status_label: Label = $VBoxContainer/StatusLabel

func _ready():
	_configure_demo_assets(true)
	text_input.text_changed.connect(_on_text_changed)
	init_btn.pressed.connect(_on_init_pressed)
	synthesize_btn.pressed.connect(_on_synthesize_pressed)
	async_btn.pressed.connect(_on_async_pressed)
	stop_btn.pressed.connect(_on_stop_pressed)
	tts.initialized.connect(_on_tts_initialized)
	tts.synthesis_completed.connect(_on_synthesis_completed)
	tts.synthesis_failed.connect(_on_synthesis_failed)

func _resource_dir_exists(path: String) -> bool:
	return DirAccess.dir_exists_absolute(ProjectSettings.globalize_path(path))

func _contains_japanese(text: String) -> bool:
	for index in range(text.length()):
		var codepoint := text.unicode_at(index)
		if (codepoint >= 0x3040 and codepoint <= 0x30ff) or (codepoint >= 0x3400 and codepoint <= 0x4dbf) or (codepoint >= 0x4e00 and codepoint <= 0x9fff):
			return true
	return false

func _contains_latin(text: String) -> bool:
	for index in range(text.length()):
		var codepoint := text.unicode_at(index)
		if (codepoint >= 0x41 and codepoint <= 0x5a) or (codepoint >= 0x61 and codepoint <= 0x7a):
			return true
	return false

func _has_compiled_openjtalk_dictionary(path: String) -> bool:
	if not _resource_dir_exists(path):
		return false

	var absolute_path := ProjectSettings.globalize_path(path)
	for required_file in ["sys.dic", "unk.dic", "matrix.bin", "char.bin"]:
		if not FileAccess.file_exists(absolute_path.path_join(required_file)):
			return false

	return true

func _set_demo_assets(model_path: String, config_path: String, dictionary_path: String, language_code: String, status_text: String, default_text: String, apply_default_text: bool) -> void:
	var selection_changed := tts.model_path != model_path or tts.config_path != config_path or tts.dictionary_path != dictionary_path or tts.language_code != language_code

	tts.model_path = model_path
	tts.config_path = config_path
	tts.dictionary_path = dictionary_path
	tts.language_code = language_code

	if apply_default_text and (text_input.text.is_empty() or text_input.text == DEFAULT_JA_TEXT or text_input.text == DEFAULT_EN_TEXT):
		text_input.text = default_text

	if selection_changed:
		audio_player.stop()
		synthesize_btn.disabled = true
		async_btn.disabled = true
		stop_btn.disabled = true
		init_btn.disabled = false

	status_label.text = status_text

func _configure_demo_assets(apply_default_text: bool = false) -> void:
	var downloaded_ja_model_ready := FileAccess.file_exists(DOWNLOADED_JA_MODEL_PATH) and FileAccess.file_exists(DOWNLOADED_JA_CONFIG_PATH)
	var downloaded_ja_dict_ready := _has_compiled_openjtalk_dictionary(DOWNLOADED_JA_DICT_PATH)
	var downloaded_ja_dict_incomplete := _resource_dir_exists(DOWNLOADED_JA_DICT_PATH) and not downloaded_ja_dict_ready
	var bundled_multilingual_ready := FileAccess.file_exists(BUNDLED_TEST_MODEL_PATH) and FileAccess.file_exists(BUNDLED_TEST_CONFIG_PATH)
	var normalized_text := text_input.text.strip_edges()
	var prefers_downloaded_ja := _contains_japanese(normalized_text) and not _contains_latin(normalized_text)

	if downloaded_ja_model_ready and downloaded_ja_dict_ready and prefers_downloaded_ja:
		_set_demo_assets(
			DOWNLOADED_JA_MODEL_PATH,
			DOWNLOADED_JA_CONFIG_PATH,
			DOWNLOADED_JA_DICT_PATH,
			"",
			"Status: Not initialized (downloaded Japanese model)",
			DEFAULT_JA_TEXT,
			apply_default_text
		)
		return

	if bundled_multilingual_ready:
		var bundled_status := "Status: Not initialized (bundled 6-language test model)"
		if downloaded_ja_model_ready and downloaded_ja_dict_incomplete:
			bundled_status = "Status: Japanese model found, but compiled naist-jdic is incomplete. Using bundled 6-language test model."
		_set_demo_assets(
			BUNDLED_TEST_MODEL_PATH,
			BUNDLED_TEST_CONFIG_PATH,
			"",
			"",
			bundled_status,
			DEFAULT_EN_TEXT,
			apply_default_text
		)
		return

	if downloaded_ja_model_ready and downloaded_ja_dict_ready:
		_set_demo_assets(
			DOWNLOADED_JA_MODEL_PATH,
			DOWNLOADED_JA_CONFIG_PATH,
			DOWNLOADED_JA_DICT_PATH,
			"",
			"Status: English and mixed text require the bundled 6-language test model. Falling back to downloaded Japanese model.",
			DEFAULT_JA_TEXT,
			apply_default_text
		)
		return

	var empty_status := "Status: Download a model from Piper Plus: Download Models..."
	if downloaded_ja_model_ready and downloaded_ja_dict_incomplete:
		empty_status = "Status: Download the compiled naist-jdic dictionary from Piper Plus: Download Models..."
	_set_demo_assets("", "", "", "", empty_status, DEFAULT_EN_TEXT, apply_default_text)

func _on_text_changed(_new_text: String) -> void:
	_configure_demo_assets(false)

func _on_init_pressed():
	_configure_demo_assets(false)
	if tts.model_path.is_empty():
		status_label.text = "Status: No model available. Download one first."
		return

	status_label.text = "Status: Initializing..."
	init_btn.disabled = true
	var err = tts.initialize()
	if err != OK:
		status_label.text = "Status: Failed to initialize (error: %d)" % err
		init_btn.disabled = false

func _on_tts_initialized(success: bool):
	if success:
		status_label.text = "Status: Ready"
		synthesize_btn.disabled = false
		async_btn.disabled = false
	else:
		status_label.text = "Status: Initialization failed"
		init_btn.disabled = false

# Synchronous synthesis (blocks the main thread)
func _on_synthesize_pressed():
	_configure_demo_assets(false)
	var text = text_input.text
	if text.is_empty():
		status_label.text = "Status: Please enter text"
		return

	status_label.text = "Status: Synthesizing (sync)..."
	synthesize_btn.disabled = true
	async_btn.disabled = true

	var audio = tts.synthesize(text)
	if audio:
		audio_player.stream = audio
		audio_player.play()
		status_label.text = "Status: Playing audio"
	else:
		status_label.text = "Status: Synthesis failed"

	synthesize_btn.disabled = false
	async_btn.disabled = false

# Asynchronous synthesis (non-blocking)
func _on_async_pressed():
	_configure_demo_assets(false)
	var text = text_input.text
	if text.is_empty():
		status_label.text = "Status: Please enter text"
		return

	status_label.text = "Status: Synthesizing (async)..."
	synthesize_btn.disabled = true
	async_btn.disabled = true
	stop_btn.disabled = false

	var err = tts.synthesize_async(text)
	if err != OK:
		status_label.text = "Status: Failed to start async synthesis (error: %d)" % err
		synthesize_btn.disabled = false
		async_btn.disabled = false
		stop_btn.disabled = true

func _on_stop_pressed():
	tts.stop()
	status_label.text = "Status: Stopped"
	synthesize_btn.disabled = false
	async_btn.disabled = false
	stop_btn.disabled = true

func _on_synthesis_completed(audio: AudioStreamWAV):
	audio_player.stream = audio
	audio_player.play()
	status_label.text = "Status: Playing audio (async)"
	synthesize_btn.disabled = false
	async_btn.disabled = false
	stop_btn.disabled = true

func _on_synthesis_failed(error: String):
	status_label.text = "Status: Synthesis failed - %s" % error
	synthesize_btn.disabled = false
	async_btn.disabled = false
	stop_btn.disabled = true
