import type { PathLike } from "fs";

export interface XOConfig {
  contentDir: PathLike;
  layoutDir: PathLike;
  partialsDir: PathLike;
  distDir: string;
  port?: number;
}

export const defaultConfig: XOConfig = {
  contentDir: "content",
  layoutDir: "layouts",
  partialsDir: "content/_partials",
  distDir: "dist",
  port: 3000,
};
