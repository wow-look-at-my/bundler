import { glob as fsGlob } from "node:fs/promises";

export async function glob(pattern: string, rootDir: string): Promise<string[]> {
  const results: string[] = [];

  for await (const file of fsGlob(pattern, { cwd: rootDir, exclude: (name) => name === "node_modules" })) {
    results.push(file);
  }

  return results.sort();
}
