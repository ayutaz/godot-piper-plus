extends RefCounted
class_name TestBase

var failures: Array[String] = []
var skips: Array[String] = []

func get_suite_name() -> String:
    return get_script().resource_path.get_file().get_basename()

func reset_results() -> void:
    failures.clear()
    skips.clear()

func assert_true(value: bool, message: String) -> void:
    if not value:
        failures.append(message)

func assert_false(value: bool, message: String) -> void:
    if value:
        failures.append(message)

func assert_equal(actual, expected, message: String) -> void:
    if typeof(actual) == TYPE_FLOAT or typeof(expected) == TYPE_FLOAT:
        if not is_equal_approx(float(actual), float(expected)):
            failures.append("%s (expected=%s actual=%s)" % [message, str(expected), str(actual)])
        return
    if actual != expected:
        failures.append("%s (expected=%s actual=%s)" % [message, str(expected), str(actual)])

func assert_not_null(value, message: String) -> void:
    if value == null:
        failures.append(message)

func skip(message: String) -> void:
    skips.append(message)
