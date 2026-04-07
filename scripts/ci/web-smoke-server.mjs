import { createServer } from 'node:http';
import { promises as fs } from 'node:fs';
import path from 'node:path';

function parseArgs(argv) {
  const result = {
    root: '',
    host: '127.0.0.1',
    port: 0,
  };

  for (let i = 0; i < argv.length; i += 1) {
    const arg = argv[i];
    if (arg === '--root') {
      result.root = argv[++i] ?? '';
    } else if (arg === '--host') {
      result.host = argv[++i] ?? '127.0.0.1';
    } else if (arg === '--port') {
      result.port = Number(argv[++i] ?? '0');
    } else if (arg === '--help') {
      result.help = true;
    }
  }

  return result;
}

function contentTypeFor(filePath) {
  switch (path.extname(filePath).toLowerCase()) {
    case '.html':
      return 'text/html; charset=utf-8';
    case '.js':
    case '.mjs':
      return 'application/javascript; charset=utf-8';
    case '.json':
      return 'application/json; charset=utf-8';
    case '.wasm':
      return 'application/wasm';
    case '.pck':
      return 'application/octet-stream';
    case '.css':
      return 'text/css; charset=utf-8';
    case '.svg':
      return 'image/svg+xml';
    case '.png':
      return 'image/png';
    case '.ico':
      return 'image/x-icon';
    default:
      return 'application/octet-stream';
  }
}

const headers = {
  'Cross-Origin-Opener-Policy': 'same-origin',
  'Cross-Origin-Embedder-Policy': 'require-corp',
  'Cross-Origin-Resource-Policy': 'same-origin',
  'Cache-Control': 'no-store',
};

async function resolveFile(root, requestUrl) {
  const url = new URL(requestUrl, 'http://localhost');
  let pathname = decodeURIComponent(url.pathname);
  if (pathname === '/' || pathname === '') {
    pathname = '/index.html';
  }

  const normalized = path.normalize(pathname).replace(/^([/\\])+/, '');
  const filePath = path.resolve(root, normalized);
  const relative = path.relative(root, filePath);
  if (relative.startsWith('..') || path.isAbsolute(relative)) {
    return null;
  }

  try {
    const stat = await fs.stat(filePath);
    if (stat.isDirectory()) {
      const indexPath = path.join(filePath, 'index.html');
      await fs.access(indexPath);
      return indexPath;
    }
    return filePath;
  } catch {
    return null;
  }
}

async function main() {
  const args = parseArgs(process.argv.slice(2));
  if (args.help || !args.root) {
    console.error('Usage: node web-smoke-server.mjs --root <dir> [--host 127.0.0.1] [--port 0]');
    process.exit(args.help ? 0 : 1);
  }

  const root = path.resolve(args.root);
  await fs.access(root);

  const server = createServer(async (req, res) => {
    try {
      const filePath = await resolveFile(root, req.url ?? '/');
      if (!filePath) {
        res.writeHead(404, headers);
        res.end('Not Found');
        return;
      }

      const body = await fs.readFile(filePath);
      res.writeHead(200, {
        ...headers,
        'Content-Type': contentTypeFor(filePath),
      });
      res.end(body);
    } catch (error) {
      res.writeHead(500, headers);
      res.end(String(error instanceof Error ? error.message : error));
    }
  });

  await new Promise((resolve) => {
    server.listen(args.port, args.host, resolve);
  });

  const address = server.address();
  if (typeof address === 'string' || address == null) {
    console.log(`WEB_SMOKE_SERVER_READY http://${args.host}:${args.port}/`);
  } else {
    console.log(`WEB_SMOKE_SERVER_READY http://${args.host}:${address.port}/`);
  }

  const shutdown = () => {
    server.close(() => process.exit(0));
  };
  process.on('SIGINT', shutdown);
  process.on('SIGTERM', shutdown);
}

await main();
