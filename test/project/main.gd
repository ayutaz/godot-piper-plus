extends Node

const END_STRING := "==== TESTS FINISHED ===="
const FAILURE_STRING := "******** FAILED ********"
const RESULT_PREFIX := "RESULT"
const WEB_SMOKE_PREFIX := "WEB_SMOKE"
const SUMMARY_OUTPUT_PATH := "user://web_smoke_summary.json"

signal smoke_summary_ready(summary: Dictionary)
signal smoke_finished(success: bool)

var _failures: Array[String] = []
var _suites: Array = []
var _total_count := 0
var _pass_count := 0
var _skip_count := 0
var _strict_skip_patterns: Array[String] = []
var _require_pass := false

func _env_flag(name: String) -> bool:
    var value := OS.get_environment(name).strip_edges().to_lower()
    return value in ["1", "true", "yes", "on"]

func _parse_env_list(name: String) -> Array[String]:
    var raw := OS.get_environment(name)
    if raw.is_empty():
        return []

    var normalized := raw.replace("\r\n", "\n").replace("\r", "\n")
    normalized = normalized.replace(";;", "\n").replace(";", "\n").replace(",", "\n")

    var items: Array[String] = []
    for item in normalized.split("\n", false):
        var trimmed := String(item).strip_edges()
        if not trimmed.is_empty():
            items.append(trimmed)
    return items

func _skip_is_strict(message: String) -> bool:
    for pattern in _strict_skip_patterns:
        if message.findn(pattern) != -1:
            return true
    return false

func _print_summary() -> void:
    print("%s total=%d pass=%d fail=%d skip=%d" % [
        RESULT_PREFIX,
        _total_count,
        _pass_count,
        _failures.size(),
        _skip_count,
    ])

func _print_web_smoke_status() -> void:
    if not OS.has_feature("web_smoke"):
        return
    print("%s status=%s" % [WEB_SMOKE_PREFIX, "pass" if _failures.is_empty() else "fail"])

func _build_summary() -> Dictionary:
    return {
        "total": _total_count,
        "pass": _pass_count,
        "fail": _failures.size(),
        "skip": _skip_count,
        "failures": _failures.duplicate(),
        "result_prefix": RESULT_PREFIX,
        "end_string": END_STRING,
        "failure_string": FAILURE_STRING,
    }

func _write_summary_file(summary: Dictionary) -> void:
    var file := FileAccess.open(SUMMARY_OUTPUT_PATH, FileAccess.WRITE)
    if file == null:
        push_error("Failed to write %s" % SUMMARY_OUTPUT_PATH)
        return
    file.store_string(JSON.stringify(summary, "\t"))
    file.flush()

func _ready() -> void:
    _strict_skip_patterns = _parse_env_list("PIPER_FAIL_ON_SKIP_PATTERNS")
    _require_pass = _env_flag("PIPER_REQUIRE_PASS")

    var suite_script = load("res://test_piper_tts.gd")
    if suite_script == null:
        _failures.append("Failed to load res://test_piper_tts.gd")
        print(FAILURE_STRING)
        print(_failures[0])
        _print_summary()
        print(END_STRING)
        var load_summary := _build_summary()
        _write_summary_file(load_summary)
        smoke_summary_ready.emit(load_summary)
        smoke_finished.emit(false)
        get_tree().quit(1)
        return

    var suite = suite_script.new()
    if suite == null:
        _failures.append("Failed to instantiate res://test_piper_tts.gd")
        print(FAILURE_STRING)
        print(_failures[0])
        _print_summary()
        print(END_STRING)
        var instantiate_summary := _build_summary()
        _write_summary_file(instantiate_summary)
        smoke_summary_ready.emit(instantiate_summary)
        smoke_finished.emit(false)
        get_tree().quit(1)
        return

    _suites = [suite]
    await get_tree().process_frame
    await _run_all()
    var summary := _build_summary()
    _write_summary_file(summary)
    smoke_summary_ready.emit(summary)
    smoke_finished.emit(_failures.is_empty())
    get_tree().quit(0 if _failures.is_empty() else 1)

func _run_all() -> void:
    for suite in _suites:
        var suite_name: String = str(suite.get_suite_name())
        print("-- Running %s --" % suite_name)

        for method_name in suite.list_test_names():
            _total_count += 1
            print("  RUN  %s.%s" % [suite_name, method_name])
            suite.reset_results()
            await suite.run_test(method_name)

            var strict_skip_messages: Array[String] = []
            for message in suite.skips:
                _skip_count += 1
                print("  SKIP %s.%s: %s" % [suite_name, method_name, message])
                if _skip_is_strict(message):
                    strict_skip_messages.append(message)

            for message in strict_skip_messages:
                var formatted_skip := "%s.%s: required skip condition encountered: %s" % [suite_name, method_name, message]
                _failures.append(formatted_skip)
                print("  FAIL %s" % formatted_skip)

            if suite.failures.is_empty() and suite.skips.is_empty() and strict_skip_messages.is_empty():
                _pass_count += 1
                print("  PASS %s.%s" % [suite_name, method_name])
            else:
                for message in suite.failures:
                    var formatted := "%s.%s: %s" % [suite_name, method_name, message]
                    _failures.append(formatted)
                    print("  FAIL %s" % formatted)

    if _require_pass and _pass_count == 0:
        _failures.append("No tests passed")

    _print_summary()
    _print_web_smoke_status()

    if _failures.is_empty():
        print(END_STRING)
        return

    print(FAILURE_STRING)
    for failure in _failures:
        print(failure)
    print(END_STRING)
