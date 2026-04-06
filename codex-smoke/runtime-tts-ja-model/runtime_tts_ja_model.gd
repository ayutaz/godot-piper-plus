extends Control

const MODEL_PATH := "res://addons/piper_plus/models/css10/css10-ja-6lang-fp16.onnx"
const CONFIG_PATH := "res://addons/piper_plus/models/css10/config.json"
const DICT_PATH := "res://addons/piper_plus/dictionaries/open_jtalk_dic_utf_8-1.11"
const TEXT_TO_SAY := "こんにちは、世界。こんにちは、世界。"

@onready var tts: PiperTTS = $PiperTTS
@onready var audio_player: AudioStreamPlayer = $AudioStreamPlayer
@onready var status_label: Label = $StatusLabel

func _ready() -> void:
	tts.initialized.connect(_on_tts_initialized)
	tts.synthesis_failed.connect(_on_synthesis_failed)
	call_deferred("_begin")

func _begin() -> void:
	status_label.text = "dict_exists:%s" % [DirAccess.dir_exists_absolute(ProjectSettings.globalize_path(DICT_PATH))]
	tts.model_path = MODEL_PATH
	tts.config_path = CONFIG_PATH
	tts.dictionary_path = DICT_PATH
	tts.language_code = "ja"
	var err := tts.initialize()
	if err != OK:
		status_label.text = "init_error:%d" % err

func _on_tts_initialized(success: bool) -> void:
	if not success:
		status_label.text = "init_failed"
		return
	var inspected := tts.inspect_text(TEXT_TO_SAY)
	var missing_count := 0
	var sentence_count := 0
	if not inspected.is_empty():
		var missing = inspected.get("missing_phonemes", {})
		if missing is Dictionary:
			missing_count = missing.size()
		var phoneme_sentences = inspected.get("phoneme_sentences", [])
		if phoneme_sentences is Array:
			sentence_count = phoneme_sentences.size()
	var audio := tts.synthesize(TEXT_TO_SAY)
	if audio == null:
		status_label.text = "synth_failed:%d:%d" % [sentence_count, missing_count]
		return
	audio_player.stream = audio
	audio_player.play()
	status_label.text = "playing:%d:%d" % [sentence_count, missing_count]

func _on_synthesis_failed(error: String) -> void:
	status_label.text = "synth_failed:%s" % error
