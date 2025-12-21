import * as esbuild from "esbuild";
import { join } from "node:path";
import { loadConfig, type Ts0Config } from "../config.ts";

export interface BuildResult {
	success: boolean;
	outputFiles: string[];
	errors: string[];
	duration: number;
}

export async function build(options?: { watch?: boolean }): Promise<BuildResult> {
	const startTime = performance.now();
	const { config, rootDir } = loadConfig();

	if (!config.entry) {
		return {
			success: false,
			outputFiles: [],
			errors: ["No entry point specified"],
			duration: performance.now() - startTime,
		};
	}

	const esbuildConfig: esbuild.BuildOptions = {
		entryPoints: [join(rootDir, config.entry)],
		bundle: true,
		platform: config.target === "node" ? "node" : "browser",
		format: config.format,
		minify: config.minify,
		sourcemap: config.sourcemap,
		target: "esnext",
		// Single file output with shebang, or directory output
		...(config.outfile
			? {
					outfile: join(rootDir, config.outfile),
					banner: { js: "#!/usr/bin/env node" },
				}
			: {
					outdir: join(rootDir, config.outdir || "dist"),
				}),
		// Node-specific settings
		...(config.target === "node" && {
			packages: "external",
		}),
		// User overrides
		...config.esbuild,
	};

	try {
		if (options?.watch) {
			const ctx = await esbuild.context(esbuildConfig);
			await ctx.watch();
			console.log("Watching for changes...");
			return {
				success: true,
				outputFiles: [],
				errors: [],
				duration: performance.now() - startTime,
			};
		}

		const result = await esbuild.build(esbuildConfig);

		const outputFiles = result.outputFiles?.map((f) => f.path) || [];

		return {
			success: result.errors.length === 0,
			outputFiles,
			errors: result.errors.map((e) => e.text),
			duration: performance.now() - startTime,
		};
	} catch (err) {
		const error = err as esbuild.BuildFailure;
		return {
			success: false,
			outputFiles: [],
			errors: error.errors?.map((e) => e.text) || [String(err)],
			duration: performance.now() - startTime,
		};
	}
}

export async function typecheck(): Promise<{ success: boolean; output: string }> {
	const { config, rootDir } = loadConfig();

	// Generate a temporary tsconfig based on ts0 config
	const tsconfigContent = {
		compilerOptions: {
			target: "ESNext",
			module: "NodeNext",
			moduleResolution: "NodeNext",
			strict: config.strict,
			noEmit: true,
			skipLibCheck: true,
			esModuleInterop: true,
			allowImportingTsExtensions: true,
		},
		include: ["**/*.ts"],
		exclude: ["node_modules", config.outdir],
	};

	const { execSync } = await import("node:child_process");

	try {
		// Write temporary tsconfig
		const { writeFileSync, unlinkSync } = await import("node:fs");
		const tempTsconfig = join(rootDir, ".ts0-tsconfig.json");
		writeFileSync(tempTsconfig, JSON.stringify(tsconfigContent, null, 2));

		try {
			const output = execSync(`npx tsc --project ${tempTsconfig}`, {
				cwd: rootDir,
				encoding: "utf-8",
				stdio: ["pipe", "pipe", "pipe"],
			});
			return { success: true, output: output || "No type errors found." };
		} finally {
			unlinkSync(tempTsconfig);
		}
	} catch (err) {
		const error = err as { stdout?: string; stderr?: string };
		return {
			success: false,
			output: error.stdout || error.stderr || String(err),
		};
	}
}
