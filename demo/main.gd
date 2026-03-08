extends Control

@onready var tts: PiperTTS = $PiperTTS
@onready var audio_player: AudioStreamPlayer = $AudioStreamPlayer
@onready var text_input: LineEdit = $VBoxContainer/TextInput
@onready var synthesize_btn: Button = $VBoxContainer/HBoxContainer/SynthesizeButton
@onready var async_btn: Button = $VBoxContainer/HBoxContainer/AsyncButton
@onready var stop_btn: Button = $VBoxContainer/HBoxContainer/StopButton
@onready var init_btn: Button = $VBoxContainer/HBoxContainer2/InitButton
@onready var status_label: Label = $VBoxContainer/StatusLabel

func _ready():
	init_btn.pressed.connect(_on_init_pressed)
	synthesize_btn.pressed.connect(_on_synthesize_pressed)
	async_btn.pressed.connect(_on_async_pressed)
	stop_btn.pressed.connect(_on_stop_pressed)
	tts.initialized.connect(_on_tts_initialized)
	tts.synthesis_completed.connect(_on_synthesis_completed)
	tts.synthesis_failed.connect(_on_synthesis_failed)

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
