import { writeFileSync, existsSync, mkdirSync } from "node:fs";
import { execSync } from "node:child_process";
import { join, basename } from "node:path";

const DEFAULT_TS0_CONFIG = {
	entry: "src/main.ts",
	outdir: "dist",
	target: "node",
	format: "esm",
	strict: true,
};

const SAMPLE_MAIN = `// Your application entry point

export function main() {
	console.log("Hello from ts0!");
}

main();
`;

const SAMPLE_TEST = `import { test } from "node:test";
import { strict as assert } from "node:assert";

test("example test", () => {
	assert.equal(1 + 1, 2);
});
`;

function createPackageJson(name: string): object {
	return {
		name,
		version: "0.1.0",
		type: "module",
		devDependencies: {
			"@types/node": "^22.0.0",
		},
	};
}

export async function init(options: { force?: boolean } = {}): Promise<void> {
	const rootDir = process.cwd();
	const configPath = join(rootDir, "ts0.json");

	if (existsSync(configPath) && !options.force) {
		console.log("ts0.json already exists. Use --force to overwrite.");
		return;
	}

	// Create ts0.json
	writeFileSync(configPath, JSON.stringify(DEFAULT_TS0_CONFIG, null, 2) + "\n");
	console.log("Created ts0.json");

	// Create src directory if needed
	const srcDir = join(rootDir, "src");
	if (!existsSync(srcDir)) {
		mkdirSync(srcDir, { recursive: true });
		console.log("Created src/");
	}

	// Create sample main.ts if doesn't exist
	const mainPath = join(srcDir, "main.ts");
	if (!existsSync(mainPath)) {
		writeFileSync(mainPath, SAMPLE_MAIN);
		console.log("Created src/main.ts");
	}

	// Create sample test if doesn't exist
	const testPath = join(srcDir, "main.test.ts");
	if (!existsSync(testPath)) {
		writeFileSync(testPath, SAMPLE_TEST);
		console.log("Created src/main.test.ts");
	}

	// Create package.json if doesn't exist
	const packageJsonPath = join(rootDir, "package.json");
	if (!existsSync(packageJsonPath)) {
		const projectName = basename(rootDir);
		writeFileSync(packageJsonPath, JSON.stringify(createPackageJson(projectName), null, 2) + "\n");
		console.log("Created package.json");

		// Install dependencies
		console.log("Installing dependencies...");
		try {
			execSync("npm install", { cwd: rootDir, stdio: "inherit" });
		} catch {
			console.error("Failed to install dependencies. Run 'npm install' manually.");
		}
	}

	console.log("\nReady! Try these commands:");
	console.log("	ts0 run		 - Run your app");
	console.log("	ts0 build	 - Build for production");
	console.log("	ts0 test		- Run tests");
}
