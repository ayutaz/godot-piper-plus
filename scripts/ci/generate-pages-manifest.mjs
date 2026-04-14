#!/usr/bin/env node

import { mkdir, readFile, writeFile } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

function parseArgs(argv) {
  const result = {
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
  console.error('Usage: node generate-pages-manifest.mjs --catalog <file> [--output <file>] [--entry index.html] [--list-language-codes]');
  process.exit(1);
}

function canonicalCode(value) {
  return String(value ?? '').trim().toLowerCase();
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
  if (args.help || !args.catalog) {
    usage();
  }

  if (args.unknown) {
    console.error(`unknown argument: ${args.unknown}`);
    usage();
  }

  const catalogPath = path.resolve(args.catalog);
  const catalog = JSON.parse(await readFile(catalogPath, 'utf8'));
  const catalogLanguages = Array.isArray(catalog.languages) ? catalog.languages : [];
  if (catalogLanguages.length === 0) {
    throw new Error(`catalog contains no languages: ${catalogPath}`);
  }

  const supportedLanguageCodes = [];
  const sampleTexts = {};
  const smokeScenarios = {};
  for (const item of catalogLanguages) {
    const languageCode = canonicalCode(item.language_code);
    if (!languageCode) {
      continue;
    }
    supportedLanguageCodes.push(languageCode);
    sampleTexts[languageCode] = String(item.template_text ?? '');
    smokeScenarios[languageCode] = buildScenario(sampleTexts[languageCode], languageCode, args);
  }

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
      key: args.modelKey,
      path: args.modelPath,
      config_path: args.configPath,
    },
    demo: {
      supported_language_codes: supportedLanguageCodes,
      default_language_code: canonicalCode(args.defaultLanguageCode),
      sample_texts: sampleTexts,
    },
    dictionary: {
      key: args.openjtalkKey,
      bootstrap_mode: 'staged_asset',
      cmudict_path: args.cmudictPath,
      pinyin_single_path: args.pinyinSinglePath,
      pinyin_phrases_path: args.pinyinPhrasesPath,
      openjtalk_path: args.openjtalkPath,
      openjtalk_install_directory: args.openjtalkInstallDirectory,
      openjtalk_required_files: ['sys.dic', 'unk.dic', 'matrix.bin', 'char.bin'],
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
