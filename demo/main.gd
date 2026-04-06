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
	_configure_demo_assets()
	init_btn.pressed.connect(_on_init_pressed)
	synthesize_btn.pressed.connect(_on_synthesize_pressed)
	async_btn.pressed.connect(_on_async_pressed)
	stop_btn.pressed.connect(_on_stop_pressed)
	tts.initialized.connect(_on_tts_initialized)
	tts.synthesis_completed.connect(_on_synthesis_completed)
	tts.synthesis_failed.connect(_on_synthesis_failed)

func _resource_dir_exists(path: String) -> bool:
	return DirAccess.dir_exists_absolute(ProjectSettings.globalize_path(path))

func _has_compiled_openjtalk_dictionary(path: String) -> bool:
	if not _resource_dir_exists(path):
		return false

	var absolute_path := ProjectSettings.globalize_path(path)
	for required_file in ["sys.dic", "unk.dic", "matrix.bin", "char.bin"]:
		if not FileAccess.file_exists(absolute_path.path_join(required_file)):
			return false

	return true

func _configure_demo_assets():
	var downloaded_ja_model_ready := FileAccess.file_exists(DOWNLOADED_JA_MODEL_PATH) and FileAccess.file_exists(DOWNLOADED_JA_CONFIG_PATH)
	var downloaded_ja_dict_ready := _has_compiled_openjtalk_dictionary(DOWNLOADED_JA_DICT_PATH)
	var downloaded_ja_dict_incomplete := _resource_dir_exists(DOWNLOADED_JA_DICT_PATH) and not downloaded_ja_dict_ready

	if downloaded_ja_model_ready and downloaded_ja_dict_ready:
		tts.model_path = DOWNLOADED_JA_MODEL_PATH
		tts.config_path = DOWNLOADED_JA_CONFIG_PATH
		tts.dictionary_path = DOWNLOADED_JA_DICT_PATH
		tts.language_code = ""
		if text_input.text.is_empty() or text_input.text == DEFAULT_EN_TEXT:
			text_input.text = DEFAULT_JA_TEXT
		status_label.text = "Status: Not initialized (downloaded Japanese model)"
		return

	if FileAccess.file_exists(BUNDLED_TEST_MODEL_PATH) and FileAccess.file_exists(BUNDLED_TEST_CONFIG_PATH):
		tts.model_path = BUNDLED_TEST_MODEL_PATH
		tts.config_path = BUNDLED_TEST_CONFIG_PATH
		tts.dictionary_path = ""
		tts.language_code = "en"
		if text_input.text.is_empty() or text_input.text == DEFAULT_JA_TEXT:
			text_input.text = DEFAULT_EN_TEXT
		if downloaded_ja_model_ready and downloaded_ja_dict_incomplete:
			status_label.text = "Status: Japanese model found, but compiled naist-jdic is incomplete. Using bundled English test model."
		else:
			status_label.text = "Status: Not initialized (bundled English test model)"
		return

	tts.model_path = ""
	tts.config_path = ""
	tts.dictionary_path = ""
	tts.language_code = ""
	if downloaded_ja_model_ready and downloaded_ja_dict_incomplete:
		status_label.text = "Status: Download the compiled naist-jdic dictionary from Piper Plus: Download Models..."
	else:
		status_label.text = "Status: Download a model from Piper Plus: Download Models..."

func _on_init_pressed():
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
