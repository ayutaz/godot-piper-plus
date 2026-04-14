import { readFile } from 'node:fs/promises';
import path from 'node:path';

function parseArgs(argv) {
  const result = {
    project: '',
  };

  for (let i = 0; i < argv.length; i += 1) {
    const arg = argv[i];
    if (arg === '--project') {
      result.project = argv[++i] ?? '';
    } else if (arg === '--help') {
      result.help = true;
    }
  }
  return result;
}

function parseExportPresets(text) {
  const sections = new Map();
  let current = '';
  for (const rawLine of text.split(/\r?\n/)) {
    const line = rawLine.trim();
    if (!line || line.startsWith(';')) {
      continue;
    }
    const match = line.match(/^\[(.+)\]$/);
    if (match) {
      current = match[1];
      if (!sections.has(current)) {
        sections.set(current, new Map());
      }
      continue;
    }
    const eqIndex = line.indexOf('=');
    if (eqIndex === -1 || !current) {
      continue;
    }
    sections.get(current).set(line.slice(0, eqIndex), line.slice(eqIndex + 1));
  }
  return sections;
}

function stripQuotes(value) {
  return value.replace(/^"/, '').replace(/"$/, '');
}

function getPresetSections(sections, presetName) {
  for (const [sectionName, values] of sections) {
    if (!sectionName.startsWith('preset.') || sectionName.endsWith('.options')) {
      continue;
    }
    if (stripQuotes(values.get('name') ?? '') === presetName) {
      return {
        values,
        options: sections.get(`${sectionName}.options`) ?? new Map(),
      };
    }
  }
  throw new Error(`preset not found: ${presetName}`);
}

function assertValue(values, key, expected, label) {
  const actual = values.get(key);
  if (actual !== expected) {
    throw new Error(`${label} mismatch for ${key}: expected ${expected}, got ${actual ?? '<missing>'}`);
  }
}

async function main() {
  const args = parseArgs(process.argv.slice(2));
  if (args.help || !args.project) {
    console.error('Usage: node validate-web-smoke-presets.mjs --project <dir>');
    process.exit(args.help ? 0 : 1);
  }

  const projectRoot = path.resolve(args.project);
  const presetPath = path.join(projectRoot, 'export_presets.cfg');
  const presetText = await readFile(presetPath, 'utf8');
  const sections = parseExportPresets(presetText);
  const expectedIncludeFilter = '"models/*.onnx,models/*.onnx.json,piper_plus_assets/dictionaries/open_jtalk_dic_utf_8-1.11/*,addons/piper_plus/models/*/*.onnx,addons/piper_plus/models/*/*.onnx.json,addons/piper_plus/dictionaries/*.json,addons/piper_plus/model_descriptor.gd,addons/piper_plus/model_descriptors/*.json,addons/piper_plus/multilingual_sample_text_catalog.json"';

  const webPreset = getPresetSections(sections, 'Web');
  assertValue(webPreset.values, 'platform', '"Web"', 'Web preset value');
  assertValue(webPreset.values, 'custom_features', '"web_smoke"', 'Web preset value');
  assertValue(webPreset.values, 'include_filter', expectedIncludeFilter, 'Web preset value');
  assertValue(webPreset.options, 'variant/thread_support', 'false', 'Web preset option');

  const webThreadsPreset = getPresetSections(sections, 'Web Threads');
  assertValue(webThreadsPreset.values, 'platform', '"Web"', 'Web Threads preset value');
  assertValue(webThreadsPreset.values, 'custom_features', '"web_smoke,web_threads"', 'Web Threads preset value');
  assertValue(webThreadsPreset.values, 'include_filter', expectedIncludeFilter, 'Web Threads preset value');
  assertValue(webThreadsPreset.options, 'variant/thread_support', 'true', 'Web Threads preset option');

  console.log('WEB_SMOKE_PRESETS_OK');
}

await main();
