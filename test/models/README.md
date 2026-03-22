## テストモデル

このディレクトリは、`test/run-tests.sh` と GitHub Actions の Godot headless 統合テストで使うモデル bundle を保持します。

- `multilingual-test-medium.onnx`
- `multilingual-test-medium.onnx.json`

`test/prepare-assets.sh` が毎回この内容を `test/project/models/` へ同期してからテストを起動します。ローカルで別モデルを試したい場合は、`PIPER_TEST_MODEL_PATH` / `PIPER_TEST_CONFIG_PATH` / `PIPER_TEST_DICT_PATH` / `PIPER_TEST_LANGUAGE_CODE` / `PIPER_TEST_TEXT` を使って上書きできます。
