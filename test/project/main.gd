extends Node

const END_STRING := "==== TESTS FINISHED ===="
const FAILURE_STRING := "******** FAILED ********"

var _failures: Array[String] = []
var _suites: Array = []

func _ready() -> void:
    _suites = [
        preload("res://test_piper_tts.gd").new(),
    ]
    await _run_all()
    get_tree().quit(_failures.size())

func _run_all() -> void:
    for suite in _suites:
        var suite_name := suite.get_suite_name()
        print("-- Running %s --" % suite_name)

        for method_info in suite.get_method_list():
            var method_name: String = method_info.name
            if not method_name.begins_with("test_"):
                continue

            print("  RUN  %s.%s" % [suite_name, method_name])
            suite.reset_results()
            var result = suite.call(method_name)
            if result is GDScriptFunctionState:
                await result

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
