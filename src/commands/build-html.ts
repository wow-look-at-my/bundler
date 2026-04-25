import * as esbuild from "esbuild";
import { readFileSync, writeFileSync, mkdirSync, watch as fsWatch } from "node:fs";
import { dirname, resolve, basename, extname } from "node:path";
import type { Ts0Config } from "../config.ts";
import type { BuildResult } from "./build.ts";

export function isHtmlEntry(entry: string | undefined): boolean {
	return !!entry && entry.toLowerCase().endsWith(".html");
}

export async function buildHtml(
	config: Ts0Config,
	rootDir: string,
	options?: { watch?: boolean },
): Promise<BuildResult> {
	const startTime = performance.now();

	if (!config.entry) {
		return {
			success: false,
			outputFiles: [],
			errors: ["No entry point specified"],
			duration: performance.now() - startTime,
		};
	}

	const htmlPath = resolve(rootDir, config.entry);
	const htmlSourceDir = dirname(htmlPath);

	const outFile = config.outfile
		? resolve(rootDir, config.outfile)
		: resolve(rootDir, config.outdir || "dist", basename(config.entry));

	const buildOnce = async (): Promise<{ errors: string[] }> => {
		const html = readFileSync(htmlPath, "utf-8");
		const result = await processHtml(html, htmlSourceDir, config);
		mkdirSync(dirname(outFile), { recursive: true });
		writeFileSync(outFile, result.html);
		return { errors: result.errors };
	};

	if (options?.watch) {
		const initial = await buildOnce();
		if (initial.errors.length) {
			initial.errors.forEach((e) => console.error(`	${e}`));
		} else {
			console.log(`Wrote ${outFile}`);
		}

		const watcher = fsWatch(rootDir, { recursive: true }, (_event, filename) => {
			if (!filename) return;
			if (filename.startsWith("dist/") || filename.startsWith("node_modules/")) return;
			const ext = extname(filename).toLowerCase();
			if (![".ts", ".tsx", ".js", ".jsx", ".mjs", ".cjs", ".css", ".html"].includes(ext)) return;
			rebuild(buildOnce, filename);
		});

		process.on("SIGINT", () => {
			watcher.close();
			process.exit(0);
		});

		console.log("Watching for changes...");
		return {
			success: true,
			outputFiles: [outFile],
			errors: [],
			duration: performance.now() - startTime,
		};
	}

	const { errors } = await buildOnce();

	return {
		success: errors.length === 0,
		outputFiles: [outFile],
		errors,
		duration: performance.now() - startTime,
	};
}

let rebuildPending = false;
async function rebuild(
	buildOnce: () => Promise<{ errors: string[] }>,
	filename: string,
): Promise<void> {
	if (rebuildPending) return;
	rebuildPending = true;
	// Coalesce rapid changes from editors that write multiple times
	setTimeout(async () => {
		rebuildPending = false;
		try {
			const start = performance.now();
			const { errors } = await buildOnce();
			const ms = (performance.now() - start).toFixed(0);
			if (errors.length) {
				console.error(`Rebuild failed (${filename}):`);
				errors.forEach((e) => console.error(`	${e}`));
			} else {
				console.log(`Rebuilt in ${ms}ms (${filename})`);
			}
		} catch (err) {
			console.error(err);
		}
	}, 50);
}

interface ProcessResult {
	html: string;
	errors: string[];
}

