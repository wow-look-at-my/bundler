import { spawn } from "node:child_process";
import { join, basename } from "node:path";
import { loadConfig } from "./config.ts";
import { build } from "./build.ts";

export interface RunOptions {
  // Skip build step (use for development with --experimental-strip-types)
  noBuild?: boolean;
  // Additional arguments to pass to the script
  args?: string[];
  // Specific file to run (overrides entry)
  file?: string;
}

export async function run(options: RunOptions = {}): Promise<void> {
  const { config, rootDir } = loadConfig();

  let fileToRun: string;

  if (options.file) {
    // Run specific file
    fileToRun = join(rootDir, options.file);

    if (options.noBuild) {
      // Use Node's experimental strip-types for direct .ts execution
      await runWithNode(fileToRun, options.args || [], true);
    } else {
      // Build to temp location and run
      const result = await build();
      if (!result.success) {
        console.error("Build failed:");
        result.errors.forEach((e) => console.error(`  ${e}`));
        process.exit(1);
      }
      const outFile = join(rootDir, config.outdir, basename(options.file).replace(/\.ts$/, ".js"));
      await runWithNode(outFile, options.args || [], false);
    }
  } else {
    // Run entry point
    const entry = Array.isArray(config.entry) ? config.entry[0] : config.entry;

    if (!entry) {
      console.error("No entry point found. Specify one in ts0.json or pass a file.");
      process.exit(1);
    }

    if (options.noBuild) {
      fileToRun = join(rootDir, entry);
      await runWithNode(fileToRun, options.args || [], true);
    } else {
      const result = await build();
      if (!result.success) {
        console.error("Build failed:");
        result.errors.forEach((e) => console.error(`  ${e}`));
        process.exit(1);
      }
      fileToRun = join(rootDir, config.outdir, basename(entry).replace(/\.ts$/, ".js"));
      await runWithNode(fileToRun, options.args || [], false);
    }
  }
}

async function runWithNode(file: string, args: string[], stripTypes: boolean): Promise<void> {
  const nodeArgs = stripTypes ? ["--experimental-strip-types", file, ...args] : [file, ...args];

  const child = spawn("node", nodeArgs, {
    stdio: "inherit",
    cwd: process.cwd(),
  });

  return new Promise((resolve, reject) => {
    child.on("close", (code) => {
      if (code === 0) {
        resolve();
      } else {
        process.exit(code || 1);
      }
    });

    child.on("error", reject);
  });
}
