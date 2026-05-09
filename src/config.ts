import { readFileSync, existsSync } from "node:fs";
import { join, dirname } from "node:path";

export interface Ts0Config {
	// Entry point - auto-detected if not specified
	entry?: string;

	// Output file (single bundled executable) or directory
	outfile?: string;
	outdir?: string;

	// Target runtime
	target: "node" | "browser";

	// Module format
	format: "esm" | "cjs";

	// TypeScript strictness
	strict: boolean;

	// Minify output
	minify: boolean;

	// Generate sourcemaps
	sourcemap: boolean;

	// Test configuration
	test: {
		pattern: string;
	};

	// HTML entries only: embed runtime-fetched assets (shaders, .hdr, .glb,
	// images, .json, …) into a window.fetch interceptor at the top of
	// <head>. Default true; set false to skip the interceptor entirely
	// (e.g. when the bundle will only ever be served from a real origin
	// where the asset tree is reachable).
	embedAssets?: boolean;

	// HTML entries only: directories to scan for embeddable assets, relative
	// to the config file (rootDir). When set, ONLY these directories are
	// scanned (instead of the HTML entry's directory). Asset keys in the
	// fetch interceptor are relative to rootDir, so fetch("people/foo.xml")
	// matches assetDirs: ["people"].
	assetDirs?: string[];

	// Additional esbuild options (escape hatch)
	esbuild?: Record<string, unknown>;
}

const DEFAULT_CONFIG: Ts0Config = {
	outdir: "dist",
	target: "node",
	format: "esm",
	strict: true,
	minify: false,
	sourcemap: true,
	test: {
		pattern: "**/*.test.ts",
	},
};

export function findConfigFile(startDir: string = process.cwd()): string | null {
	let dir = startDir;
	while (dir !== dirname(dir)) {
		const configPath = join(dir, "ts0.json");
		if (existsSync(configPath)) {
			return configPath;
		}
		dir = dirname(dir);
	}
	return null;
}

export function loadConfig(configPath?: string): { config: Ts0Config; rootDir: string } {
	const foundPath = configPath || findConfigFile();

	if (!foundPath || !existsSync(foundPath)) {
		// No config file - use defaults and auto-detect entry
		const rootDir = process.cwd();
		const config = { ...DEFAULT_CONFIG };
		config.entry = autoDetectEntry(rootDir);
		return { config, rootDir };
	}

	const rootDir = dirname(foundPath);
	const userConfig = JSON.parse(readFileSync(foundPath, "utf-8"));

	const config: Ts0Config = {
		...DEFAULT_CONFIG,
		...userConfig,
		test: {
			...DEFAULT_CONFIG.test,
			...userConfig.test,
		},
	};

	// Auto-detect entry if not specified
	if (!config.entry) {
		config.entry = autoDetectEntry(rootDir);
	}

	return { config, rootDir };
}

function autoDetectEntry(rootDir: string): string {
	const candidates = [
		"src/main.ts",
		"src/index.ts",
		"main.ts",
		"index.ts",
		"index.html",
		"src/index.html",
	];

	for (const candidate of candidates) {
		if (existsSync(join(rootDir, candidate))) {
			return candidate;
		}
	}

	return "src/main.ts"; // Default even if doesn't exist yet
}

export function getDefaultConfig(): Ts0Config {
	return { ...DEFAULT_CONFIG };
}
