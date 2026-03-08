extends TestBase

const TEST_MODEL_PATH := "res://models/ja_JP-test-medium.onnx"
const TEST_CONFIG_PATH := "res://models/ja_JP-test-medium.onnx.json"

var _runner: Node


func _init(runner: Node) -> void:
    _runner = runner


func run_all() -> Dictionary:
    var tests := [
        "test_node_creation",
        "test_properties",
        "test_speech_rate_range",
        "test_execution_provider_enum",
        "test_initialize_without_model",
        "test_synthesize_without_init",
        "test_is_ready_default",
        "test_is_processing_default",
        "test_initialize_with_model",
        "test_synthesize_basic",
        "test_synthesize_async",
        "test_audio_stream_format",
    ]

    for test_name in tests:
        await _run_case(test_name)

    return {
        "passed": passed,
        "failed": failed,
        "skipped": skipped,
        "failures": failures,
        "skips": skips,
    }


func _run_case(test_name: String) -> void:
    print("RUN: %s" % test_name)
    var failed_before := failed
    var skipped_before := skipped
    await call(test_name)
    if failed == failed_before and skipped == skipped_before:
        mark_pass()


func _addon_available() -> bool:
    return ClassDB.class_exists("PiperTTS")


func _has_test_model() -> bool:
    return FileAccess.file_exists(TEST_MODEL_PATH) and FileAccess.file_exists(TEST_CONFIG_PATH)


func _make_tts():
    if not _addon_available():
        return null

    var tts = ClassDB.instantiate("PiperTTS")
    if tts == null:
        return null

    _runner.add_child(tts)
    return tts


func _free_tts(tts) -> void:
    if tts != null and is_instance_valid(tts):
        tts.queue_free()
        await _runner.get_tree().process_frame


func _skip_if_no_addon(test_name: String) -> bool:
    if _addon_available():
        return false
    push_skip("%s: PiperTTS class is unavailable. Build and copy the GDExtension into test/project/addons/piper_plus/bin/." % test_name)
    return true


func _skip_if_no_model(test_name: String) -> bool:
    if _has_test_model():
        return false
    push_skip("%s: test model not found at %s" % [test_name, TEST_MODEL_PATH])
    return true


func test_node_creation() -> void:
    if _skip_if_no_addon("test_node_creation"):
        return
    var tts = _make_tts()
    assert_not_null(tts, "PiperTTS should be instantiated")
    await _free_tts(tts)


func test_properties() -> void:
    if _skip_if_no_addon("test_properties"):
        return
    var tts = _make_tts()
    tts.set("model_path", "res://models/demo.onnx")
    tts.set("config_path", "res://models/demo.onnx.json")
    tts.set("dictionary_path", "res://dict")
    tts.set("speaker_id", 3)
    tts.call("set_noise_scale", 1.25)
    tts.call("set_noise_w", 0.55)

    assert_equal(tts.get("model_path"), "res://models/demo.onnx", "model_path should round-trip")
    assert_equal(tts.get("config_path"), "res://models/demo.onnx.json", "config_path should round-trip")
    assert_equal(tts.get("dictionary_path"), "res://dict", "dictionary_path should round-trip")
    assert_equal(tts.get("speaker_id"), 3, "speaker_id should round-trip")
    assert_equal(tts.call("get_noise_scale"), 1.25, "noise_scale should round-trip")
    assert_equal(tts.call("get_noise_w"), 0.55, "noise_w should round-trip")
    await _free_tts(tts)


func test_speech_rate_range() -> void:
    if _skip_if_no_addon("test_speech_rate_range"):
        return
    var tts = _make_tts()
    tts.call("set_speech_rate", 0.0)
    assert_equal(tts.call("get_speech_rate"), 0.1, "speech_rate should clamp to the lower bound")
    tts.call("set_speech_rate", 10.0)
    assert_equal(tts.call("get_speech_rate"), 5.0, "speech_rate should clamp to the upper bound")
    await _free_tts(tts)


func test_execution_provider_enum() -> void:
    if _skip_if_no_addon("test_execution_provider_enum"):
        return
    assert_equal(ClassDB.class_get_integer_constant("PiperTTS", "EP_CPU"), 0, "EP_CPU should match the bound enum value")
    assert_equal(ClassDB.class_get_integer_constant("PiperTTS", "EP_AUTO"), 4, "EP_AUTO should match the bound enum value")


