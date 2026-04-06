extends Control

const MODEL_PATH := "res://addons/piper_plus/models/css10/css10-ja-6lang-fp16.onnx"
const CONFIG_PATH := "res://addons/piper_plus/models/css10/config.json"
const DICT_PATH := "res://addons/piper_plus/dictionaries/open_jtalk_dic_utf_8-1.11"
const TEXT_TO_SAY := "hello,are you from? hello,are you from? hello,are you from? hello,are you from?"

@onready var tts: PiperTTS = $PiperTTS
@onready var audio_player: AudioStreamPlayer = $AudioStreamPlayer
@onready var status_label: Label = $StatusLabel

func _ready() -> void:
	call_deferred("_begin")

func _begin() -> void:
	tts.model_path = MODEL_PATH
	tts.config_path = CONFIG_PATH
	tts.dictionary_path = DICT_PATH
	tts.language_code = "en"
	var init_err := tts.initialize()
	if init_err != OK:
		status_label.text = "init_error:%d" % init_err
		return

	var forced := tts.inspect_text(TEXT_TO_SAY)
	var forced_code := String(forced.get("resolved_language_code", ""))
	var forced_id := int(forced.get("resolved_language_id", -1))
	var forced_audio := tts.synthesize(TEXT_TO_SAY)
	if forced_audio == null:
		status_label.text = "forced_synth_failed:%s:%d" % [forced_code, forced_id]
		return

	tts.language_code = ""
	var auto := tts.inspect_text(TEXT_TO_SAY)
	var auto_code := String(auto.get("resolved_language_code", ""))
	var auto_id := int(auto.get("resolved_language_id", -1))

	audio_player.stream = forced_audio
	audio_player.play()
	status_label.text = "playing:forced=%s:%d:auto=%s:%d" % [forced_code, forced_id, auto_code, auto_id]
