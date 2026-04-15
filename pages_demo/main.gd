extends Control

const SampleTextCatalog = preload("res://addons/piper_plus/multilingual_sample_text_catalog.gd")

const MODEL_KEY := "multilingual-test-medium"
const MODEL_PATH := "res://piper_plus_assets/models/multilingual-test-medium/multilingual-test-medium.onnx"
const CONFIG_PATH := "res://piper_plus_assets/models/multilingual-test-medium/multilingual-test-medium.onnx.json"
const OPENJTALK_DICT_PATH := "res://piper_plus_assets/dictionaries/open_jtalk_dic_utf_8-1.11"
const DEFAULT_LANGUAGE_CODE := "ja"
const STATUS_PREFIX := "PAGES_DEMO status="
const SUMMARY_PREFIX := "PAGES_DEMO summary="

var tts: Object = null
var tts_node: Node = null
var audio_player: AudioStreamPlayer = null

var title_label: Label
var description_label: Label
var catalog_label: Label
var contract_label: Label
var language_label: Label
var language_picker: OptionButton
var input_label: Label
var input_field: LineEdit
var synthesize_button: Button
var status_label: Label

var _startup_probe_passed := false
var _startup_probe_language_code := ""
var _startup_probe_text := ""
var _selected_language_code := DEFAULT_LANGUAGE_CODE
var _last_input_text := ""
var _syncing_ui := false

func _ready() -> void:
	_build_ui()
	_populate_language_picker()
	_apply_language(_resolve_initial_language_code(), true)
	_publish_state("boot", "boot", "Booting Pages demo...")

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

	_tts_set("model_path", _model_path())
	_tts_set("config_path", _config_path())
	_tts_set("dictionary_path", _dictionary_path())
	_tts_set("language_code", _selected_language_code)
	_tts_connect("initialized", _on_tts_initialized)
	_tts_connect("synthesis_completed", _on_synthesis_completed)
	_tts_connect("synthesis_failed", _on_synthesis_failed)

	_update_contract_label()
	_publish_state("boot", "initialize", "Initializing runtime...", "", _selected_language_code)
	var err := _tts_call_int("initialize")
	if err != OK and not _tts_call_bool("is_ready"):
		_publish_state("fail", "initialize", "initialize() failed with error %d" % err, "", _selected_language_code)

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
	description_label.text = "Canonical 6-language demo for ja/en/zh/es/fr/pt. Selecting a language auto-fills the per-language template text before synthesis."
	description_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	layout.add_child(description_label)

	catalog_label = Label.new()
	catalog_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	layout.add_child(catalog_label)

	contract_label = Label.new()
	contract_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	layout.add_child(contract_label)

	language_label = Label.new()
	language_label.text = "Language"
	layout.add_child(language_label)

	language_picker = OptionButton.new()
	language_picker.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	language_picker.item_selected.connect(_on_language_selected)
	layout.add_child(language_picker)

	input_label = Label.new()
	input_label.text = "Template text"
	layout.add_child(input_label)

	input_field = LineEdit.new()
	input_field.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	input_field.placeholder_text = "Select a language to load its template text."
	input_field.text_changed.connect(_on_text_changed)
	layout.add_child(input_field)

	var button_row := HBoxContainer.new()
	button_row.add_theme_constant_override("separation", 6)
	layout.add_child(button_row)

	synthesize_button = Button.new()
	synthesize_button.text = "Synthesize"
	synthesize_button.disabled = true
	synthesize_button.pressed.connect(_on_synthesize_pressed)
	button_row.add_child(synthesize_button)

	status_label = Label.new()
	status_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	layout.add_child(status_label)

func _populate_language_picker() -> void:
	language_picker.clear()
	for language_code in _supported_language_codes():
		var display_name := SampleTextCatalog.get_language_display_name(language_code)
		language_picker.add_item(display_name)
		language_picker.set_item_metadata(language_picker.get_item_count() - 1, language_code)

func _supported_language_codes() -> PackedStringArray:
	return SampleTextCatalog.list_language_codes()

