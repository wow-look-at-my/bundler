#!/usr/bin/env node

import { build, typecheck } from "./commands/build.ts";
import { run } from "./commands/run.ts";
import { test } from "./commands/test.ts";
import { init } from "./commands/init.ts";

const HELP = `
ts0 - Simple TypeScript framework

Usage:
  ts0 <command> [options]

Commands:
  init              Create ts0.json and sample files
  build             Type-check and build the project
  run [file]        Build and run (or run specific file)
  test [pattern]    Run tests

Options:
  --watch, -w       Watch mode (build, test)
  --no-build        Skip build step (run)
  --force           Overwrite existing files (init)
  --help, -h        Show this help

Examples:
  ts0 init          # Initialize a new project
  ts0 build         # Type-check and build
  ts0 run           # Build and run entry point
  ts0 run src/app.ts      # Run specific file
  ts0 run --no-build      # Run without building (fast dev)
  ts0 test          # Run all tests
  ts0 test --watch  # Run tests in watch mode
`;

async function main() {
  const args = process.argv.slice(2);
  const command = args[0];

  const hasFlag = (name: string, short?: string): boolean =>
    args.includes(`--${name}`) || (short ? args.includes(`-${short}`) : false);

  const getPositional = (index: number) => {
    const filtered = args.slice(1).filter((a) => !a.startsWith("-"));
    return filtered[index - 1];
  };

  if (!command || hasFlag("help", "h")) {
    console.log(HELP);
    return;
  }

  switch (command) {
    case "init": {
      await init({ force: hasFlag("force") });
      break;
    }

    case "build": {
      console.log("Type-checking...");
      const checkResult = await typecheck();
      if (!checkResult.success) {
        console.error(checkResult.output);
        process.exit(1);
      }

      console.log("Building...");
      const result = await build({ watch: hasFlag("watch", "w") });

      if (!hasFlag("watch", "w")) {
        if (result.success) {
          console.log(`Built in ${result.duration.toFixed(0)}ms`);
        } else {
          console.error("Build failed:");
          result.errors.forEach((e) => console.error(`  ${e}`));
          process.exit(1);
        }
      }
      break;
    }

    case "run": {
      const file = getPositional(1);
      await run({
        file,
        noBuild: hasFlag("no-build"),
        args: args.slice(file ? 2 : 1).filter((a) => !a.startsWith("-")),
      });
      break;
    }

    case "test": {
      const pattern = getPositional(1);
      await test({
        pattern,
        watch: hasFlag("watch", "w"),
      });
      break;
    }

    default:
      console.error(`Unknown command: ${command}`);
      console.log(HELP);
      process.exit(1);
  }
}

main().catch((err) => {
  console.error(err);
  process.exit(1);
});
