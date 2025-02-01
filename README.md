xo

xo is a lightweight Static Site Generator (SSG) built with Bun.js. It allows you to create and deploy static websites efficiently with built-in support for Markdown content, layout templating, and live reload during development.
Features

    Markdown Support: Automatically processes .md files with frontmatter and converts them into HTML.
    Custom Layouts: Easily define layouts for your pages.
    Partial Templates: Modularize content with partials and embed them dynamically.
    Tailwind CSS Integration: Automatically detects and uses Tailwind CSS if installed.
    Live Reload: Built-in WebSocket support for live reload during development.
    Static Asset Handling: Supports CSS, JS, and image assets (JPG, PNG, GIF, SVG).
    Fast Build Process: Powered by Bun.js for fast and efficient site generation.

Installation

To get started, clone the repository and install dependencies:

git clone https://github.com/your-username/xo.git
cd xo
bun install

Usage
Development Server

Start the development server with live reload:

bun dev

This will start the server on http://localhost:3000, watch for file changes, and automatically rebuild your site.
Build for Production

To generate the static site for production:

```sh
bun build
```
This will output the static files into the dist directory.

## Partials
You can use partials in your Markdown files. Partial templates are stored in the _partials/ folder and can be included using the following syntax:

```mustache
{{> partialName }}
```

Ensure that the partial files are named appropriately (e.g., newsletter.md).
File Structure

```sh
├── bun.build.ts           # Bun.js build configuration
├── dev.ts                 # Development server with live reload
├── content                # Content folder for your site
│   ├── blog
│   ├── index.md
│   └── _partials
├── layouts                # Layout templates (HTML files)
├── public                 # Static assets (images, styles, etc.)
├── README.md              # Project documentation
├── package.json
└── tsconfig.json
```

## Plugins
### Markdown Processor
The Markdown processor plugin parses .md files with frontmatter, processes partials, and renders Mustache templates to generate HTML content.
### Assets Plugin
The assets plugin handles static files such as images, CSS, and JS. It copies these files to the dist directory.