func test_initialize_without_model() -> void:
    if _skip_if_no_addon("test_initialize_without_model"):
        return
    var tts = _make_tts()
    var err = tts.call("initialize")
    assert_equal(err, ERR_UNCONFIGURED, "initialize without model/config should fail with ERR_UNCONFIGURED")
    assert_true(not tts.call("is_ready"), "PiperTTS should remain not ready after failed initialize")
    await _free_tts(tts)


func test_synthesize_without_init() -> void:
    if _skip_if_no_addon("test_synthesize_without_init"):
        return
    var tts = _make_tts()
    assert_true(tts.call("synthesize", "hello") == null, "synthesize should return null before initialize")
    assert_equal(tts.call("synthesize_async", "hello"), ERR_UNCONFIGURED, "synthesize_async should reject uninitialized use")
    await _free_tts(tts)


func test_is_ready_default() -> void:
    if _skip_if_no_addon("test_is_ready_default"):
        return
    var tts = _make_tts()
    assert_true(not tts.call("is_ready"), "PiperTTS should start in a not-ready state")
    await _free_tts(tts)


func test_is_processing_default() -> void:
    if _skip_if_no_addon("test_is_processing_default"):
        return
    var tts = _make_tts()
    assert_true(not tts.call("is_processing"), "PiperTTS should not be processing by default")
    await _free_tts(tts)


func test_initialize_with_model() -> void:
    if _skip_if_no_addon("test_initialize_with_model") or _skip_if_no_model("test_initialize_with_model"):
        return
    var tts = _make_tts()
    tts.set("model_path", TEST_MODEL_PATH)
    tts.set("config_path", TEST_CONFIG_PATH)
    var err = tts.call("initialize")
    assert_equal(err, OK, "initialize should succeed when a test model is available")
    assert_true(tts.call("is_ready"), "PiperTTS should be ready after initialize")
    await _free_tts(tts)


func test_synthesize_basic() -> void:
    if _skip_if_no_addon("test_synthesize_basic") or _skip_if_no_model("test_synthesize_basic"):
        return
    var tts = _make_tts()
    tts.set("model_path", TEST_MODEL_PATH)
    tts.set("config_path", TEST_CONFIG_PATH)
    var err = tts.call("initialize")
    assert_equal(err, OK, "initialize should succeed before synthesize")
    if err == OK:
        var audio = tts.call("synthesize", "Hello from Piper Plus")
        assert_not_null(audio, "synthesize should return AudioStreamWAV when the model is available")
    await _free_tts(tts)


func test_synthesize_async() -> void:
    if _skip_if_no_addon("test_synthesize_async") or _skip_if_no_model("test_synthesize_async"):
        return
    var tts = _make_tts()
    tts.set("model_path", TEST_MODEL_PATH)
    tts.set("config_path", TEST_CONFIG_PATH)
    var err = tts.call("initialize")
    assert_equal(err, OK, "initialize should succeed before synthesize_async")
    if err != OK:
        await _free_tts(tts)
        return

    var completed := false
    tts.connect("synthesis_completed", func(_audio): completed = true, CONNECT_ONE_SHOT)
    var async_err = tts.call("synthesize_async", "Hello from Piper Plus")
    assert_equal(async_err, OK, "synthesize_async should start successfully")

    var deadline := Time.get_ticks_msec() + 15000
    while not completed and Time.get_ticks_msec() < deadline:
        await _runner.get_tree().process_frame

    assert_true(completed, "synthesis_completed should be emitted before timeout")
    await _free_tts(tts)


func test_audio_stream_format() -> void:
    if _skip_if_no_addon("test_audio_stream_format") or _skip_if_no_model("test_audio_stream_format"):
        return
    var tts = _make_tts()
    tts.set("model_path", TEST_MODEL_PATH)
    tts.set("config_path", TEST_CONFIG_PATH)
    var err = tts.call("initialize")
    assert_equal(err, OK, "initialize should succeed before audio format checks")
    if err == OK:
        var audio = tts.call("synthesize", "Hello from Piper Plus")
        assert_not_null(audio, "audio stream should be created")
        if audio != null:
            assert_equal(audio.mix_rate, 22050, "mix_rate should match the default 22050 Hz expectation")
            assert_equal(audio.format, AudioStreamWAV.FORMAT_16_BITS, "audio should be 16-bit PCM")
            assert_true(not audio.stereo, "audio should be mono")
            assert_true(audio.data.size() > 0, "audio data should not be empty")
    await _free_tts(tts)
