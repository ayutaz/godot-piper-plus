import { mkdir, readFile, writeFile } from 'node:fs/promises';
import path from 'node:path';
import { spawn } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const STATUS_PREFIX = 'PAGES_DEMO status=';
const SUMMARY_PREFIX = 'PAGES_DEMO summary=';

function parseArgs(argv) {
  const result = {
    root: '',
    url: '',
    entry: '',
    manifest: '',
    label: 'pages-demo',
    scenario: '',
    timeoutMs: 240000,
    screenshot: '',
    consoleLog: '',
  };

  for (let i = 0; i < argv.length; i += 1) {
    const arg = argv[i];
    if (arg === '--root') {
      result.root = argv[++i] ?? '';
    } else if (arg === '--url') {
      result.url = argv[++i] ?? '';
    } else if (arg === '--entry') {
      result.entry = argv[++i] ?? '';
    } else if (arg === '--manifest') {
      result.manifest = argv[++i] ?? '';
    } else if (arg === '--label') {
      result.label = argv[++i] ?? result.label;
    } else if (arg === '--scenario') {
      result.scenario = argv[++i] ?? '';
    } else if (arg === '--timeout-ms') {
      result.timeoutMs = Number(argv[++i] ?? `${result.timeoutMs}`);
    } else if (arg === '--screenshot') {
      result.screenshot = argv[++i] ?? '';
    } else if (arg === '--console-log') {
      result.consoleLog = argv[++i] ?? '';
    } else if (arg === '--help') {
      result.help = true;
    }
  }

  return result;
}

function waitForServerReady(child, label) {
  return new Promise((resolve, reject) => {
    let buffered = '';

    child.stdout.on('data', (chunk) => {
      const text = chunk.toString('utf8');
      process.stdout.write(`[${label} server] ${text}`);
      buffered += text;
      const lines = buffered.split(/\r?\n/);
      buffered = lines.pop() ?? '';

      for (const line of lines) {
        if (line.startsWith('WEB_SMOKE_SERVER_READY ')) {
          resolve(line.substring('WEB_SMOKE_SERVER_READY '.length).trim());
        }
      }
    });

    child.stderr.on('data', (chunk) => {
      process.stderr.write(`[${label} server] ${chunk.toString('utf8')}`);
    });

    child.on('exit', (code) => {
      reject(new Error(`server exited before becoming ready (code=${code})`));
    });

    child.on('error', reject);
  });
}

function normalizeBaseUrl(rawUrl) {
  const url = new URL(rawUrl);
  if (!url.pathname.endsWith('/')) {
    url.pathname = `${url.pathname}/`;
  }
  return url.toString();
}

async function delay(ms) {
  await new Promise((resolve) => setTimeout(resolve, ms));
}

async function fetchJsonWithRetry(url, timeoutMs) {
  const deadline = Date.now() + timeoutMs;
  let lastError = null;

  while (Date.now() < deadline) {
    try {
      const response = await fetch(url, { cache: 'no-store' });
      if (response.ok) {
        return response.json();
      }
      lastError = new Error(`failed to fetch manifest: ${url} (${response.status})`);
    } catch (error) {
      lastError = error;
    }

    await delay(3000);
  }

  throw lastError ?? new Error(`failed to fetch manifest: ${url}`);
}

async function gotoWithRetry(page, url, timeoutMs) {
  const deadline = Date.now() + timeoutMs;
  let lastError = null;

  while (Date.now() < deadline) {
    try {
      await page.goto(url, { waitUntil: 'load', timeout: Math.min(30000, timeoutMs) });
      return;
    } catch (error) {
      lastError = error;
    }

    await delay(3000);
  }

  throw lastError ?? new Error(`failed to open page: ${url}`);
}

async function loadManifest(args) {
  if (args.root) {
    const manifestPath = path.join(path.resolve(args.root), args.manifest || 'public-demo-manifest.json');
    return JSON.parse(await readFile(manifestPath, 'utf8'));
  }

  const manifestUrl = new URL(args.manifest || 'public-demo-manifest.json', normalizeBaseUrl(args.url)).toString();
  return fetchJsonWithRetry(manifestUrl, args.timeoutMs);
}

function summaryMatchesScenario(summary, scenario) {
  if (!scenario) {
    return true;
  }

  if (!summary || typeof summary !== 'object') {
    return false;
  }

  const checks = [
    ['status', summary.status, scenario.status],
    ['action', summary.action, scenario.action],
    ['selected_language_code', summary.selected_language_code, scenario.selected_language_code],
    ['resolved_language_code', summary.resolved_language_code, scenario.resolved_language_code],
    ['input_text', summary.input_text, scenario.input_text],
    ['startup_probe_language_code', summary.startup_probe_language_code, scenario.startup_probe_language_code],
    ['startup_probe_text', summary.startup_probe_text, scenario.startup_probe_text],
    ['startup_probe_passed', summary.startup_probe_passed, scenario.startup_probe_passed],
    ['supports_japanese_text_input', summary.supports_japanese_text_input, scenario.supports_japanese_text_input],
    ['dictionary_bootstrap_mode', summary.dictionary_bootstrap_mode, scenario.dictionary_bootstrap_mode],
  ];

  for (const [, actual, expected] of checks) {
    if (expected === undefined) {
      continue;
    }
    if (actual !== expected) {
      return false;
    }
  }

  return true;
}

