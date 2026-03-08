extends Control

@onready var tts: PiperTTS = $PiperTTS
@onready var audio_player: AudioStreamPlayer = $AudioStreamPlayer
@onready var text_input: LineEdit = $VBoxContainer/TextInput
@onready var synthesize_btn: Button = $VBoxContainer/HBoxContainer/SynthesizeButton
@onready var init_btn: Button = $VBoxContainer/HBoxContainer/InitButton
@onready var status_label: Label = $VBoxContainer/StatusLabel

func _ready():
	init_btn.pressed.connect(_on_init_pressed)
	synthesize_btn.pressed.connect(_on_synthesize_pressed)
	tts.initialized.connect(_on_tts_initialized)

func _on_init_pressed():
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
	else:
		status_label.text = "Status: Initialization failed"
		init_btn.disabled = false

func _on_synthesize_pressed():
	var text = text_input.text
	if text.is_empty():
		status_label.text = "Status: Please enter text"
		return

	status_label.text = "Status: Synthesizing..."
	synthesize_btn.disabled = true

	var audio = tts.synthesize(text)
	if audio:
		audio_player.stream = audio
		audio_player.play()
		status_label.text = "Status: Playing audio"
	else:
		status_label.text = "Status: Synthesis failed"

	synthesize_btn.disabled = false
