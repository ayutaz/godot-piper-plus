#!/usr/bin/env node

import { mkdir, readFile, writeFile } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const DEFAULT_MODEL_KEY = 'multilingual-test-medium';
const DEFAULT_MODEL_PATH = 'piper_plus_assets/models/multilingual-test-medium/multilingual-test-medium.onnx';
const DEFAULT_CONFIG_PATH = 'piper_plus_assets/models/multilingual-test-medium/multilingual-test-medium.onnx.json';
const DEFAULT_CMUDICT_PATH = 'addons/piper_plus/dictionaries/cmudict_data.json';
const DEFAULT_PINYIN_SINGLE_PATH = 'addons/piper_plus/dictionaries/pinyin_single.json';
const DEFAULT_PINYIN_PHRASES_PATH = 'addons/piper_plus/dictionaries/pinyin_phrases.json';
const DEFAULT_OPENJTALK_KEY = 'naist-jdic';
const DEFAULT_OPENJTALK_PATH = 'piper_plus_assets/dictionaries/open_jtalk_dic_utf_8-1.11';
const DEFAULT_OPENJTALK_INSTALL_DIRECTORY = 'open_jtalk_dic_utf_8-1.11';

function parseArgs(argv) {
  const result = {
    descriptor: '',
    descriptorPath: '',
    templateCatalogPath: 'addons/piper_plus/multilingual_sample_text_catalog.json',
    catalog: '',
    output: '',
    entry: 'index.html',
    addonGdextensionPath: 'addons/piper_plus/piper_plus.gdextension',
    modelKey: 'multilingual-test-medium',
    modelPath: 'piper_plus_assets/models/multilingual-test-medium/multilingual-test-medium.onnx',
    configPath: 'piper_plus_assets/models/multilingual-test-medium/multilingual-test-medium.onnx.json',
    cmudictPath: 'addons/piper_plus/dictionaries/cmudict_data.json',
    pinyinSinglePath: 'addons/piper_plus/dictionaries/pinyin_single.json',
    pinyinPhrasesPath: 'addons/piper_plus/dictionaries/pinyin_phrases.json',
    openjtalkKey: 'naist-jdic',
    openjtalkPath: 'piper_plus_assets/dictionaries/open_jtalk_dic_utf_8-1.11',
    openjtalkInstallDirectory: 'open_jtalk_dic_utf_8-1.11',
    defaultLanguageCode: 'ja',
    statusPrefix: 'PAGES_DEMO status=',
    summaryPrefix: 'PAGES_DEMO summary=',
    successStatus: 'pass',
    failureStatus: 'fail',
    timeoutMs: 240000,
    listLanguageCodes: false,
  };

  for (let i = 0; i < argv.length; i += 1) {
    const arg = argv[i];
    if (arg === '--catalog') {
      result.catalog = argv[++i] ?? '';
    } else if (arg === '--descriptor') {
      result.descriptor = argv[++i] ?? '';
    } else if (arg === '--descriptor-path') {
      result.descriptorPath = argv[++i] ?? result.descriptorPath;
    } else if (arg === '--template-catalog-path') {
      result.templateCatalogPath = argv[++i] ?? result.templateCatalogPath;
    } else if (arg === '--output') {
      result.output = argv[++i] ?? '';
    } else if (arg === '--entry') {
      result.entry = argv[++i] ?? result.entry;
    } else if (arg === '--addon-gdextension-path') {
      result.addonGdextensionPath = argv[++i] ?? result.addonGdextensionPath;
    } else if (arg === '--model-key') {
      result.modelKey = argv[++i] ?? result.modelKey;
    } else if (arg === '--model-path') {
      result.modelPath = argv[++i] ?? result.modelPath;
    } else if (arg === '--config-path') {
      result.configPath = argv[++i] ?? result.configPath;
    } else if (arg === '--cmudict-path') {
      result.cmudictPath = argv[++i] ?? result.cmudictPath;
    } else if (arg === '--pinyin-single-path') {
      result.pinyinSinglePath = argv[++i] ?? result.pinyinSinglePath;
    } else if (arg === '--pinyin-phrases-path') {
      result.pinyinPhrasesPath = argv[++i] ?? result.pinyinPhrasesPath;
    } else if (arg === '--openjtalk-key') {
      result.openjtalkKey = argv[++i] ?? result.openjtalkKey;
    } else if (arg === '--openjtalk-path') {
      result.openjtalkPath = argv[++i] ?? result.openjtalkPath;
    } else if (arg === '--openjtalk-install-directory') {
      result.openjtalkInstallDirectory = argv[++i] ?? result.openjtalkInstallDirectory;
    } else if (arg === '--default-language-code') {
      result.defaultLanguageCode = argv[++i] ?? result.defaultLanguageCode;
    } else if (arg === '--status-prefix') {
      result.statusPrefix = argv[++i] ?? result.statusPrefix;
    } else if (arg === '--summary-prefix') {
      result.summaryPrefix = argv[++i] ?? result.summaryPrefix;
    } else if (arg === '--success-status') {
      result.successStatus = argv[++i] ?? result.successStatus;
    } else if (arg === '--failure-status') {
      result.failureStatus = argv[++i] ?? result.failureStatus;
    } else if (arg === '--timeout-ms') {
      result.timeoutMs = Number(argv[++i] ?? `${result.timeoutMs}`);
    } else if (arg === '--list-language-codes') {
      result.listLanguageCodes = true;
    } else if (arg === '--help') {
      result.help = true;
    } else {
      result.unknown = arg;
    }
  }

  return result;
}

