extends Control

const MODEL_KEY := "multilingual-test-medium"
const MODEL_PATH := "res://piper_plus_assets/models/multilingual-test-medium/multilingual-test-medium.onnx"
const CONFIG_PATH := "res://piper_plus_assets/models/multilingual-test-medium/multilingual-test-medium.onnx.json"
const STATUS_PREFIX := "PAGES_DEMO status="
const SUMMARY_PREFIX := "PAGES_DEMO summary="
const DEFAULT_LANGUAGE_CODE := "ja"
const SAMPLE_TEXT_BY_LANGUAGE := {
	"ja": "こんにちは",
	"en": "hello from godot",
}
const LANGUAGE_OPTIONS := [
	{
		"code": "ja",
		"label": "Japanese (ja)",
		"placeholder": "日本語テキストを入力してください",
	},
	{
		"code": "en",
		"label": "English (en)",
		"placeholder": "Enter English text to synthesize",
	},
]

var tts: Object = null
var tts_node: Node = null
var audio_player: AudioStreamPlayer
var title_label: Label
var description_label: Label
var contract_label: Label
var status_label: Label
var language_picker: OptionButton
var input_field: LineEdit
var synthesize_button: Button

var _startup_probe_passed := false
var _startup_probe_language_code := ""
var _startup_probe_text := ""
var _selected_language_code := DEFAULT_LANGUAGE_CODE
var _last_action := "boot"
var _last_input_text := ""

func _ready() -> void:
	_build_ui()
	_apply_language(DEFAULT_LANGUAGE_CODE, true)
	_publish_state("boot", "boot", "Booting Pages demo...", "", DEFAULT_LANGUAGE_CODE)

	if not ClassDB.class_exists("PiperTTS"):
		_publish_state("fail", "class_lookup", "PiperTTS class is not available in this export.")
		return

	var instance: Object = ClassDB.instantiate("PiperTTS")
	if instance == null or not (instance is Node):
		_publish_state("fail", "instantiate", "PiperTTS could not be instantiated.")
		return

	tts = instance
	tts_node = instance as Node
	audio_player = AudioStreamPlayer.new()
	add_child(tts_node)
	add_child(audio_player)

	_tts_set("model_path", MODEL_PATH)
	_tts_set("config_path", CONFIG_PATH)
	_tts_set("language_code", _selected_language_code)
	_tts_connect("initialized", _on_tts_initialized)

	_update_contract_label()
	_publish_state("boot", "initialize", "Initializing runtime...", "", _selected_language_code)
	var err := _tts_call_int("initialize")
	if err != OK and not _tts_call_bool("is_ready"):
		_publish_state(
			"fail",
			"initialize",
			"initialize() failed with error %d" % err,
			"",
			_selected_language_code
		)

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
	description_label.text = "CPU-only, no-threads Web demo using multilingual-test-medium with staged naist-jdic. The public smoke validates Japanese synthesis on startup."
	description_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	layout.add_child(description_label)

	contract_label = Label.new()
	contract_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	layout.add_child(contract_label)

	var language_label := Label.new()
	language_label.text = "Language"
	layout.add_child(language_label)

	language_picker = OptionButton.new()
	for item in LANGUAGE_OPTIONS:
		language_picker.add_item(String(item.get("label", "")))
	language_picker.item_selected.connect(_on_language_selected)
	layout.add_child(language_picker)

	input_field = LineEdit.new()
	layout.add_child(input_field)

	synthesize_button = Button.new()
	synthesize_button.text = "Synthesize"
	synthesize_button.disabled = true
	synthesize_button.pressed.connect(_on_synthesize_pressed)
	layout.add_child(synthesize_button)

	status_label = Label.new()
	status_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	layout.add_child(status_label)

func _language_option(index: int) -> Dictionary:
	if index < 0 or index >= LANGUAGE_OPTIONS.size():
		return LANGUAGE_OPTIONS[0]
	return LANGUAGE_OPTIONS[index]