func _sample_texts() -> Dictionary:
	var texts := {}
	for language_code in _supported_language_codes():
		texts[language_code] = SampleTextCatalog.get_language_template_text(language_code)
	return texts

func _asset_requirements() -> Dictionary:
	return SampleTextCatalog.get_asset_requirements()

func _model_path() -> String:
	return String(_asset_requirements().get("model_path", MODEL_PATH))

func _config_path() -> String:
	return String(_asset_requirements().get("config_path", CONFIG_PATH))

func _dictionary_path() -> String:
	return String(_asset_requirements().get("openjtalk_path", OPENJTALK_DICT_PATH))

func _placeholder_text(language_code: String) -> String:
	return SampleTextCatalog.get_language_placeholder_text(language_code)

func _resolve_initial_language_code() -> String:
	var scenario_language := _detect_web_smoke_scenario()
	if not scenario_language.is_empty():
		return SampleTextCatalog.resolve_language_code(scenario_language)
	return SampleTextCatalog.resolve_language_code(DEFAULT_LANGUAGE_CODE)

func _detect_web_smoke_scenario() -> String:
	if not OS.has_feature("web"):
		return ""

	var scenario := OS.get_environment("PIPER_WEB_SMOKE_SCENARIO").strip_edges().to_lower()
	if not scenario.is_empty():
		return scenario

	if ClassDB.class_exists("JavaScriptBridge"):
		var js_value: Variant = JavaScriptBridge.eval(
			"(globalThis.__PIPER_WEB_SMOKE_SCENARIO || '').toString()",
			true
		)
		scenario = String(js_value).strip_edges().to_lower()

	return scenario

func _get_language_code_at(index: int) -> String:
	if index < 0 or index >= language_picker.get_item_count():
		return DEFAULT_LANGUAGE_CODE
	return String(language_picker.get_item_metadata(index))

func _find_language_index(language_code: String) -> int:
	var canonical := SampleTextCatalog.resolve_language_code(language_code)
	for index in range(language_picker.get_item_count()):
		if _get_language_code_at(index) == canonical:
			return index
	return 0

func _template_for_language(language_code: String) -> String:
	return SampleTextCatalog.get_language_template_text(language_code)

func _apply_language(language_code: String, update_text: bool) -> void:
	var canonical := SampleTextCatalog.resolve_language_code(language_code)
	_selected_language_code = canonical

	var language_index := _find_language_index(canonical)
	_syncing_ui = true
	if language_picker.selected != language_index:
		language_picker.select(language_index)
	input_field.placeholder_text = _placeholder_text(canonical)
	if update_text or input_field.text.is_empty():
		input_field.text = _template_for_language(canonical)
		_last_input_text = input_field.text
	_syncing_ui = false

	if tts != null:
		_tts_set("language_code", canonical)

	_update_contract_label()

func _update_contract_label() -> void:
	var supported := _supported_language_codes()
	var sample_texts := _sample_texts()
	var lines := PackedStringArray([
		"Contract: CPU-only web public demo using the canonical 6-language template catalog.",
		"Model: %s" % MODEL_KEY,
		"Descriptor: %s" % SampleTextCatalog.get_descriptor_path(),
		"Catalog: %s" % String(SampleTextCatalog.get_catalog_name()),
		"Supported languages: %s" % ", ".join(supported),
		"Selected language: %s" % _selected_language_code,
		"Selected template: %s" % String(sample_texts.get(_selected_language_code, "")),
	])

	if tts == null:
		lines.append("Runtime contract: unavailable")
		lines.append("Japanese dictionary bootstrap: unknown")
		lines.append("Japanese dictionary path: unknown")
		lines.append("supports_japanese_text_input: unknown")
	else:
		var contract := _runtime_contract()
		var dictionary_mode := String(contract.get("openjtalk_dictionary_bootstrap_mode", "unknown"))
		var dictionary_path := String(contract.get("resolved_dictionary_path", "unknown"))
		var supports_japanese := bool(contract.get("supports_japanese_text_input", false))
		lines.append("Runtime contract: available")
		lines.append("Japanese dictionary bootstrap: %s" % dictionary_mode)
		lines.append("Japanese dictionary path: %s" % dictionary_path)
		lines.append("supports_japanese_text_input: %s" % ("true" if supports_japanese else "false"))

	catalog_label.text = "Catalog sample texts loaded: %s" % String(SampleTextCatalog.get_catalog_name())
	contract_label.text = "\n".join(lines)

