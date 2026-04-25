import { greeting } from "./greet.ts";

const greetingEl = document.getElementById("greeting");
const countEl = document.getElementById("count");
const bumpEl = document.getElementById("bump");

if (greetingEl) {
	greetingEl.textContent = greeting("ts0");
}

let count = 0;
bumpEl?.addEventListener("click", () => {
	count += 1;
	if (countEl) countEl.textContent = String(count);
});
