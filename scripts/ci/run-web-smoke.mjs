import { spawn } from 'node:child_process';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const END_STRING = '==== TESTS FINISHED ====';
const FAILURE_STRING = '******** FAILED ********';
const WEB_SMOKE_PREFIX = 'WEB_SMOKE status=';
const WEB_SMOKE_SUMMARY_PREFIX = 'WEB_SMOKE summary=';
const RESULT_RE = /^RESULT total=(\d+) pass=(\d+) fail=(\d+) skip=(\d+)/;
const PASS_RE = /^\s+PASS\s+(.+)$/;
const SKIP_RE = /^\s+SKIP\s+(.+?)(?::\s+(.*))?$/;
const FAIL_RE = /^\s+FAIL\s+(.+?)(?::\s+(.*))?$/;
const SCENARIO_PROFILES = {
  en: {
    requiredPasses: [
      'test_piper_tts.test_initialize_with_model',
      'test_piper_tts.test_synthesize_basic',
    ],
    timeoutMs: 240000,
  },
  ja: {
    requiredPasses: [
      'test_piper_tts.test_japanese_dictionary_error_surface',
      'test_piper_tts.test_japanese_request_time_dictionary_error_surface',
      'test_piper_tts.test_japanese_text_input_with_dictionary',
    ],
    timeoutMs: 300000,
  },
};

function parseArgs(argv) {
  const result = {
    root: '',
    entry: 'piper-plus-tests.html',
    label: 'web',
    timeoutMs: 240000,
    scenario: '',
    requirePasses: [],
    reportPath: '',
  };

  for (let i = 0; i < argv.length; i += 1) {
    const arg = argv[i];
    if (arg === '--root') {
      result.root = argv[++i] ?? '';
    } else if (arg === '--entry') {
      result.entry = argv[++i] ?? result.entry;
    } else if (arg === '--label') {
      result.label = argv[++i] ?? result.label;
    } else if (arg === '--timeout-ms') {
      result.timeoutMs = Number(argv[++i] ?? `${result.timeoutMs}`);
    } else if (arg === '--scenario') {
      result.scenario = argv[++i] ?? '';
    } else if (arg === '--require-pass') {
      result.requirePasses.push(argv[++i] ?? '');
    } else if (arg === '--report-path') {
      result.reportPath = argv[++i] ?? '';
    } else if (arg === '--help') {
      result.help = true;
    }
  }

  return result;
}

function usage() {
  console.error('Usage: node run-web-smoke.mjs --root <dir> [--entry piper-plus-tests.html] [--label name] [--scenario en|ja] [--require-pass suite.test] [--timeout-ms 240000] [--report-path file]');
}

function safeSlug(value) {
  return String(value || 'web')
    .toLowerCase()
    .replace(/[^a-z0-9._-]+/g, '-')
    .replace(/^-+|-+$/g, '') || 'web';
}

