extends Control

@onready var play_button: Button = $PlayButton
@onready var status_label: Label = $StatusLabel
@onready var audio_player: AudioStreamPlayer = $AudioPlayer

func _ready() -> void:
	status_label.text = "idle"
	play_button.pressed.connect(_on_play_pressed)
	audio_player.stream = _build_test_tone()

func _on_play_pressed() -> void:
	status_label.text = "played"
	audio_player.play()

func _build_test_tone() -> AudioStreamWAV:
	var sample_rate := 22050
	var duration_sec := 1.0
	var frame_count := int(sample_rate * duration_sec)
	var pcm := PackedByteArray()
	pcm.resize(frame_count * 2)
	for i in range(frame_count):
		var sample := int(sin(float(i) * 440.0 * TAU / float(sample_rate)) * 20000.0)
		var packed := sample & 0xFFFF
		pcm[i * 2] = packed & 0xFF
		pcm[i * 2 + 1] = (packed >> 8) & 0xFF
	var stream := AudioStreamWAV.new()
	stream.format = AudioStreamWAV.FORMAT_16_BITS
	stream.mix_rate = sample_rate
	stream.stereo = false
	stream.data = pcm
	return stream
