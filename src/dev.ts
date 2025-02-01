import { watch } from "fs/promises";
import { build } from "./bun.build";
// import { spawn } from "bun";
import { join } from "path";
import { createHash } from "crypto";
import type { ServerWebSocket } from "bun";
import { reverseDependencies } from "./deps";

type HashCache = Map<string, string>;
const hashCache: HashCache = new Map();

// ========================
// FILE HASHING
// ========================
async function computeHash(path: string): Promise<string> {
  try {
    const file = await Bun.file(path).text();
    return createHash("sha1").update(file).digest("hex");
  } catch {
    return "";
  }
}

async function shouldRebuild(filePath: string): Promise<boolean> {
  const newHash = await computeHash(filePath);
  const oldHash = hashCache.get(filePath);

  if (newHash !== oldHash) {
    hashCache.set(filePath, newHash);
    return true;
  }
  return false;
}

// ========================
// DEVELOPMENT SERVER
// ========================
async function run() {
  const ANSI = {
    reset: "\x1b[0m",
    cyan: "\x1b[36m",
    green: "\x1b[32m",
    red: "\x1b[31m",
    yellow: "\x1b[33m",
    magenta: "\x1b[35m",
  };

  const colors = {
    cyan: (text: string) => `${ANSI.cyan}${text}${ANSI.reset}`,
    green: (text: string) => `${ANSI.green}${text}${ANSI.reset}`,
    red: (text: string) => `${ANSI.red}${text}${ANSI.reset}`,
    yellow: (text: string) => `${ANSI.yellow}${text}${ANSI.reset}`,
    magenta: (text: string) => `${ANSI.magenta}${text}${ANSI.reset}`,
  };

  const logger = {
    info: (msg: string) => console.log(`[xo] ${colors.cyan(msg)}`),
    success: (msg: string) => console.log(`[xo] ${colors.green(msg)}`),
    error: (msg: string) => console.log(`[xo] ${colors.red(msg)}`),
    warn: (msg: string) => console.log(`[xo] ${colors.yellow(msg)}`),
    debug: (msg: string) => console.log(`[xo] ${colors.magenta(msg)}`),
  };

  // ========================
  // FILE WATCHER SETUP
  // ========================
  const watchers = [
    { baseDir: "content", watcher: watch("content", { recursive: true }) },
    { baseDir: "layouts", watcher: watch("layouts", { recursive: true }) },
    {
      baseDir: "content/_partials",
      watcher: watch("content/_partials", { recursive: true }),
    },
  ];

  async function handleFileChange(event: {
    filename?: string;
    baseDir?: string;
  }) {
    const timestamp = new Date().toISOString().split("T")[1].split(".")[0];
    if (!event.filename || !event.baseDir) return;
    const filePath = join(event.baseDir, event.filename);
    const displayPath = colors.cyan(filePath);

    logger.info(`[${timestamp}] File changed: ${displayPath}`);
    if (!(await shouldRebuild(filePath))) {
      logger.info(`[${timestamp}] Skipping rebuild for ${displayPath}`);
      return;
    }

    const affectedMdFiles = new Set<string>();

    // Add directly changed Markdown files
    if (filePath.endsWith(".md")) {
      affectedMdFiles.add(filePath);
    }

    // Find reverse dependencies
    if (reverseDependencies.has(filePath)) {
      reverseDependencies
        .get(filePath)
        ?.forEach((mdFile) => affectedMdFiles.add(mdFile));
    }

    try {
      if (affectedMdFiles.size > 0) {
        await build(Array.from(affectedMdFiles));
      } else {
        await build();
      }
      server.publish("reload", "refresh");
      logger.success(`[${timestamp}] Rebuild successful!`);
    } catch (error) {
      logger.error(
        `[${timestamp}] Rebuild failed: ${
          error instanceof Error ? error.message : String(error)
        }`,
      );
    }
  }

  // ========================
  // SERVER INITIALIZATION
  // ========================
  const server = Bun.serve({
    port: 3000,
    async fetch(req: Request): Promise<Response | undefined> {
      const url = new URL(req.url);
      const publicDir = join(process.cwd(), "public");

      if (url.pathname.startsWith("/public/")) {
        const filePath = join(publicDir, url.pathname.replace("/public/", ""));
        const file = Bun.file(filePath);
        if (await file.exists()) {
          return new Response(file);
        }
        return new Response("Not Found", { status: 404 });
      }

      if (url.pathname === "/__ws") {
        return server.upgrade(req)
          ? undefined
          : new Response("WebSocket upgrade failed", { status: 400 });
      }

      try {
        const file = Bun.file(`./dist${url.pathname}/index.html`);
        if (!(await file.exists())) {
          return new Response("Not Found", { status: 404 });
        }

        let html = await file.text();
        html = html.replace(
          "</body>",
          `<script>
                let ws = new WebSocket('ws://${req.headers.get("host")}/__ws');
                ws.onmessage = () => window.location.reload();
                ws.onclose = () => console.log("[Live Reload] Disconnected");
                setInterval(() => { if (ws.readyState !== 1) ws = new WebSocket('ws://${req.headers.get("host")}/__ws'); }, 5000);
              </script></body>`,
        );
        return new Response(html, { headers: { "Content-Type": "text/html" } });
      } catch {
        return new Response("Not Found", { status: 404 });
      }
    },
    websocket: {
      open(ws) {
        ws.subscribe("reload");
      },
      close(ws) {
        ws.unsubscribe("reload");
      },
      message: function (
        ws: ServerWebSocket<unknown>,
        message: string | Buffer,
      ): void | Promise<void> {
        throw new Error("Function not implemented.");
      },
    },
  });

  // ========================
  // STARTUP SEQUENCE
  // ========================
  console.log(
    colors.yellow(`
    █░█ █▀▀█
    ▄▀▄ █░░█
    ▀░▀ ▀▀▀▀ [v0.0.1]
  `),
  );

  // Start file watchers
  for (const { baseDir, watcher } of watchers) {
    (async () => {
      for await (const event of watcher) {
        if (event.filename) {
          handleFileChange({ filename: event.filename, baseDir });
        }
      }
    })();
  }

  // Initial build
  await build();
  logger.success(
    `Dev server running at ${colors.green("http://localhost:3000")}`,
  );
  logger.info("Watching for changes...");
}

export { run };
