import { type BunPlugin, type BuildConfig } from "bun";
import { marked } from "marked";
import matter from "gray-matter";
import mustache from "mustache";
import { join, dirname, relative, basename } from "path";
import { readdir, mkdir } from "fs/promises";
import { trackDependencies } from "./deps";

const publicDir = join(process.cwd(), "public");
const distPublicDir = join(process.cwd(), "dist/public");

async function syncPublicFolder() {
  try {
    // Only copy if public dir exists
    if (await Bun.file(publicDir).exists()) {
      // Use Bun's native copy with differential sync
      await Bun.$`cp -Ru ${publicDir} ${distPublicDir}`.quiet();
    }
  } catch (error) {
    console.error("Public folder sync error:", error);
  }
}

// ========================
// MARKDOWN PROCESSING PLUGIN
// ========================
const mdPlugin: BunPlugin = {
  name: "Markdown Processor",
  setup(build) {
    build.onLoad({ filter: /\.md$/ }, async ({ path }) => {
      console.log("Processing:", path);

      // 1. Parse Frontmatter & Content
      const raw = await Bun.file(path).text();
      const { data: frontmatter, content } = matter(raw);

      // 2. Resolve Partials
      const { resolved, partialPaths } = await resolvePartials(content);

      // 3. Render Mustache Templates
      const templateData = {
        ...frontmatter,
        assets: relative(process.cwd(), join(dirname(path), "assets")),
        baseUrl: process.env.BASE_URL || "/",
      };

      const partials: Record<string, string> = {
        async: await readPartial("async"),
      };

      const rendered = mustache.render(resolved, templateData, partials);

      // 4. Convert to HTML
      const html = marked.parse(rendered);

      // 5. Apply Layout
      const layout = (frontmatter.layout as string) || "default";
      const layoutPath = join(process.cwd(), "layouts", `${layout}.html`);
      const layoutHtml = await Bun.file(layoutPath).text();

      const finalHtml = mustache.render(layoutHtml, {
        ...templateData,
        content: html,
        styles: Bun.file("./public/styles/main.css"),
        scripts: Bun.file("./public/scripts/app.js"),
      });

      // 6. Track Dependencies
      trackDependencies(path, layoutPath, partialPaths);

      // 7. Write Output
      const outputPath = path.endsWith("index.md")
        ? join("dist", "index.html")
        : join(
            "dist",
            relative("content", path).replace(/\.md$/, "/index.html"),
          );

      await mkdir(dirname(outputPath), { recursive: true });
      await Bun.write(outputPath, finalHtml);

      return { contents: finalHtml, loader: "text" };
    });
  },
};

// ========================
// PLUGIN ASSETS: Copy Static Assets
// ========================
const assetsPlugin: BunPlugin = {
  name: "Assets Plugin",
  setup(build) {
    // Only handle non-public assets
    build.onResolve({ filter: /\.(jpg|png|gif|css|js|svg)$/ }, ({ path }) => {
      if (path.startsWith(publicDir)) return { path, external: true };
      return { path, namespace: "assets" };
    });

    build.onLoad({ filter: /.*/, namespace: "assets" }, async ({ path }) => {
      const outputPath = join(
        "dist",
        relative("content", dirname(path)),
        basename(path),
      );

      // Only copy if modified
      const srcStat = (await Bun.file(path).stat()).mtimeMs;
      const destStat = await Bun.file(outputPath)
        .stat()
        .then((stat) => stat.mtimeMs)
        .catch(() => 0);

      if (srcStat > destStat) {
        await Bun.write(outputPath, await Bun.file(path).arrayBuffer());
      }

      return { contents: await Bun.file(path).text(), loader: "file" };
    });
  },
};