func _language_index_by_code(language_code: String) -> int:
	for index in range(LANGUAGE_OPTIONS.size()):
		var item: Dictionary = LANGUAGE_OPTIONS[index]
		if String(item.get("code", "")) == language_code:
			return index
	return 0

func _sample_text(language_code: String) -> String:
	return String(SAMPLE_TEXT_BY_LANGUAGE.get(language_code, SAMPLE_TEXT_BY_LANGUAGE["ja"]))

func _apply_language(language_code: String, update_text: bool) -> void:
	var index := _language_index_by_code(language_code)
	var item := _language_option(index)
	_selected_language_code = String(item.get("code", DEFAULT_LANGUAGE_CODE))

	if language_picker != null and language_picker.selected != index:
		language_picker.select(index)

	if input_field != null:
		input_field.placeholder_text = String(item.get("placeholder", ""))
		if update_text:
			input_field.text = _sample_text(_selected_language_code)

	if tts != null:
		_tts_set("language_code", _selected_language_code)

	_update_contract_label()

func _runtime_contract() -> Dictionary:
	if tts != null and tts.has_method("get_runtime_contract"):
		return tts.call("get_runtime_contract") as Dictionary
	return {}

func _last_synthesis_result() -> Dictionary:
	if tts != null and tts.has_method("get_last_synthesis_result"):
		return tts.call("get_last_synthesis_result") as Dictionary
	return {}

func _update_contract_label() -> void:
	var contract := _runtime_contract()
	var lines := PackedStringArray([
		"Contract: EP_CPU / no-threads / PWA export / startup self-test on load.",
		"Model: %s" % MODEL_KEY,
		"Demo languages: ja / en",
		"Default startup probe: %s (%s)" % [_sample_text(DEFAULT_LANGUAGE_CODE), DEFAULT_LANGUAGE_CODE],
		"Licenses: see LICENSE.txt and THIRD_PARTY_LICENSES.txt in the published site root.",
	])

	if contract.is_empty():
		lines.append("Runtime contract: unavailable")
		lines.append("Japanese dictionary bootstrap: unknown")
		lines.append("Japanese dictionary path: unknown")
		lines.append("supports_japanese_text_input: unknown")
	else:
		var dictionary_mode := String(contract.get("openjtalk_dictionary_bootstrap_mode", "unknown"))
		var dictionary_path := String(contract.get("resolved_dictionary_path", "unknown"))
		var supports_japanese := bool(contract.get("supports_japanese_text_input", false))
		lines.append("Runtime contract: available")
		lines.append("Japanese dictionary bootstrap: %s" % dictionary_mode)
		lines.append("Japanese dictionary path: %s" % dictionary_path)
		lines.append("supports_japanese_text_input: %s" % ("true" if supports_japanese else "false"))

	contract_label.text = "\n".join(lines)

func _emit_status(status: String) -> void:
	print("%s%s" % [STATUS_PREFIX, status])

func _emit_summary(summary: Dictionary) -> void:
	print("%s%s" % [SUMMARY_PREFIX, JSON.stringify(summary)])

