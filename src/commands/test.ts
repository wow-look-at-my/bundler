import { spawn } from "node:child_process";
import { glob } from "node:fs/promises";
import { join } from "node:path";
import { loadConfig } from "../config.ts";

export interface TestOptions {
	pattern?: string;
	watch?: boolean;
}

export async function test(options: TestOptions = {}): Promise<void> {
	const { config, rootDir } = loadConfig();

	const pattern = options.pattern || config.test.pattern;

	// Find test files
	const testFiles: string[] = [];
	for await (const file of glob(pattern, { cwd: rootDir, exclude: (name) => name === "node_modules" })) {
		testFiles.push(file);
	}

	if (testFiles.length === 0) {
		console.log(`No test files found matching: ${pattern}`);
		return;
	}

	console.log(`Found ${testFiles.length} test file(s)\n`);

	// Use Node's built-in test runner
	const nodeArgs = [
		"--experimental-strip-types",
		"--test",
		...(options.watch ? ["--watch"] : []),
		...testFiles.map((f) => join(rootDir, f)),
	];

	const child = spawn("node", nodeArgs, {
		stdio: "inherit",
		cwd: rootDir,
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
