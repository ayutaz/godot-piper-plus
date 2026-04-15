#!/usr/bin/env node

import fs from "node:fs";
import path from "node:path";

function usage() {
  console.error("usage: validate-pages-artifact.mjs <artifact-dir> [--expected-build <sha>]");
  process.exit(1);
}

function assertCondition(condition, message) {
  if (!condition) {
    throw new Error(message);
  }
}

function requireJson(filePath) {
  assertCondition(fs.existsSync(filePath), `missing required file: ${filePath}`);
  return JSON.parse(fs.readFileSync(filePath, "utf8"));
}

function hasExtension(rootDir, extension) {
  return fs.readdirSync(rootDir).some((name) => name.toLowerCase().endsWith(extension));
}

const argv = process.argv.slice(2);
const artifactDir = argv[0];
if (!artifactDir) {
  usage();
}

let expectedBuild = "";
for (let index = 1; index < argv.length; index += 1) {
  const arg = argv[index];
  if (arg === "--expected-build") {
    const value = argv[index + 1];
    if (value == null || value.startsWith("--")) {
      console.error("missing value for --expected-build");
      usage();
    }
    expectedBuild = value;
    index += 1;
  } else {
    console.error(`unknown argument: ${arg}`);
    usage();
  }
}

const siteRoot = path.resolve(artifactDir);
assertCondition(fs.existsSync(siteRoot), `artifact directory does not exist: ${siteRoot}`);
assertCondition(fs.statSync(siteRoot).isDirectory(), `artifact path is not a directory: ${siteRoot}`);

const manifestPath = path.join(siteRoot, "public-demo-manifest.json");
const buildMetaPath = path.join(siteRoot, "build-meta.json");
const manifest = requireJson(manifestPath);
const buildMeta = requireJson(buildMetaPath);

assertCondition(typeof manifest.model?.descriptor_path === "string" && manifest.model.descriptor_path.length > 0, "manifest must declare model.descriptor_path");
const descriptorPath = path.resolve(siteRoot, manifest.model.descriptor_path);
assertCondition(
  descriptorPath === siteRoot || descriptorPath.startsWith(`${siteRoot}${path.sep}`),
  `descriptor_path must stay within the artifact root: ${manifest.model.descriptor_path}`,
);
const descriptor = requireJson(descriptorPath);
assertCondition(Array.isArray(descriptor.languages), "model descriptor must define languages");
const expectedLanguageCodes = descriptor.languages.map((entry) => String(entry.language_code ?? ""));
const expectedSampleTexts = new Map(
  descriptor.languages.map((entry) => [String(entry.language_code ?? ""), String(entry.template_text ?? "")]),
);

assertCondition(manifest.entry === "index.html", "public-demo-manifest.json must point to index.html");
assertCondition(typeof manifest.addon?.gdextension_path === "string" && manifest.addon.gdextension_path.length > 0, "manifest must declare addon.gdextension_path");
assertCondition(manifest.runtime?.thread_support === false, "manifest must declare no-thread support");
assertCondition(manifest.runtime?.execution_provider_policy === "cpu_only", "manifest must declare CPU-only runtime");
assertCondition(manifest.runtime?.pwa_enabled === true, "manifest must declare PWA enabled");
assertCondition(String(buildMeta.export_preset ?? "") === "Web Pages", "build-meta must record export_preset=Web Pages");
assertCondition(String(buildMeta.entry ?? "") === manifest.entry, "build-meta entry must match manifest entry");
assertCondition(String(buildMeta.model_key ?? "") === String(manifest.model?.key ?? ""), "build-meta model_key must match manifest");
assertCondition(String(manifest.demo?.default_language_code ?? "") === "ja", "manifest must declare ja as the default language");
assertCondition(String(manifest.model?.descriptor_path ?? "") === "addons/piper_plus/model_descriptors/multilingual-test-medium.json", "manifest must declare the descriptor path");
assertCondition(String(manifest.demo?.template_catalog_path ?? "") === "addons/piper_plus/multilingual_sample_text_catalog.json", "manifest must declare the compatibility template catalog path");
assertCondition(Array.isArray(manifest.demo?.supported_language_codes), "manifest must declare demo.supported_language_codes");
assertCondition(
  JSON.stringify(manifest.demo.supported_language_codes) === JSON.stringify(expectedLanguageCodes),
  "manifest must match the canonical six-language support order",
);
assertCondition(String(manifest.demo?.catalog_name ?? "") === String(descriptor.catalog_name ?? ""), "manifest catalog_name must match the descriptor");
for (const [languageCode, sampleText] of expectedSampleTexts.entries()) {
  assertCondition(
    String(manifest.demo?.sample_texts?.[languageCode] ?? "") === sampleText,
    `manifest must declare the canonical sample text for ${languageCode}`,
  );
}
assertCondition(String(buildMeta.default_language_code ?? "") === String(manifest.demo?.default_language_code ?? ""), "build-meta default_language_code must match manifest");
assertCondition(manifest.smoke?.scenarios && typeof manifest.smoke.scenarios === "object", "manifest must declare smoke.scenarios");
for (const [languageCode, sampleText] of expectedSampleTexts.entries()) {
  const smokeScenario = manifest.smoke.scenarios[languageCode];
  assertCondition(Boolean(smokeScenario), `manifest must declare smoke.scenarios.${languageCode}`);
  assertCondition(String(smokeScenario?.action ?? "") === "startup_probe", `${languageCode} smoke scenario must validate the startup probe`);
  assertCondition(String(smokeScenario?.selected_language_code ?? "") === languageCode, `${languageCode} smoke scenario must select ${languageCode}`);
  assertCondition(String(smokeScenario?.resolved_language_code ?? "") === languageCode, `${languageCode} smoke scenario must resolve ${languageCode}`);
  assertCondition(String(smokeScenario?.input_text ?? "") === sampleText, `${languageCode} smoke scenario must validate the canonical sample text`);
  assertCondition(String(smokeScenario?.startup_probe_language_code ?? "") === languageCode, `${languageCode} smoke scenario must probe ${languageCode}`);
  assertCondition(String(smokeScenario?.startup_probe_text ?? "") === sampleText, `${languageCode} smoke scenario must probe the canonical sample text`);
  assertCondition(smokeScenario?.startup_probe_passed === true, `${languageCode} smoke scenario must require startup_probe_passed=true`);
  assertCondition(smokeScenario?.supports_japanese_text_input === true, `${languageCode} smoke scenario must require supports_japanese_text_input=true`);
  assertCondition(String(smokeScenario?.dictionary_bootstrap_mode ?? "") === "staged_asset", `${languageCode} smoke scenario must require staged_asset bootstrap mode`);
}

