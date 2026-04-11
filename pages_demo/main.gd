extends Control

const MODEL_KEY := "en_US-ljspeech-medium"
const MODEL_PATH := "res://piper_plus_assets/models/en_US-ljspeech-medium/en_US-ljspeech-medium.onnx"
const CONFIG_PATH := "res://piper_plus_assets/models/en_US-ljspeech-medium/en_US-ljspeech-medium.onnx.json"
const DEFAULT_TEXT := "Hello from Piper Plus on GitHub Pages."
const STATUS_PREFIX := "PAGES_DEMO status="
const SUMMARY_PREFIX := "PAGES_DEMO summary="

var tts: Object = null
var tts_node: Node = null
var audio_player: AudioStreamPlayer
var title_label: Label
var description_label: Label
var contract_label: Label
var status_label: Label
var input_field: LineEdit
var synthesize_button: Button

var _startup_probe_running := false
var _startup_probe_passed := false
var _pending_user_request := false

func _ready() -> void:
	_build_ui()
	_emit_status("boot")

	if not _assets_exist():
		_set_status("Pages demo assets are missing. Prepare the staged addon and model bundle first.")
		_emit_status("fail")
		return

	if not ClassDB.class_exists("PiperTTS"):
		_set_status("PiperTTS class is not available in this export.")
		_emit_status("fail")
		return

	var instance: Object = ClassDB.instantiate("PiperTTS")
	if instance == null or not (instance is Node):
		_set_status("PiperTTS could not be instantiated.")
		_emit_status("fail")
		return

	tts = instance
	tts_node = instance as Node
	audio_player = AudioStreamPlayer.new()
	add_child(tts_node)
	add_child(audio_player)

	_tts_set("model_path", MODEL_PATH)
	_tts_set("config_path", CONFIG_PATH)
	_tts_connect("initialized", _on_tts_initialized)
	_tts_connect("synthesis_completed", _on_synthesis_completed)
	_tts_connect("synthesis_failed", _on_synthesis_failed)

	_update_contract_label()
	_set_status("Initializing runtime...")
	var err := _tts_call_int("initialize")
	if err != OK and not _tts_call_bool("is_ready"):
		_set_status("initialize() failed with error %d" % err)
		_emit_status("fail")

func _build_ui() -> void:
	var margin := MarginContainer.new()
	margin.set_anchors_preset(Control.PRESET_FULL_RECT)
	margin.add_theme_constant_override("margin_left", 24)
	margin.add_theme_constant_override("margin_top", 24)
	margin.add_theme_constant_override("margin_right", 24)
	margin.add_theme_constant_override("margin_bottom", 24)
	add_child(margin)

	var layout := VBoxContainer.new()
	layout.add_theme_constant_override("separation", 12)
	margin.add_child(layout)

	title_label = Label.new()
	title_label.text = "Piper Plus Pages Demo"
	title_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_LEFT
	layout.add_child(title_label)

	description_label = Label.new()
	description_label.text = "English-only, CPU-only, no-threads Web demo using en_US-ljspeech-medium."
	description_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	layout.add_child(description_label)

	contract_label = Label.new()
	contract_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	layout.add_child(contract_label)

	input_field = LineEdit.new()
	input_field.placeholder_text = "Enter English text to synthesize"
	input_field.text = DEFAULT_TEXT
	layout.add_child(input_field)

	synthesize_button = Button.new()
	synthesize_button.text = "Synthesize"
	synthesize_button.disabled = true
	synthesize_button.pressed.connect(_on_synthesize_pressed)
	layout.add_child(synthesize_button)

	status_label = Label.new()
	status_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	layout.add_child(status_label)

func _assets_exist() -> bool:
	return FileAccess.file_exists(MODEL_PATH) and FileAccess.file_exists(CONFIG_PATH)

func _update_contract_label() -> void:
	var lines := PackedStringArray([
		"Contract: EP_CPU / no-threads / PWA export / startup self-test on load.",
		"Model: %s" % MODEL_KEY,
		"Not included: Japanese dictionary bootstrap, multilingual routing, runtime downloads.",
		"Licenses: see LICENSE.txt and THIRD_PARTY_LICENSES.txt in the published site root.",
	])
	contract_label.text = "\n".join(lines)

func _set_status(message: String) -> void:
	status_label.text = "Status: %s" % message
	print("%s%s" % [SUMMARY_PREFIX, message])

func _emit_status(status: String) -> void:
	print("%s%s" % [STATUS_PREFIX, status])

func _tts_set(property_name: StringName, value: Variant) -> void:
	if tts != null:
		tts.set(property_name, value)

func _tts_connect(signal_name: StringName, callable: Callable) -> void:
	if tts != null:
		tts.connect(signal_name, callable)

func _tts_call_bool(method_name: StringName, arg0: Variant = null) -> bool:
	if tts == null:
		return false

	if arg0 == null:
		return bool(tts.call(method_name))
	return bool(tts.call(method_name, arg0))

func _tts_call_int(method_name: StringName, arg0: Variant = null) -> int:
	if tts == null:
		return ERR_UNAVAILABLE

	if arg0 == null:
		return int(tts.call(method_name))
	return int(tts.call(method_name, arg0))

func _on_tts_initialized(success: bool) -> void:
	if not success:
		_set_status("Initialization failed.")
		_emit_status("fail")
		return

	_set_status("Runtime initialized. Running startup self-test...")
	_run_startup_probe()

func _run_startup_probe() -> void:
	_startup_probe_running = true
	var err := _tts_call_int("synthesize_async", DEFAULT_TEXT)
	if err != OK:
		_startup_probe_running = false
		_set_status("Startup self-test failed to start (%d)." % err)
		_emit_status("fail")

func _on_synthesize_pressed() -> void:
	if tts == null or not _tts_call_bool("is_ready"):
		_set_status("Runtime is not ready yet.")
		return

	var text := input_field.text.strip_edges()
	if text.is_empty():
		_set_status("Enter English text before starting synthesis.")
		return

	synthesize_button.disabled = true
	_pending_user_request = true
	_set_status("Synthesizing...")
	var err := _tts_call_int("synthesize_async", text)
	if err != OK:
		_pending_user_request = false
		synthesize_button.disabled = false
		_set_status("synthesize_async() failed with error %d." % err)

func _on_synthesis_completed(audio: AudioStreamWAV) -> void:
	if _startup_probe_running:
		_startup_probe_running = false
		_startup_probe_passed = true
		synthesize_button.disabled = false
		_set_status("Ready.")
		_emit_status("pass")
		return

	audio_player.stream = audio
	audio_player.play()
	_pending_user_request = false
	synthesize_button.disabled = false
	_set_status("Playback started.")

func _on_synthesis_failed(message: String) -> void:
	if _startup_probe_running:
		_startup_probe_running = false
		_set_status("Startup self-test failed: %s" % message)
		_emit_status("fail")
		return

	_pending_user_request = false
	synthesize_button.disabled = not _startup_probe_passed
	_set_status("Synthesis failed: %s" % message)