function ensureParentDir(filePath) {
  fs.mkdirSync(path.dirname(filePath), { recursive: true });
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

async function main() {
  const args = parseArgs(process.argv.slice(2));
  if (args.help || !args.root) {
    usage();
    process.exit(args.help ? 0 : 1);
  }

  const scenarioProfile = args.scenario ? SCENARIO_PROFILES[args.scenario] : null;
  if (args.scenario && !scenarioProfile) {
    throw new Error(`unknown scenario: ${args.scenario}`);
  }
  if (scenarioProfile && args.timeoutMs === 240000) {
    args.timeoutMs = scenarioProfile.timeoutMs;
  }

  const requiredPasses = new Set(
    [...(scenarioProfile?.requiredPasses ?? []), ...args.requirePasses].filter(Boolean),
  );
  const reportPath = path.resolve(
    args.reportPath || path.join(args.root, `web-smoke-report-${safeSlug(args.label)}.json`),
  );
  const screenshotPath = path.join(
    path.dirname(reportPath),
    `${path.basename(reportPath, path.extname(reportPath))}.png`,
  );

  let chromium;
  try {
    ({ chromium } = await import('playwright'));
  } catch (error) {
    throw new Error(
      `playwright is required for web smoke. Install it with "npm install --no-save playwright" and "npx playwright install chromium".\n${String(error)}`,
    );
  }

  const serverScript = fileURLToPath(new URL('./web-smoke-server.mjs', import.meta.url));
  const server = spawn(process.execPath, [serverScript, '--root', args.root, '--port', '0'], {
    stdio: ['ignore', 'pipe', 'pipe'],
  });

  let browser;
  let page;
  let failureReason = '';
  let summary = null;
  let rawSummary = null;
  let sawEnd = false;
  let webSmokeStatus = '';
  const passedTests = new Set();
  const skippedTests = [];
  const failedTests = [];
  const consoleEntries = [];
  const failedRequests = [];
  const errorResponses = [];

  const writeReport = () => {
    ensureParentDir(reportPath);
    fs.writeFileSync(
      reportPath,
      JSON.stringify(
        {
          label: args.label,
          scenario: args.scenario || null,
          root: path.resolve(args.root),
          entry: args.entry,
          timeout_ms: args.timeoutMs,
          required_passes: [...requiredPasses],
          web_smoke_status: webSmokeStatus || null,
          saw_end_string: sawEnd,
          summary,
          raw_summary: rawSummary,
          passed_tests: summary?.passed_tests ?? [...passedTests].map((test) => ({ test })),
          skipped_tests: summary?.skipped_tests ?? skippedTests,
          failed_tests: summary?.failed_tests ?? failedTests,
          failed_requests: failedRequests,
          error_responses: errorResponses,
          failure_reason: failureReason || null,
          console: consoleEntries,
          screenshot: fs.existsSync(screenshotPath) ? path.basename(screenshotPath) : null,
        },
        null,
        2,
      ),
    );
  };

  try {
    const baseUrl = await waitForServerReady(server, args.label);
    const browserUrl = new URL(args.entry, baseUrl).toString();
    browser = await chromium.launch({ headless: true });
    page = await browser.newPage();
    let completed = false;

    const hasSuccessfulSummary = () =>
      Boolean(
        sawEnd &&
          summary &&
          (!webSmokeStatus || webSmokeStatus === 'pass') &&
          summary.fail === 0 &&
          summary.pass > 0,
      );

    page.on('console', (message) => {
      const text = message.text();
      const location = message.location();
      const locationText = location?.url
        ? ` (${location.url}:${location.lineNumber ?? 0}:${location.columnNumber ?? 0})`
        : '';
      consoleEntries.push({
        type: message.type(),
        text,
        location: location?.url ? {
          url: location.url,
          line: location.lineNumber ?? 0,
          column: location.columnNumber ?? 0,
        } : null,
      });
      process.stdout.write(`[${args.label} browser:${message.type()}] ${text}${locationText}\n`);

      if (text.includes(FAILURE_STRING)) {
        failureReason = `browser reported failure marker: ${text}`;
      }
      if (text.includes(END_STRING)) {
        sawEnd = true;
      }
      if (text.startsWith(WEB_SMOKE_PREFIX)) {
        webSmokeStatus = text.substring(WEB_SMOKE_PREFIX.length).trim();
        if (webSmokeStatus === 'fail') {
          failureReason = 'browser reported WEB_SMOKE status=fail';
        }
      }
      if (text.startsWith(WEB_SMOKE_SUMMARY_PREFIX)) {
        rawSummary = text.substring(WEB_SMOKE_SUMMARY_PREFIX.length).trim();
        try {
          summary = JSON.parse(rawSummary);
        } catch (error) {
          failureReason = `failed to parse WEB_SMOKE summary JSON: ${String(error)}`;
        }
      }

      const match = text.match(RESULT_RE);
      if (match) {
        summary = {
          total: Number(match[1]),
          pass: Number(match[2]),
          fail: Number(match[3]),
          skip: Number(match[4]),
        };
      }

      const passMatch = text.match(PASS_RE);
      if (passMatch) {
        passedTests.add(passMatch[1].trim());
      }
      const skipMatch = text.match(SKIP_RE);
      if (skipMatch) {
        skippedTests.push({
          test: skipMatch[1].trim(),
          message: (skipMatch[2] ?? '').trim(),
        });
      }
      const failMatch = text.match(FAIL_RE);
      if (failMatch) {
        failedTests.push({
          test: failMatch[1].trim(),
          message: (failMatch[2] ?? '').trim(),
        });
      }
    });

    page.on('pageerror', (error) => {
      if (completed) {
        return;
      }
      const stack = error instanceof Error && error.stack ? error.stack : String(error);
      failureReason = `pageerror: ${stack}`;
    });

    page.on('requestfailed', (request) => {
      failedRequests.push({
        url: request.url(),
        method: request.method(),
        error: request.failure()?.errorText ?? 'unknown',
      });
      process.stderr.write(
        `[${args.label} requestfailed] ${request.failure()?.errorText ?? 'unknown'} ${request.method()} ${request.url()}\n`,
      );
    });

    page.on('response', (response) => {
      if (response.status() >= 400) {
        errorResponses.push({
          url: response.url(),
          status: response.status(),
        });
        process.stderr.write(`[${args.label} response] ${response.status()} ${response.url()}\n`);
      }
    });

    await page.goto(browserUrl, { waitUntil: 'load', timeout: args.timeoutMs });

    const deadline = Date.now() + args.timeoutMs;
    while (Date.now() < deadline) {
      if (hasSuccessfulSummary()) {
        completed = true;
        break;
      }
      if (failureReason) {
        throw new Error(failureReason);
      }
      await page.waitForTimeout(1000);
    }

    if (!sawEnd) {
      throw new Error(`web smoke timed out waiting for ${END_STRING}`);
    }
    if (!summary) {
      throw new Error('web smoke did not emit a RESULT summary');
    }
    if (webSmokeStatus && webSmokeStatus !== 'pass') {
      throw new Error(`web smoke reported unexpected status=${webSmokeStatus}`);
    }
    if (summary.fail !== 0) {
      throw new Error(`web smoke reported ${summary.fail} failure(s)`);
    }
    if (summary.pass <= 0) {
      throw new Error('web smoke completed without any passing tests');
    }

    const reportedPasses = new Set(
      Array.isArray(summary?.passed_tests)
        ? summary.passed_tests.map((entry) => String(entry?.test ?? '')).filter(Boolean)
        : [...passedTests],
    );
    const missingPasses = [...requiredPasses].filter((testName) => !reportedPasses.has(testName));
    if (missingPasses.length > 0) {
      throw new Error(`web smoke scenario '${args.scenario || args.label}' is missing required passing tests: ${missingPasses.join(', ')}`);
    }
  } catch (error) {
    if (!failureReason) {
      failureReason = error instanceof Error ? error.message : String(error);
    }
    throw error;
  } finally {
    if (failureReason && page) {
      await page.screenshot({ path: screenshotPath, fullPage: true }).catch(() => {});
    }
    writeReport();
    if (browser) {
      await browser.close().catch(() => {});
    }
    server.kill('SIGTERM');
  }
}

await main();
