import { readFile } from 'node:fs/promises';
import path from 'node:path';

function parseArgs(argv) {
  const result = {
    project: '',
    preset: 'Web Pages',
  };

  for (let i = 0; i < argv.length; i += 1) {
    const arg = argv[i];
    if (arg === '--project') {
      result.project = argv[++i] ?? '';
    } else if (arg === '--preset') {
      result.preset = argv[++i] ?? result.preset;
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

async function main() {
  const args = parseArgs(process.argv.slice(2));
  if (args.help || !args.project) {
    console.error('Usage: node validate-pages-preset.mjs --project <dir> [--preset "Web Pages"]');
    process.exit(args.help ? 0 : 1);
  }

  const projectRoot = path.resolve(args.project);
  const presetPath = path.join(projectRoot, 'export_presets.cfg');
  const presetText = await readFile(presetPath, 'utf8');
  const sections = parseExportPresets(presetText);
  const projectPath = path.join(projectRoot, 'project.godot');
  const projectText = await readFile(projectPath, 'utf8');
  const projectSections = parseExportPresets(projectText);

  let presetIndex = null;
  for (const [sectionName, values] of sections) {
    if (!sectionName.startsWith('preset.') || sectionName.endsWith('.options')) {
      continue;
    }
    if (stripQuotes(values.get('name') ?? '') === args.preset) {
      presetIndex = sectionName.replace('preset.', '');
      break;
    }
  }

  if (presetIndex == null) {
    throw new Error(`preset not found: ${args.preset}`);
  }

  const presetValues = sections.get(`preset.${presetIndex}`) ?? new Map();
  const optionValues = sections.get(`preset.${presetIndex}.options`) ?? new Map();

  const expectedPresetValues = new Map([
    ['platform', '"Web"'],
    ['custom_features', '"web_pages_public"'],
    ['export_path', '"build/web-pages/index.html"'],
    ['include_filter', '"piper_plus_assets/models/*/*.onnx,piper_plus_assets/models/*/*.onnx.json,addons/piper_plus/dictionaries/*.json"'],
  ]);

  const expectedOptionValues = new Map([
    ['html/export_icon', 'true'],
    ['progressive_web_app/enabled', 'true'],
    ['progressive_web_app/ensure_cross_origin_isolation_headers', 'true'],
    ['variant/thread_support', 'false'],
    ['variant/extensions_support', 'true'],
    ['threads/emscripten_pool_size', '0'],
    ['threads/godot_pool_size', '0'],
    ['vram_texture_compression/for_mobile', 'false'],
  ]);

  for (const [key, expected] of expectedPresetValues) {
    const actual = presetValues.get(key);
    if (actual !== expected) {
      throw new Error(`preset value mismatch for ${key}: expected ${expected}, got ${actual ?? '<missing>'}`);
    }
  }

  for (const [key, expected] of expectedOptionValues) {
    const actual = optionValues.get(key);
    if (actual !== expected) {
      throw new Error(`preset option mismatch for ${key}: expected ${expected}, got ${actual ?? '<missing>'}`);
    }
  }

  const releaseTemplate = stripQuotes(optionValues.get('custom_template/release') ?? '');
  if (!releaseTemplate.endsWith('web_dlink_nothreads_release.zip')) {
    throw new Error(`unexpected release template path: ${releaseTemplate || '<missing>'}`);
  }

  const applicationValues = projectSections.get('application') ?? new Map();
  const projectIcon = applicationValues.get('config/icon');
  if (projectIcon !== '"res://addons/piper_plus/icon.svg"') {
    throw new Error(`project config/icon mismatch: expected "res://addons/piper_plus/icon.svg", got ${projectIcon ?? '<missing>'}`);
  }

  console.log(`PAGES_PRESET_OK preset=${args.preset}`);
}

await main();
