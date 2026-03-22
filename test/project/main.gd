extends Node

const END_STRING := "==== TESTS FINISHED ===="
const FAILURE_STRING := "******** FAILED ********"

var _failures: Array[String] = []
var _suites: Array = []

func _ready() -> void:
    _suites = [
        load("res://test_piper_tts.gd").new(),
    ]
    await get_tree().process_frame
    await _run_all()
    get_tree().quit(_failures.size())

func _run_all() -> void:
    for suite in _suites:
        var suite_name: String = str(suite.get_suite_name())
        print("-- Running %s --" % suite_name)

        for method_name in suite.list_test_names():
            print("  RUN  %s.%s" % [suite_name, method_name])
            suite.reset_results()
            await suite.run_test(method_name)

            for message in suite.skips:
                print("  SKIP %s.%s: %s" % [suite_name, method_name, message])

            if suite.failures.is_empty():
                print("  PASS %s.%s" % [suite_name, method_name])
            else:
                for message in suite.failures:
                    var formatted := "%s.%s: %s" % [suite_name, method_name, message]
                    _failures.append(formatted)
                    print("  FAIL %s" % formatted)

    if _failures.is_empty():
        print(END_STRING)
        return

    print(FAILURE_STRING)
    for failure in _failures:
        print(failure)
    print(END_STRING)