async function processHtml(
	html: string,
	sourceDir: string,
	config: Ts0Config,
): Promise<ProcessResult> {
	const errors: string[] = [];
	const replacements: Array<{ match: string; replacement: string }> = [];

	// <script ...src="..."...></script> (paired tag with src attr)
	const scriptRe = /<script\b([^>]*)>([\s\S]*?)<\/script>/gi;
	for (const match of html.matchAll(scriptRe)) {
		const [fullMatch, attrStr] = match;
		const attrs = parseAttrs(attrStr);
		if (!attrs.src) continue;
		if (isExternal(attrs.src)) continue;

		const filePath = resolve(sourceDir, attrs.src);
		const isModule = (attrs.type || "").toLowerCase() === "module";

		try {
			const result = await esbuild.build({
				entryPoints: [filePath],
				bundle: true,
				platform: "browser",
				format: isModule ? "esm" : "iife",
				minify: config.minify,
				sourcemap: config.sourcemap ? "inline" : false,
				target: "esnext",
				write: false,
				logLevel: "silent",
				...config.esbuild,
			});

			if (result.errors.length > 0) {
				errors.push(...result.errors.map((e) => formatEsbuildMessage(e)));
				continue;
			}

			const code = result.outputFiles?.[0]?.text ?? "";
			const remainingAttrs = { ...attrs };
			delete remainingAttrs.src;
			const attrPart = formatAttrs(remainingAttrs);
			const opening = attrPart ? `<script ${attrPart}>` : `<script>`;
			replacements.push({
				match: fullMatch,
				replacement: `${opening}${escapeForScript(code)}</script>`,
			});
		} catch (err) {
			errors.push(formatBuildError(err, filePath));
		}
	}

	// <link rel="stylesheet" href="...">
	const linkRe = /<link\b([^>]*?)\/?>/gi;
	for (const match of html.matchAll(linkRe)) {
		const [fullMatch, attrStr] = match;
		const attrs = parseAttrs(attrStr);
		if ((attrs.rel || "").toLowerCase() !== "stylesheet") continue;
		if (!attrs.href) continue;
		if (isExternal(attrs.href)) continue;

		const filePath = resolve(sourceDir, attrs.href);

		try {
			const result = await esbuild.build({
				entryPoints: [filePath],
				bundle: true,
				minify: config.minify,
				sourcemap: false,
				target: "esnext",
				write: false,
				logLevel: "silent",
				loader: { ".css": "css" },
				...config.esbuild,
			});

			if (result.errors.length > 0) {
				errors.push(...result.errors.map((e) => formatEsbuildMessage(e)));
				continue;
			}

			const css = result.outputFiles?.[0]?.text ?? "";
			const remainingAttrs = { ...attrs };
			delete remainingAttrs.rel;
			delete remainingAttrs.href;
			delete remainingAttrs.type;
			const attrPart = formatAttrs(remainingAttrs);
			const opening = attrPart ? `<style ${attrPart}>` : `<style>`;
			replacements.push({
				match: fullMatch,
				replacement: `${opening}${escapeForStyle(css)}</style>`,
			});
		} catch (err) {
			errors.push(formatBuildError(err, filePath));
		}
	}

	let result = html;
	for (const { match, replacement } of replacements) {
		result = result.replace(match, () => replacement);
	}
	return { html: result, errors };
}

function isExternal(url: string): boolean {
	return /^(?:[a-z]+:|\/\/)/i.test(url);
}

function parseAttrs(attrStr: string): Record<string, string> {
	const attrs: Record<string, string> = {};
	const re = /([a-zA-Z_:][\w:-]*)(?:\s*=\s*(?:"([^"]*)"|'([^']*)'|([^\s"'=<>`]+)))?/g;
	let m: RegExpExecArray | null;
	while ((m = re.exec(attrStr)) !== null) {
		const [, name, dq, sq, uq] = m;
		attrs[name.toLowerCase()] = dq ?? sq ?? uq ?? "";
	}
	return attrs;
}

function formatAttrs(attrs: Record<string, string>): string {
	return Object.entries(attrs)
		.map(([k, v]) => (v === "" ? k : `${k}="${escapeAttr(v)}"`))
		.join(" ");
}

function escapeAttr(v: string): string {
	return v.replace(/&/g, "&amp;").replace(/"/g, "&quot;");
}

// Prevent embedded JS from prematurely closing the script tag.
function escapeForScript(code: string): string {
	return code.replace(/<\/script/gi, "<\\/script");
}

// Prevent embedded CSS content from closing the style tag.
function escapeForStyle(css: string): string {
	return css.replace(/<\/style/gi, "<\\/style");
}

function formatEsbuildMessage(msg: esbuild.Message): string {
	if (msg.location) {
		return `${msg.location.file}:${msg.location.line}:${msg.location.column}: ${msg.text}`;
	}
	return msg.text;
}

function formatBuildError(err: unknown, filePath: string): string {
	const e = err as esbuild.BuildFailure & { message?: string };
	if (e.errors?.length) {
		return e.errors.map(formatEsbuildMessage).join("\n");
	}
	return `${filePath}: ${e.message ?? String(err)}`;
}
