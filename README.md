# ts0

A simple TypeScript framework with good defaults. One CLI, one config file, no boilerplate.

`ts0` wraps [esbuild](https://esbuild.github.io/), the TypeScript compiler, and Node's
built-in test runner so you can `init`, `build`, `run`, and `test` a TypeScript project
without writing a `tsconfig.json`, picking a bundler, or wiring up a test framework.

## Requirements

- Node.js **22 or newer** (uses `--experimental-strip-types` and the built-in test runner)

## Install

```sh
npm install -g ts0
```

## Quick start

```sh
mkdir my-app && cd my-app
ts0 init      # creates ts0.json, src/main.ts, src/main.test.ts, package.json
ts0 run       # build + run your app
ts0 test      # run tests
ts0 build     # produce a bundled output
```

## Commands

| Command          | What it does                                           |
| ---------------- | ------------------------------------------------------ |
| `ts0 init`       | Create `ts0.json` and starter files in the cwd         |
| `ts0 build`      | Type-check with `tsc --noEmit`, then bundle via esbuild|
| `ts0 run [file]` | Build, then run the entry point (or a specific file)   |
| `ts0 test [pat]` | Run tests via Node's built-in test runner              |

### Flags

- `--watch`, `-w` &mdash; watch mode (`build`, `test`)
- `--no-build` &mdash; skip the build step and run sources directly via `--experimental-strip-types` (`run`)
- `--force` &mdash; overwrite existing files (`init`)
- `--help`, `-h` &mdash; show help

### Examples

```sh
ts0 run                    # build and run the entry point
ts0 run src/app.ts         # run a specific file
ts0 run --no-build         # skip the bundle, run TS directly (fast dev loop)
ts0 test --watch           # watch mode tests
ts0 build --watch          # rebuild on change
```

## Configuration

`ts0` reads `ts0.json` from the current directory (or any ancestor). Every field is
optional &mdash; if there is no config file, `ts0` falls back to sensible defaults and
auto-detects an entry point from `src/main.ts`, `src/index.ts`, `main.ts`, `index.ts`,
`index.html`, or `src/index.html`.

```json
{
    "entry": "src/main.ts",
    "outfile": "dist/my-app",
    "target": "node",
    "format": "esm",
    "strict": true,
    "minify": false,
    "sourcemap": true,
    "test": {
        "pattern": "**/*.test.ts"
    }
}
```

| Field       | Type                  | Default            | Notes                                                         |
| ----------- | --------------------- | ------------------ | ------------------------------------------------------------- |
| `entry`     | `string`              | auto-detected      | Entry point relative to the config file. May be `.ts` or `.html` |
| `outfile`   | `string`              | &mdash;            | Single-file output. Adds a `#!/usr/bin/env node` shebang for JS |
| `outdir`    | `string`              | `"dist"`           | Used when `outfile` is not set                                |
| `target`    | `"node" \| "browser"` | `"node"`           | esbuild platform (ignored for HTML entries &mdash; always browser) |
| `format`    | `"esm" \| "cjs"`      | `"esm"`            | Output module format                                          |
| `strict`    | `boolean`             | `true`             | Toggles TypeScript `strict` mode for the type-check step      |
| `minify`    | `boolean`             | `false`            | Minify the bundle                                             |
| `sourcemap` | `boolean`             | `true`             | Emit a sourcemap (inlined for HTML entries)                   |
| `test.pattern` | `string`           | `"**/*.test.ts"`   | Glob for test files                                           |
| `esbuild`   | `object`              | &mdash;            | Escape hatch &mdash; merged into the esbuild options last     |

When `outfile` is set, `ts0` produces a single executable file with a Node shebang &mdash;
useful for shipping a CLI as one file. When only `outdir` is set, output goes there
preserving the entry's basename.

For Node targets, `packages: "external"` is set automatically so `node_modules` are not
bundled into the output.

### HTML entries

If `entry` ends with `.html`, `ts0 build` produces a single self-contained HTML file:
every `<script src="…">` and `<link rel="stylesheet" href="…">` referencing a local file
is bundled with esbuild and inlined into the output as `<script>…</script>` or
`<style>…</style>`. External URLs (`https://…`, `//…`, `data:…`) are left untouched.

```html
<!-- index.html -->
<link rel="stylesheet" href="./src/styles.css" />
<script type="module" src="./src/main.ts"></script>
```

```sh
ts0 build      # writes dist/index.html with the .ts and .css inlined
```

`ts0 run` is for Node entries only; it errors out when the entry is HTML. Open the
produced `dist/*.html` in a browser instead.

## How it works

- **Build:** `ts0 build` runs `tsc --noEmit` against a tsconfig generated from your
    `ts0.json`, then bundles with esbuild.
- **Run:** with `--no-build`, `ts0 run` shells out to `node --experimental-strip-types`
    to execute TypeScript directly &mdash; no build step in the dev loop.
- **Test:** test files are discovered via the configured glob and handed to
    `node --test --experimental-strip-types`.

## License

MIT