const requiredRelativeFiles = [
  "index.html",
  "LICENSE.txt",
  "THIRD_PARTY_LICENSES.txt",
  manifest.addon?.gdextension_path,
  manifest.model?.path,
  manifest.model?.config_path,
  manifest.model?.descriptor_path,
  manifest.demo?.template_catalog_path,
  manifest.dictionary?.cmudict_path,
  manifest.dictionary?.pinyin_single_path,
  manifest.dictionary?.pinyin_phrases_path,
  ...(Array.isArray(manifest.notices) ? manifest.notices : []),
];

for (const relativePath of requiredRelativeFiles) {
  assertCondition(typeof relativePath === "string" && relativePath.length > 0, "manifest contains an empty path");
  const absolutePath = path.join(siteRoot, relativePath);
  assertCondition(fs.existsSync(absolutePath), `required artifact file is missing: ${relativePath}`);
}

if (manifest.dictionary?.openjtalk_path) {
  assertCondition(typeof manifest.dictionary?.key === "string" && manifest.dictionary.key.length > 0, "manifest must declare dictionary.key when openjtalk_path is present");
  assertCondition(manifest.dictionary?.bootstrap_mode === "staged_asset", "manifest must declare staged_asset bootstrap mode for OpenJTalk");
  assertCondition(
    String(manifest.dictionary?.openjtalk_install_directory ?? "") === "open_jtalk_dic_utf_8-1.11",
    "manifest must declare open_jtalk_dic_utf_8-1.11 as the install directory",
  );
  const openjtalkRequiredFiles = manifest.dictionary.openjtalk_required_files;
  const expectedOpenjtalkRequiredFiles = ["char.bin", "matrix.bin", "sys.dic", "unk.dic"];
  assertCondition(Array.isArray(openjtalkRequiredFiles), "manifest must declare dictionary.openjtalk_required_files when openjtalk_path is present");
  assertCondition(openjtalkRequiredFiles.length > 0, "manifest must declare at least one OpenJTalk required file when openjtalk_path is present");
  assertCondition(
    openjtalkRequiredFiles.every((filename) => typeof filename === "string" && filename.length > 0),
    "manifest contains an empty OpenJTalk required file path",
  );
  assertCondition(
    expectedOpenjtalkRequiredFiles.every((filename) => openjtalkRequiredFiles.includes(filename)),
    `manifest must declare OpenJTalk required files: ${expectedOpenjtalkRequiredFiles.join(", ")}`,
  );
  const openjtalkRoot = path.join(siteRoot, manifest.dictionary.openjtalk_path);
  assertCondition(fs.existsSync(openjtalkRoot), `OpenJTalk dictionary directory is missing: ${manifest.dictionary.openjtalk_path}`);
  assertCondition(fs.statSync(openjtalkRoot).isDirectory(), `OpenJTalk dictionary path is not a directory: ${manifest.dictionary.openjtalk_path}`);
  for (const filename of openjtalkRequiredFiles) {
    const absolutePath = path.join(openjtalkRoot, filename);
    assertCondition(fs.existsSync(absolutePath), `OpenJTalk dictionary file is missing: ${path.posix.join(manifest.dictionary.openjtalk_path, filename)}`);
  }
}

const addonBinDir = path.join(siteRoot, path.dirname(manifest.addon.gdextension_path), "bin");
assertCondition(fs.existsSync(addonBinDir), `addon runtime bin directory is missing: ${addonBinDir}`);
assertCondition(fs.statSync(addonBinDir).isDirectory(), `addon runtime bin path is not a directory: ${addonBinDir}`);
assertCondition(
  fs.readdirSync(addonBinDir).some((name) => name !== ".gitignore"),
  `addon runtime bin directory is empty: ${addonBinDir}`,
);

assertCondition(hasExtension(siteRoot, ".js"), "artifact root must contain a .js loader file");
assertCondition(hasExtension(siteRoot, ".wasm"), "artifact root must contain a .wasm runtime file");
assertCondition(hasExtension(siteRoot, ".pck"), "artifact root must contain a .pck data file");

const entryHtml = fs.readFileSync(path.join(siteRoot, "index.html"), "utf8");
assertCondition(entryHtml.includes(".pck"), "index.html must reference the exported .pck payload");

if (expectedBuild) {
  assertCondition(
    String(buildMeta.git_sha ?? "") === expectedBuild,
    `build-meta git_sha mismatch: expected ${expectedBuild}, got ${String(buildMeta.git_sha ?? "")}`,
  );
}

console.log(
  JSON.stringify(
    {
      artifact_dir: siteRoot,
      git_sha: buildMeta.git_sha,
      model_key: manifest.model?.key,
      notices: manifest.notices,
    },
    null,
    2,
  ),
);
