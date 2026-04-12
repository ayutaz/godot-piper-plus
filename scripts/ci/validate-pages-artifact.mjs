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

assertCondition(manifest.entry === "index.html", "public-demo-manifest.json must point to index.html");
assertCondition(typeof manifest.addon?.gdextension_path === "string" && manifest.addon.gdextension_path.length > 0, "manifest must declare addon.gdextension_path");
assertCondition(manifest.runtime?.thread_support === false, "manifest must declare no-thread support");
assertCondition(manifest.runtime?.execution_provider_policy === "cpu_only", "manifest must declare CPU-only runtime");
assertCondition(manifest.runtime?.pwa_enabled === true, "manifest must declare PWA enabled");
assertCondition(String(buildMeta.export_preset ?? "") === "Web Pages", "build-meta must record export_preset=Web Pages");
assertCondition(String(buildMeta.entry ?? "") === manifest.entry, "build-meta entry must match manifest entry");
assertCondition(String(buildMeta.model_key ?? "") === String(manifest.model?.key ?? ""), "build-meta model_key must match manifest");

const requiredRelativeFiles = [
  "index.html",
  "LICENSE.txt",
  "THIRD_PARTY_LICENSES.txt",
  manifest.addon?.gdextension_path,
  manifest.model?.path,
  manifest.model?.config_path,
  manifest.dictionary?.cmudict_path,
  ...(Array.isArray(manifest.notices) ? manifest.notices : []),
];

for (const relativePath of requiredRelativeFiles) {
  assertCondition(typeof relativePath === "string" && relativePath.length > 0, "manifest contains an empty path");
  const absolutePath = path.join(siteRoot, relativePath);
  assertCondition(fs.existsSync(absolutePath), `required artifact file is missing: ${relativePath}`);
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
