#!/usr/bin/env node

import { execFileSync } from "node:child_process";
import fs from "node:fs";
import os from "node:os";
import path from "node:path";

const ADD_EM_ASM_SIGNATURE = "function addEmAsm(addr,body){";
const ADD_EM_ASM_PATCH = 'function addEmAsm(addr,body){var args=[];var arity=0;for(;arity<16;arity++){if(body.indexOf("$"+arity)!=-1){args.push("$"+arity)}else{break}}args=args.join(",");body=body.trim();if(body.startsWith("({")&&body.endsWith("})")){body=body.slice(1,-1).trim()}var func=body.startsWith("{")?`(${args}) => ${body};`:`(${args}) => { ${body} };`;ASM_CONSTS[addr]=eval(func)}';

function escapePowerShell(value) {
  return value.replace(/'/g, "''");
}

function extractZipArchive(zipPath, destinationPath) {
  if (process.platform === "win32") {
    execFileSync(
      "powershell",
      [
        "-NoProfile",
        "-Command",
        `Expand-Archive -LiteralPath '${escapePowerShell(zipPath)}' -DestinationPath '${escapePowerShell(destinationPath)}' -Force`,
      ],
      { stdio: "inherit" },
    );
    return;
  }

  execFileSync("unzip", ["-q", "-o", zipPath, "-d", destinationPath], { stdio: "inherit" });
}

function createZipArchive(sourceDir, zipPath) {
  if (process.platform === "win32") {
    execFileSync(
      "powershell",
      [
        "-NoProfile",
        "-Command",
        `Compress-Archive -Path (Join-Path '${escapePowerShell(sourceDir)}' '*') -DestinationPath '${escapePowerShell(zipPath)}' -Force`,
      ],
      { stdio: "inherit" },
    );
    return;
  }

  execFileSync("bash", ["-lc", `zip -qr "${zipPath}" .`], { cwd: sourceDir, stdio: "inherit" });
}

function findFunctionEnd(source, startIndex) {
  let depth = 0;
  let quote = null;
  let escaped = false;

  for (let index = startIndex; index < source.length; index += 1) {
    const current = source[index];

    if (quote !== null) {
      if (escaped) {
        escaped = false;
        continue;
      }
      if (current === "\\") {
        escaped = true;
        continue;
      }
      if (current === quote) {
        quote = null;
      }
      continue;
    }

    if (current === '"' || current === "'" || current === "`") {
      quote = current;
      continue;
    }

    if (current === "{") {
      depth += 1;
      continue;
    }

    if (current === "}") {
      depth -= 1;
      if (depth === 0) {
        return index + 1;
      }
    }
  }

  throw new Error(`failed to find end of addEmAsm() starting at byte ${startIndex}`);
}

function patchJavascriptFile(filePath) {
  const original = fs.readFileSync(filePath, "utf8");
  const startIndex = original.indexOf(ADD_EM_ASM_SIGNATURE);
  if (startIndex === -1) {
    return false;
  }
  if (original.includes(ADD_EM_ASM_PATCH)) {
    return false;
  }

  const endIndex = findFunctionEnd(original, startIndex);
  const patched = `${original.slice(0, startIndex)}${ADD_EM_ASM_PATCH}${original.slice(endIndex)}`;
  fs.writeFileSync(filePath, patched);
  return true;
}

function visitJavascriptFiles(rootPath) {
  const entries = fs.readdirSync(rootPath, { withFileTypes: true });
  let patchedCount = 0;

  for (const entry of entries) {
    const entryPath = path.join(rootPath, entry.name);
    if (entry.isDirectory()) {
      patchedCount += visitJavascriptFiles(entryPath);
      continue;
    }
    if (entry.isFile() && entry.name.endsWith(".js")) {
      if (patchJavascriptFile(entryPath)) {
        patchedCount += 1;
      }
    }
  }

  return patchedCount;
}

function patchZipArchive(zipPath) {
  const tempDir = fs.mkdtempSync(path.join(os.tmpdir(), "patch-web-asm-const-"));
  try {
    extractZipArchive(zipPath, tempDir);
    const patchedCount = visitJavascriptFiles(tempDir);
    if (patchedCount === 0) {
      return 0;
    }
    fs.rmSync(zipPath, { force: true });
    createZipArchive(tempDir, zipPath);
    return patchedCount;
  } finally {
    fs.rmSync(tempDir, { recursive: true, force: true });
  }
}

function patchPath(targetPath) {
  const resolvedPath = path.resolve(targetPath);
  if (!fs.existsSync(resolvedPath)) {
    throw new Error(`path not found: ${resolvedPath}`);
  }

  const stats = fs.statSync(resolvedPath);
  if (stats.isDirectory()) {
    return visitJavascriptFiles(resolvedPath);
  }
  if (stats.isFile() && resolvedPath.endsWith(".zip")) {
    return patchZipArchive(resolvedPath);
  }
  if (stats.isFile() && resolvedPath.endsWith(".js")) {
    return patchJavascriptFile(resolvedPath) ? 1 : 0;
  }
  return 0;
}

if (process.argv.length < 3) {
  console.error("usage: patch-web-asm-const.mjs <path> [<path> ...]");
  process.exit(1);
}

let totalPatched = 0;
for (const targetPath of process.argv.slice(2)) {
  totalPatched += patchPath(targetPath);
}

console.log(`patch-web-asm-const: patched ${totalPatched} file(s)`);
