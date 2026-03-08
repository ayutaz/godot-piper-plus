extends Node

const END_STRING := "==== TESTS FINISHED ===="
const FAILURE_STRING := "******** FAILED ********"
const TestPiperTTS := preload("res://test_piper_tts.gd")


func _ready() -> void:
    await get_tree().process_frame
    var suite := TestPiperTTS.new(self)
    var summary := await suite.run_all()

    print("")
    print("Passed: %d  Failed: %d  Skipped: %d" % [summary.passed, summary.failed, summary.skipped])
    if summary.failed > 0:
        print(FAILURE_STRING)
        for failure in summary.failures:
            print(failure)
    print(END_STRING)
    get_tree().quit(1 if summary.failed > 0 else 0)