func _runtime_contract() -> Dictionary:
	if tts != null and tts.has_method("get_runtime_contract"):
		return tts.call("get_runtime_contract") as Dictionary
	return {}

func _last_synthesis_result() -> Dictionary:
	if tts != null and tts.has_method("get_last_synthesis_result"):
		return tts.call("get_last_synthesis_result") as Dictionary
	return {}

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

func _synthesize_for_language(text: String, language_code: String) -> AudioStreamWAV:
	var request := {
		"text": text,
		"language_code": language_code,
	}

	if tts != null and tts.has_method("synthesize_request"):
		return tts.call("synthesize_request", request) as AudioStreamWAV

	_tts_set("language_code", language_code)
	return _tts_call_audio("synthesize", text)

func _publish_state(
	status: String,
	action: String,
	message: String,
	text: String = "",
	language_code: String = ""
) -> void:
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
	var runtime_contract := _runtime_contract()
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
		"supports_japanese_text_input": bool(runtime_contract.get("supports_japanese_text_input", false)),
		"dictionary_bootstrap_mode": String(runtime_contract.get("openjtalk_dictionary_bootstrap_mode", "")),
		"resolved_dictionary_path": String(runtime_contract.get("resolved_dictionary_path", "")),
		"supported_language_codes": _supported_language_codes(),
		"sample_texts": _sample_texts(),
	}

	if not language_code.is_empty():
		summary["selected_language_code"] = language_code

	_emit_summary(summary)
	_emit_status(status)

func _emit_status(status: String) -> void:
	print("%s%s" % [STATUS_PREFIX, status])

func _emit_summary(summary: Dictionary) -> void:
	print("%s%s" % [SUMMARY_PREFIX, JSON.stringify(summary)])

func _tts_last_error() -> Dictionary:
	if tts != null and tts.has_method("get_last_error"):
		return tts.call("get_last_error") as Dictionary
	return {}

func _on_tts_initialized(success: bool) -> void:
	if not success:
		_publish_state("fail", "initialize", "Initialization failed.")
		return

	_run_startup_probe()

func _run_startup_probe() -> void:
	_startup_probe_language_code = _selected_language_code
	_startup_probe_text = _template_for_language(_startup_probe_language_code)
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
	_publish_state(
		"pass",
		"startup_probe",
		"Ready. Startup self-test passed for %s." % _startup_probe_language_code,
		_startup_probe_text,
		_startup_probe_language_code
	)

func _on_language_selected(index: int) -> void:
	if _syncing_ui:
		return
	var language_code := _get_language_code_at(index)
	_apply_language(language_code, true)
	if _startup_probe_passed:
		_publish_state(
			"pass",
			"language_change",
			"Language switched to %s." % language_code,
			input_field.text,
			language_code
		)

func _on_text_changed(new_text: String) -> void:
	if _syncing_ui:
		return
	_last_input_text = new_text

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
	audio_player.stop()
	audio_player.play()
	synthesize_button.disabled = false
	_publish_state(
		"pass",
		"manual_synthesize",
		"Playback started.",
		text,
		_selected_language_code
	)

func _on_synthesis_completed(audio: AudioStreamWAV) -> void:
	if audio_player != null:
		audio_player.stream = audio
		audio_player.stop()
		audio_player.play()

func _on_synthesis_failed(error: String) -> void:
	_publish_state("fail", "synthesis", error, input_field.text, _selected_language_code)
	synthesize_button.disabled = false
