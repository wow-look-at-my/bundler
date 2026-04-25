# CLAUDE.md

Notes for Claude working in this repository.

## What this project is

`ts0` &mdash; a small TypeScript framework CLI that wraps esbuild, the TypeScript
compiler, and Node's built-in test runner. The repository directory is named
`bundler`; the published package and binary are both named `ts0`.

The CLI exposes four commands: `init`, `build`, `run`, `test`. See `README.md` for
user-facing documentation.

## Layout

```
src/
    cli.ts              # entry point - arg parsing and command dispatch
    config.ts           # ts0.json loader, defaults, entry auto-detection
    commands/
        init.ts         # scaffolds ts0.json + src/ + package.json
        build.ts        # tsc --noEmit, then esbuild bundle (dispatches HTML)
        build-html.ts   # bundles an .html entry into a single inlined .html
        run.ts          # build (or strip-types) + node
        test.ts         # node --test on discovered test files
samples/
    basic/              # Node-entry smoke-test sample exercised by CI
    html/               # HTML-entry smoke-test sample exercised by CI
.github/workflows/ci.yml
ts0.json                # ts0 builds itself with these settings
```

`ts0` is self-hosting: `package.json`'s `build` script invokes the CLI from
source via `node --experimental-strip-types src/cli.ts build`, which reads the
repo's own `ts0.json` and produces `dist/ts0`.

## Runtime requirements

- Node.js **22 or newer**. The CLI relies on `--experimental-strip-types` and
    the built-in `node --test` runner, both Node 22+ features. Do not lower the
    `engines.node` field.
- This is a Node/TypeScript project. **Do not** apply the `go-toolchain` rules
    here &mdash; they do not apply.

## Working on the code

```sh
npm install
npm run build                                       # build dist/ts0
node --experimental-strip-types src/cli.ts <cmd>    # run from source without building
```

There is currently no unit test suite for `ts0` itself. CI exercises the CLI
end-to-end by:

1. Building `dist/ts0` from source.
2. `npm link`ing it.
3. Running `ts0 init`, `build`, `run`, `test` against a fresh tmp project.
4. Running `ts0 build` and `ts0 test` against `samples/basic`.
5. Running `ts0 build` against `samples/html` and asserting the bundled JS/CSS
    are inlined into a single `dist/index.html`.

If you change CLI behavior, update the relevant `samples/*` and CI smoke steps so
the new behavior is covered.

## Conventions

- **Indentation:** tabs (4-wide), per `.editorconfig`. Match this in every file.
- **Modules:** ESM only (`"type": "module"`). Use `.ts` extensions in relative
    imports (e.g. `import { build } from "./commands/build.ts"`) so
    `--experimental-strip-types` resolves them.
- **Dependencies:** keep them minimal. Production deps are `esbuild` and
    `typescript`; `@types/node` is the only dev dep. Don't add a CLI parser, a
    test framework, or a bundler abstraction &mdash; the whole point is that ts0
    stays small.
- **Argument parsing:** `src/cli.ts` parses `process.argv` by hand. New commands
    should follow the same pattern (no new dependency).

## Configuration model

`config.ts` defines `Ts0Config` and a single `DEFAULT_CONFIG` object. When
adding a new option:

1. Add the field to the `Ts0Config` interface and `DEFAULT_CONFIG`.
2. If it changes build behavior, thread it through the `esbuildConfig` object
    in `commands/build.ts`. The user-supplied `esbuild` field is spread last so
    it stays an escape hatch &mdash; keep it that way.
3. Document it in `README.md`'s configuration table.

`loadConfig()` walks up from the cwd looking for `ts0.json` and falls back to
defaults plus auto-detected entry. Don't break the no-config-file path.

## Type-checking

`commands/build.ts` writes a temporary `.ts0-tsconfig.json` (gitignored), runs
`tsc --noEmit` against it, and deletes it in a `finally`. The TypeScript binary
is resolved from `ts0`'s own `node_modules` via `createRequire` so the user's
project doesn't need its own `typescript` install. Preserve both behaviors.

## HTML entries

When `entry` ends with `.html`, `build.ts` delegates to `commands/build-html.ts`.
That module reads the HTML, finds `<script src="…">` and `<link rel="stylesheet"
href="…">` references that point at local files, runs `esbuild` on each, and
inlines the bundled output as `<script>…</script>` / `<style>…</style>`.
External URLs (`https://`, `//`, `data:`, etc.) are left alone. Tag attributes
on the script/link (other than `src`/`href`/`rel`/`type`) are preserved.

Keep the HTML parser regex-based and dependency-free &mdash; do not add an HTML
parser package.

## Documentation

Per global instructions: when you change project structure, commands, config
fields, or tooling, update `README.md` and this file in the same commit. Don't
let the docs drift.

## Git workflow

- Develop on the branch the task specifies.
- Commit and push frequently; the VM can reset.
- PRs in this org are squash-merged &mdash; don't rebase or force-push.