async function main() {
  const args = parseArgs(process.argv.slice(2));
  if (args.help || (!args.root && !args.url)) {
    console.error('Usage: node run-pages-demo-smoke.mjs (--root <dir> | --url <page_url>) [--manifest public-demo-manifest.json] [--entry index.html] [--scenario <language_code>]');
    process.exit(args.help ? 0 : 1);
  }

  let chromium;
  try {
    ({ chromium } = await import('playwright'));
  } catch (error) {
    throw new Error(
      `playwright is required for pages demo smoke. Install it with "npm install --no-save playwright" and "npx playwright install chromium".\n${String(error)}`,
    );
  }

  const consoleEntries = [];
  let browser;
  let server;
  let page;
  let baseUrl = args.url;
  let failureReason = '';
  let lastSummary = null;
  let lastSummaryRaw = '';

  try {
    if (args.root) {
      const serverScript = fileURLToPath(new URL('./web-smoke-server.mjs', import.meta.url));
      server = spawn(process.execPath, [serverScript, '--root', args.root, '--port', '0', '--coi-mode', 'none', '--cache-control', 'no-store'], {
        stdio: ['ignore', 'pipe', 'pipe'],
      });
      baseUrl = await waitForServerReady(server, args.label);
    } else {
      baseUrl = normalizeBaseUrl(baseUrl);
    }

    const manifest = await loadManifest({
      ...args,
      url: baseUrl,
    });
    const statusPrefix = manifest.smoke?.statusPrefix ?? STATUS_PREFIX;
    const summaryPrefix = manifest.smoke?.summaryPrefix ?? SUMMARY_PREFIX;
    const successStatus = manifest.smoke?.successStatus ?? 'pass';
    const failureStatus = manifest.smoke?.failureStatus ?? 'fail';
    const scenario = args.scenario
      ? manifest.smoke?.scenarios?.[args.scenario] ?? null
      : null;

    if (args.scenario && !scenario) {
      throw new Error(`pages demo smoke scenario is not declared in manifest: ${args.scenario}`);
    }

    const entry = args.entry || manifest.entry || 'index.html';
    const browserUrl = new URL(entry, baseUrl).toString();
    const cacheBustedUrl = new URL(browserUrl);
    cacheBustedUrl.searchParams.set('smoke', `${Date.now()}`);

    browser = await chromium.launch({ headless: true });
    const context = await browser.newContext();
    page = await context.newPage();

    if (scenario) {
      await page.addInitScript((scenarioName) => {
        globalThis.__PIPER_WEB_SMOKE_SCENARIO = scenarioName;
      }, args.scenario);
    }

    let status = '';

    page.on('console', (message) => {
      const text = message.text();
      const entryRecord = {
        type: message.type(),
        text,
      };
      consoleEntries.push(entryRecord);
      process.stdout.write(`[${args.label} browser:${message.type()}] ${text}\n`);

      if (text.startsWith(statusPrefix)) {
        status = text.substring(statusPrefix.length).trim();
        if (status === failureStatus) {
          failureReason = `browser reported ${statusPrefix}${failureStatus}`;
        }
      } else if (text.startsWith(summaryPrefix)) {
        lastSummaryRaw = text.substring(summaryPrefix.length).trim();
        try {
          lastSummary = JSON.parse(lastSummaryRaw);
        } catch (error) {
          failureReason = `failed to parse ${summaryPrefix} payload: ${String(error)}`;
        }
      }
    });

    page.on('pageerror', (error) => {
      if (!failureReason) {
        failureReason = `pageerror: ${error instanceof Error && error.stack ? error.stack : String(error)}`;
      }
    });

    page.on('requestfailed', (request) => {
      const text = `requestfailed: ${request.failure()?.errorText ?? 'unknown'} ${request.method()} ${request.url()}`;
      consoleEntries.push({ type: 'requestfailed', text });
      process.stderr.write(`[${args.label}] ${text}\n`);
    });

    page.on('response', (response) => {
      if (response.status() >= 400) {
        const text = `response: ${response.status()} ${response.url()}`;
        consoleEntries.push({ type: 'response', text });
        process.stderr.write(`[${args.label}] ${text}\n`);
      }
    });

    await gotoWithRetry(page, cacheBustedUrl.toString(), args.timeoutMs);

    const deadline = Date.now() + (manifest.smoke?.timeoutMs ?? args.timeoutMs);
    while (Date.now() < deadline) {
      if (status === successStatus) {
        if (!summaryMatchesScenario(lastSummary, scenario)) {
          await page.waitForTimeout(1000);
          continue;
        }
        if (args.consoleLog) {
          await mkdir(path.dirname(path.resolve(args.consoleLog)), { recursive: true });
          await writeFile(path.resolve(args.consoleLog), JSON.stringify({
            status,
            scenario: args.scenario || null,
            summary: lastSummary,
            raw_summary: lastSummaryRaw || null,
            entries: consoleEntries,
          }, null, 2));
        }
        return;
      }

      if (failureReason) {
        throw new Error(failureReason);
      }

      await page.waitForTimeout(1000);
    }

    throw new Error(`pages demo smoke timed out waiting for ${statusPrefix}${successStatus}`);
  } catch (error) {
    if (page && args.screenshot) {
      await mkdir(path.dirname(path.resolve(args.screenshot)), { recursive: true });
      await page.screenshot({ path: path.resolve(args.screenshot), fullPage: true }).catch(() => {});
    }
    if (args.consoleLog) {
      await mkdir(path.dirname(path.resolve(args.consoleLog)), { recursive: true });
      await writeFile(path.resolve(args.consoleLog), JSON.stringify({
        status: 'fail',
        scenario: args.scenario || null,
        error: error instanceof Error ? error.message : String(error),
        summary: lastSummary,
        raw_summary: lastSummaryRaw || null,
        entries: consoleEntries,
      }, null, 2));
    }
    throw error;
  } finally {
    if (browser) {
      await browser.close().catch(() => {});
    }
    if (server) {
      server.kill('SIGTERM');
    }
  }
}

await main();
