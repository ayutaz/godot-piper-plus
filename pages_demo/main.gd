extends Control

const MODEL_KEY := "multilingual-test-medium"
const MODEL_PATH := "res://piper_plus_assets/models/multilingual-test-medium/multilingual-test-medium.onnx"
const CONFIG_PATH := "res://piper_plus_assets/models/multilingual-test-medium/multilingual-test-medium.onnx.json"
const DEFAULT_TEXT := "hello from godot"
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

var _startup_probe_passed := false

func _ready() -> void:
	_build_ui()
	_emit_status("boot")

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
	_tts_set("language_code", "en")
	_tts_connect("initialized", _on_tts_initialized)

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
	description_label.text = "English-only, CPU-only, no-threads Web demo using the smoke-tested multilingual-test-medium bundle."
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

func _update_contract_label() -> void:
	var lines := PackedStringArray([
		"Contract: EP_CPU / no-threads / PWA export / startup self-test on load.",
		"Model: %s" % MODEL_KEY,
		"Route: language_code=en on a bundled multilingual model; no runtime downloads.",
		"Not included: Japanese dictionary bootstrap, public multilingual UI, runtime downloads.",
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

func _tts_call_audio(method_name: StringName, arg0: Variant = null) -> AudioStreamWAV:
	if tts == null:
		return null

	var result: Variant
	if arg0 == null:
		result = tts.call(method_name)
	else:
		result = tts.call(method_name, arg0)
	return result as AudioStreamWAV

func _tts_last_error() -> Dictionary:
	if tts != null and tts.has_method("get_last_error"):
		return tts.call("get_last_error") as Dictionary
	return {}

func _on_tts_initialized(success: bool) -> void:
	if not success:
		_set_status("Initialization failed.")
		_emit_status("fail")
		return

	_set_status("Runtime initialized. Running startup self-test...")
	_run_startup_probe()

func _run_startup_probe() -> void:
	var audio := _tts_call_audio("synthesize", DEFAULT_TEXT)
	if audio == null:
		var last_error: Dictionary = _tts_last_error()
		var message := String(last_error.get("message", "Synchronous synthesis returned no audio."))
		_set_status("Startup self-test failed: %s" % message)
		_emit_status("fail")
		return

	_startup_probe_passed = true
	synthesize_button.disabled = false
	_set_status("Ready.")
	_emit_status("pass")

func _on_synthesize_pressed() -> void:
	if tts == null or not _tts_call_bool("is_ready"):
		_set_status("Runtime is not ready yet.")
		return

	var text := input_field.text.strip_edges()
	if text.is_empty():
		_set_status("Enter English text before starting synthesis.")
		return

	synthesize_button.disabled = true
	_set_status("Synthesizing...")
	var audio := _tts_call_audio("synthesize", text)
	if audio == null:
		var last_error: Dictionary = _tts_last_error()
		var message := String(last_error.get("message", "Synchronous synthesis returned no audio."))
		synthesize_button.disabled = false
		_set_status("Synthesis failed: %s" % message)
		return

	audio_player.stream = audio
	audio_player.play()
	synthesize_button.disabled = false
	_set_status("Playback started.")
