class_name TestBase
extends RefCounted

var passed: int = 0
var failed: int = 0
var skipped: int = 0
var failures: Array[String] = []
var skips: Array[String] = []


func assert_true(condition: bool, message: String) -> void:
    if not condition:
        push_failure(message)


func assert_equal(actual, expected, message: String) -> void:
    if actual != expected:
        push_failure("%s (actual=%s expected=%s)" % [message, var_to_str(actual), var_to_str(expected)])


func assert_not_null(value, message: String) -> void:
    if value == null:
        push_failure(message)


func push_failure(message: String) -> void:
    failed += 1
    failures.append(message)


func push_skip(message: String) -> void:
    skipped += 1
    skips.append(message)
    print("  SKIP: %s" % message)


func mark_pass() -> void:
    passed += 1