// ========================
// HELPER: Get All Markdown Files (Excludes _ Folders)
// ========================
async function getMarkdownFiles(dir: string): Promise<string[]> {
  const files = await readdir(dir, { withFileTypes: true });
  let paths: string[] = [];

  for (const file of files) {
    const fullPath = join(dir, file.name);

    if (file.isDirectory()) {
      if (!file.name.startsWith("_")) {
        paths = paths.concat(await getMarkdownFiles(fullPath));
      }
    } else if (file.name.endsWith(".md")) {
      paths.push(fullPath);
    }
  }

  return paths;
}

// ========================
// HELPER: Resolve Partials with Path Tracking
// ========================
async function resolvePartials(
  content: string,
): Promise<{ resolved: string; partialPaths: string[] }> {
  const partialRegex = /\{\{>\s*([\w\/.-]+)\s*\}\}/g;
  let result = content;
  const partialPaths: string[] = [];

  for (const match of content.matchAll(partialRegex)) {
    const [fullMatch, partialName] = match;
    const partialPath = join(
      process.cwd(),
      "content/_partials",
      `${partialName}.md`,
    );
    partialPaths.push(partialPath);

    const partialFile = Bun.file(partialPath);
    if (await partialFile.exists()) {
      const partial = await partialFile.text();
      result = result.replace(fullMatch, partial);
    } else {
      console.error(
        `⚠️ Warning: Partial '${partialName}' not found at ${partialPath}`,
      );
      result = result.replace(
        fullMatch,
        `<!-- Missing partial: ${partialName} -->`,
      );
    }
  }

  return { resolved: result, partialPaths };
}

// ========================
// HELPER: Read Partial File Safely
// ========================
async function readPartial(partialName: string): Promise<string> {
  const partialPath = join("content/_partials", `${partialName}.md`);
  const partialFile = Bun.file(partialPath);

  return (await partialFile.exists()) ? await partialFile.text() : "";
}

// ========================
// BUILD CONFIG
// ========================
const build = async (filesToBuild?: string[]) => {
  await syncPublicFolder();
  const mdFiles = filesToBuild ?? (await getMarkdownFiles("content"));
  await Bun.build({
    entrypoints: mdFiles,
    outdir: "./dist",
    plugins: [mdPlugin, assetsPlugin],
    publicPath: "/",
    loader: {
      ".md": "text",
      ".css": "file",
      ".js": "file",
      ".jpg": "file",
      ".png": "file",
      ".svg": "file",
    },
  } as BuildConfig);
};

export { build };

// import { type BunPlugin, type BuildConfig } from "bun";
// import { marked } from "marked";
// import matter from "gray-matter";
// import mustache from "mustache";
// import { join, dirname, relative, basename } from "path";
// import { readdir, mkdir } from "fs/promises";

// // ========================
// // MARKDOWN PROCESSING PLUGIN
// // ========================
// const mdPlugin: BunPlugin = {
//   name: "Markdown Processor",
//   setup(build) {
//     build.onLoad({ filter: /\.md$/ }, async ({ path }) => {
//       console.log("Processing:", path);

//       // 1. Parse Frontmatter & Content
//       const raw = await Bun.file(path).text();
//       const { data: frontmatter, content } = matter(raw);

//       // 2. Resolve Partials
//       const resolved = await resolvePartials(content);

//       // 3. Render Mustache Templates
//       const templateData = {
//         ...frontmatter,
//         assets: relative(process.cwd(), join(dirname(path), "assets")),
//         baseUrl: process.env.BASE_URL || "/",
//       };

//       const partials: Record<string, string> = {
//         async: await readPartial("async"),
//       };

//       const rendered = mustache.render(resolved, templateData, partials);

//       // 4. Convert to HTML
//       const html = marked.parse(rendered);

//       // 5. Apply Layout
//       const layout = (frontmatter.layout as string) || "default";
//       const layoutHtml = await Bun.file(
//         join("layouts", `${layout}.html`),
//       ).text();