function usage() {
  console.error('Usage: node generate-pages-manifest.mjs --descriptor <file> [--output <file>] [--entry index.html] [--list-language-codes]');
  process.exit(1);
}

function canonicalCode(value) {
  return String(value ?? '').trim().toLowerCase();
}

function resolveValue(providedValue, defaultValue, descriptorValue) {
  const provided = String(providedValue ?? '');
  if (provided && provided !== defaultValue) {
    return provided;
  }
  if (descriptorValue != null && descriptorValue !== '') {
    return String(descriptorValue);
  }
  return provided || defaultValue;
}

function buildScenario(sampleText, languageCode, options) {
  const scenario = {
    status: options.successStatus,
    action: 'startup_probe',
    selected_language_code: languageCode,
    resolved_language_code: languageCode,
    input_text: sampleText,
    startup_probe_language_code: languageCode,
    startup_probe_text: sampleText,
    startup_probe_passed: true,
    supports_japanese_text_input: true,
    dictionary_bootstrap_mode: 'staged_asset',
  };

  if (languageCode === 'ja') {
    scenario.supports_japanese_text_input = true;
    scenario.dictionary_bootstrap_mode = 'staged_asset';
  }

  return scenario;
}

async function main() {
  const args = parseArgs(process.argv.slice(2));
  if (args.help || (!args.descriptor && !args.catalog)) {
    usage();
  }

  if (args.unknown) {
    console.error(`unknown argument: ${args.unknown}`);
    usage();
  }

  const sourcePath = path.resolve(args.descriptor || args.catalog);
  const source = JSON.parse(await readFile(sourcePath, 'utf8'));
  const sourceLanguages = Array.isArray(source.languages) ? source.languages : [];
  if (sourceLanguages.length === 0) {
    throw new Error(`descriptor/catalog contains no languages: ${sourcePath}`);
  }

  const supportedLanguageCodes = [];
  const sampleTexts = {};
  const smokeScenarios = {};
  for (const item of sourceLanguages) {
    const languageCode = canonicalCode(item.language_code);
    if (!languageCode) {
      continue;
    }
    supportedLanguageCodes.push(languageCode);
    sampleTexts[languageCode] = String(item.template_text ?? '');
    smokeScenarios[languageCode] = buildScenario(sampleTexts[languageCode], languageCode, args);
  }

  const assetRequirements = typeof source.asset_requirements === 'object' && source.asset_requirements !== null
    ? source.asset_requirements
    : {};
  const descriptorPath = args.descriptorPath || (args.descriptor ? `addons/piper_plus/model_descriptors/${path.basename(args.descriptor)}` : '');
  const templateCatalogPath = args.templateCatalogPath;
  const resolvedModelKey = resolveValue(args.modelKey, DEFAULT_MODEL_KEY, source.model_key);
  const resolvedModelPath = resolveValue(args.modelPath, DEFAULT_MODEL_PATH, assetRequirements.model_path);
  const resolvedConfigPath = resolveValue(args.configPath, DEFAULT_CONFIG_PATH, assetRequirements.config_path);
  const resolvedCmudictPath = resolveValue(args.cmudictPath, DEFAULT_CMUDICT_PATH, assetRequirements.cmudict_path);
  const resolvedPinyinSinglePath = resolveValue(args.pinyinSinglePath, DEFAULT_PINYIN_SINGLE_PATH, assetRequirements.pinyin_single_path);
  const resolvedPinyinPhrasesPath = resolveValue(args.pinyinPhrasesPath, DEFAULT_PINYIN_PHRASES_PATH, assetRequirements.pinyin_phrases_path);
  const resolvedOpenjtalkKey = resolveValue(args.openjtalkKey, DEFAULT_OPENJTALK_KEY, assetRequirements.dictionary_key);
  const resolvedOpenjtalkPath = resolveValue(args.openjtalkPath, DEFAULT_OPENJTALK_PATH, assetRequirements.openjtalk_path);
  const resolvedOpenjtalkInstallDirectory = resolveValue(args.openjtalkInstallDirectory, DEFAULT_OPENJTALK_INSTALL_DIRECTORY, assetRequirements.openjtalk_install_directory);
  const resolvedDefaultLanguageCode = canonicalCode(resolveValue(args.defaultLanguageCode, 'ja', source.default_language_code));

  const manifest = {
    entry: args.entry,
    addon: {
      gdextension_path: args.addonGdextensionPath,
    },
    runtime: {
      thread_support: false,
      execution_provider_policy: 'cpu_only',
      pwa_enabled: true,
    },
    model: {
      key: resolvedModelKey,
      path: resolvedModelPath,
      config_path: resolvedConfigPath,
      descriptor_path: descriptorPath,
    },
    demo: {
      supported_language_codes: supportedLanguageCodes,
      default_language_code: resolvedDefaultLanguageCode,
      sample_texts: sampleTexts,
      template_catalog_path: templateCatalogPath,
      catalog_name: String(source.catalog_name ?? 'multilingual-sample-text-catalog'),
    },
    dictionary: {
      key: resolvedOpenjtalkKey,
      bootstrap_mode: 'staged_asset',
      cmudict_path: resolvedCmudictPath,
      pinyin_single_path: resolvedPinyinSinglePath,
      pinyin_phrases_path: resolvedPinyinPhrasesPath,
      openjtalk_path: resolvedOpenjtalkPath,
      openjtalk_install_directory: resolvedOpenjtalkInstallDirectory,
      openjtalk_required_files: Array.isArray(assetRequirements.openjtalk_required_files)
        ? assetRequirements.openjtalk_required_files.slice()
        : ['sys.dic', 'unk.dic', 'matrix.bin', 'char.bin'],
    },
    notices: ['LICENSE.txt', 'THIRD_PARTY_LICENSES.txt'],
    smoke: {
      statusPrefix: args.statusPrefix,
      summaryPrefix: args.summaryPrefix,
      successStatus: args.successStatus,
      failureStatus: args.failureStatus,
      timeoutMs: args.timeoutMs,
      scenarios: smokeScenarios,
    },
  };

  if (args.listLanguageCodes) {
    process.stdout.write(`${supportedLanguageCodes.join(',')}\n`);
    return;
  }

  const json = `${JSON.stringify(manifest, null, 2)}\n`;
  if (args.output) {
    const outputPath = path.resolve(args.output);
    await mkdir(path.dirname(outputPath), { recursive: true });
    await writeFile(outputPath, json);
  } else {
    process.stdout.write(json);
  }
}

await main();
