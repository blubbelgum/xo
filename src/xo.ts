#!/usr/bin/env bun

import { run } from "./dev";
import { build } from "./bun.build";
import { defaultConfig } from "./config";

function showHelp() {
  console.log(`
    Usage:
      xo [command] [options]

    Commands:
      dev       Start development server (default)
      build     Production build
      init      Create sample site structure
      help      Show this help

    Options:
      --port    Set development server port
      --clean   Remove build directory before build
  `);
}

async function initProject() {
  // Create basic directory structure
  await Bun.write(
    "content/index.md",
    `---
title: Welcome
layout: default
---

# Hello World!
`,
  );

  await Bun.write(
    "layouts/default.html",
    `<!DOCTYPE html>
<html>
<head>
  <title>{{title}}</title>
</head>
<body>
  {{content}}
</body>
</html>`,
  );

  console.log("Created sample site structure!");
}

async function main() {
  const args = process.argv.slice(2);

  if (args.includes("help") || args.includes("--help")) {
    showHelp();
  } else if (args.includes("build")) {
    await build();
  } else if (args.includes("init")) {
    await initProject();
  } else {
    run();
  }
}

main();