//       const finalHtml = mustache.render(layoutHtml, {
//         ...templateData,
//         content: html,
//         styles: Bun.file("./public/styles/main.css"),
//         scripts: Bun.file("./public/scripts/app.js"),
//       });

//       // 6. Write Output
//       const outputPath = path.endsWith("index.md")
//         ? join("dist", "index.html")
//         : join(
//             "dist",
//             relative("content", path).replace(/\.md$/, "/index.html"),
//           );

//       await mkdir(dirname(outputPath), { recursive: true });
//       await Bun.write(outputPath, finalHtml); // ✅ Faster file writing

//       return { contents: finalHtml, loader: "text" };
//     });
//   },
// };

// // ========================
// // PLUGIN ASSETS: Copy Static Assets
// // ========================
// const assetsPlugin: BunPlugin = {
//   name: "Assets Plugin",
//   setup(build) {
//     build.onResolve({ filter: /\.(jpg|png|gif|css|js|svg)$/ }, ({ path }) => {
//       return { path, namespace: "assets" };
//     });

//     build.onLoad({ filter: /.*/, namespace: "asset" }, async ({ path }) => {
//       const file = Bun.file(path);
//       const outputPath = join(
//         "dist",
//         relative("content", dirname(path)),
//         basename(path),
//       );
//       return { contents: await file.text(), loader: "file" };
//     });
//   },
// };

// // ========================
// // HELPER: Get All Markdown Files (Excludes _ Folders)
// // ========================
// async function getMarkdownFiles(dir: string): Promise<string[]> {
//   const files = await readdir(dir, { withFileTypes: true });
//   let paths: string[] = [];

//   for (const file of files) {
//     const fullPath = join(dir, file.name);

//     // Exclude folders that start with "_"
//     if (file.isDirectory()) {
//       if (!file.name.startsWith("_")) {
//         paths = paths.concat(await getMarkdownFiles(fullPath));
//       }
//     } else if (file.name.endsWith(".md")) {
//       paths.push(fullPath);
//     }
//   }

//   return paths;
// }

// // ========================
// // HELPER: Resolve Partials (Loads Only from _partials/)
// // ========================
// async function resolvePartials(content: string): Promise<string> {
//   const partialRegex = /\{\{>\s*([\w\/.-]+)\s*\}\}/g;
//   let result = content;

//   for (const match of content.matchAll(partialRegex)) {
//     const [fullMatch, partialName] = match;

//     // Ensure partials always load from "content/_partials"
//     const partialPath = join("content/_partials", `${partialName}.md`);
//     const partialFile = Bun.file(partialPath);

//     if (await partialFile.exists()) {
//       const partial = await partialFile.text();
//       result = result.replace(fullMatch, partial);
//     } else {
//       console.error(
//         `⚠️ Warning: Partial '${partialName}' not found at ${partialPath}`,
//       );
//       result = result.replace(
//         fullMatch,
//         `<!-- Missing partial: ${partialName} -->`,
//       );
//     }
//   }

//   return result;
// }

// // ========================
// // HELPER: Read Partial File Safely
// // ========================
// async function readPartial(partialName: string): Promise<string> {
//   const partialPath = join("content/_partials", `${partialName}.md`);
//   const partialFile = Bun.file(partialPath);

//   return (await partialFile.exists()) ? await partialFile.text() : "";
// }

// // ========================
// // BUILD CONFIG
// // ========================

// const build = async (filesToBuild?: string[]) => {
//   const mdFiles = filesToBuild ?? (await getMarkdownFiles("content"));
//   await Bun.build({
//     entrypoints: mdFiles,
//     outdir: "./dist",
//     plugins: [mdPlugin, assetsPlugin],
//     publicPath: "/",
//     loader: {
//       ".md": "text",
//       ".css": "file",
//       ".js": "file",
//       ".jpg": "file",
//       ".png": "file",
//       ".svg": "file",
//     },
//   } as BuildConfig);
// };

// // ========================
// // EXPORTS
// // ========================
// export { build };