func _publish_state(
	status: String,
	action: String,
	message: String,
	text: String = "",
	language_code: String = ""
) -> void:
	_last_action = action
	if not text.is_empty():
		_last_input_text = text
	_update_contract_label()
	if status_label != null:
		status_label.text = "Status: %s" % message

	var resolved_language_code := ""
	var last_result := _last_synthesis_result()
	if not last_result.is_empty():
		resolved_language_code = String(last_result.get("resolved_language_code", ""))

	var last_error := _tts_last_error()
	var contract := _runtime_contract()
	var summary := {
		"status": status,
		"action": action,
		"message": message,
		"model_key": MODEL_KEY,
		"selected_language_code": _selected_language_code,
		"input_text": text if not text.is_empty() else _last_input_text,
		"startup_probe_passed": _startup_probe_passed,
		"startup_probe_language_code": _startup_probe_language_code,
		"startup_probe_text": _startup_probe_text,
		"resolved_language_code": resolved_language_code,
		"last_error_code": String(last_error.get("code", "")),
		"last_error_stage": String(last_error.get("stage", "")),
		"supports_japanese_text_input": bool(contract.get("supports_japanese_text_input", false)),
		"dictionary_bootstrap_mode": String(contract.get("openjtalk_dictionary_bootstrap_mode", "")),
		"resolved_dictionary_path": String(contract.get("resolved_dictionary_path", "")),
		"supported_language_codes": PackedStringArray(["ja", "en"]),
		"sample_texts": SAMPLE_TEXT_BY_LANGUAGE.duplicate(true),
	}

	if not language_code.is_empty():
		summary["selected_language_code"] = language_code

	_emit_summary(summary)
	_emit_status(status)

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

func _synthesize_for_language(text: String, language_code: String) -> AudioStreamWAV:
	var request := {
		"text": text,
		"language_code": language_code,
	}

	if tts != null and tts.has_method("synthesize_request"):
		return tts.call("synthesize_request", request) as AudioStreamWAV

	_tts_set("language_code", language_code)
	return _tts_call_audio("synthesize", text)

func _on_tts_initialized(success: bool) -> void:
	if not success:
		_publish_state("fail", "initialize", "Initialization failed.")
		return

	_update_contract_label()
	_publish_state("boot", "startup_probe", "Runtime initialized. Running startup self-test...")
	_run_startup_probe()

func _run_startup_probe() -> void:
	_startup_probe_language_code = DEFAULT_LANGUAGE_CODE
	_startup_probe_text = _sample_text(DEFAULT_LANGUAGE_CODE)
	var audio := _synthesize_for_language(_startup_probe_text, _startup_probe_language_code)
	if audio == null:
		var last_error := _tts_last_error()
		var message := String(last_error.get("message", "Synchronous synthesis returned no audio."))
		_publish_state(
			"fail",
			"startup_probe",
			"Startup self-test failed: %s" % message,
			_startup_probe_text,
			_startup_probe_language_code
		)
		return

	_startup_probe_passed = true
	synthesize_button.disabled = false
	_apply_language(DEFAULT_LANGUAGE_CODE, true)
	_publish_state(
		"pass",
		"startup_probe",
		"Ready. Japanese startup self-test passed.",
		_startup_probe_text,
		_startup_probe_language_code
	)

func _on_language_selected(index: int) -> void:
	var language_code := String(_language_option(index).get("code", DEFAULT_LANGUAGE_CODE))
	_apply_language(language_code, true)
	if _startup_probe_passed:
		_publish_state(
			"pass",
			"language_change",
			"Language switched to %s." % language_code,
			input_field.text,
			language_code
		)

func _on_synthesize_pressed() -> void:
	if tts == null or not _tts_call_bool("is_ready"):
		_publish_state("fail", "manual_synthesize", "Runtime is not ready yet.")
		return

	var text := input_field.text.strip_edges()
	if text.is_empty():
		_publish_state(
			"fail",
			"manual_synthesize",
			"Enter text before starting synthesis.",
			"",
			_selected_language_code
		)
		return

	synthesize_button.disabled = true
	_publish_state("boot", "manual_synthesize", "Synthesizing...", text, _selected_language_code)
	var audio := _synthesize_for_language(text, _selected_language_code)
	if audio == null:
		var last_error := _tts_last_error()
		var message := String(last_error.get("message", "Synchronous synthesis returned no audio."))
		synthesize_button.disabled = false
		_publish_state(
			"fail",
			"manual_synthesize",
			"Synthesis failed: %s" % message,
			text,
			_selected_language_code
		)
		return

	audio_player.stream = audio
	audio_player.play()
	synthesize_button.disabled = false
	_publish_state(
		"pass",
		"manual_synthesize",
		"Playback started.",
		text,
		_selected_language_code
	)
