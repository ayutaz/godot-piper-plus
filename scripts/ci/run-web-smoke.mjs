import { spawn } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const END_STRING = '==== TESTS FINISHED ====';
const FAILURE_STRING = '******** FAILED ********';
const WEB_SMOKE_PREFIX = 'WEB_SMOKE status=';
const RESULT_RE = /^RESULT total=(\d+) pass=(\d+) fail=(\d+) skip=(\d+)/;

function parseArgs(argv) {
  const result = {
    root: '',
    entry: 'piper-plus-tests.html',
    label: 'web',
    timeoutMs: 240000,
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

async function main() {
  const args = parseArgs(process.argv.slice(2));
  if (args.help || !args.root) {
    console.error('Usage: node run-web-smoke.mjs --root <dir> [--entry piper-plus-tests.html] [--label name] [--timeout-ms 240000]');
    process.exit(args.help ? 0 : 1);
  }

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
  try {
    const baseUrl = await waitForServerReady(server, args.label);
    const browserUrl = new URL(args.entry, baseUrl).toString();
    browser = await chromium.launch({ headless: true });
    const page = await browser.newPage();
    let summary = null;
    let sawEnd = false;
    let webSmokeStatus = '';
    let failureReason = '';
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

      const match = text.match(RESULT_RE);
      if (match) {
        summary = {
          total: Number(match[1]),
          pass: Number(match[2]),
          fail: Number(match[3]),
          skip: Number(match[4]),
        };
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
      process.stderr.write(
        `[${args.label} requestfailed] ${request.failure()?.errorText ?? 'unknown'} ${request.method()} ${request.url()}\n`,
      );
    });

    page.on('response', (response) => {
      if (response.status() >= 400) {
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
  } finally {
    if (browser) {
      await browser.close().catch(() => {});
    }
    server.kill('SIGTERM');
  }
}

await main();
