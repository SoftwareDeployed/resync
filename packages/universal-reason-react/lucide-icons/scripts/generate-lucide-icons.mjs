import fs from "node:fs/promises";
import path from "node:path";
import {fileURLToPath, pathToFileURL} from "node:url";

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const packageDir = path.resolve(__dirname, "..");
const repoRoot = path.resolve(packageDir, "../../..");
const jsDir = path.join(packageDir, "js");
const nativeDir = path.join(packageDir, "native");
const lucideDir = path.join(repoRoot, "node_modules", "lucide-react", "dist", "esm");
const iconsDir = path.join(lucideDir, "icons");
const dynamicImportsPath = path.join(lucideDir, "dynamicIconImports.js");

const generatedPrefix = "LucideGenerated";
const checkMode = process.argv.includes("--check");
const chunkKeys = [
  "0_9",
  "A",
  "B",
  "C",
  "D",
  "E",
  "F",
  "G",
  "H",
  "I",
  "J",
  "K",
  "L",
  "M",
  "N",
  "O",
  "P",
  "Q",
  "R",
  "S",
  "T",
  "U",
  "V",
  "W",
  "X",
  "Y",
  "Z",
];
const reservedIdentifiers = new Set([
  "and",
  "as",
  "assert",
  "begin",
  "class",
  "constraint",
  "do",
  "done",
  "downto",
  "else",
  "end",
  "exception",
  "external",
  "false",
  "for",
  "fun",
  "function",
  "functor",
  "if",
  "in",
  "include",
  "inherit",
  "initializer",
  "lazy",
  "let",
  "match",
  "method",
  "module",
  "mutable",
  "new",
  "nonrec",
  "object",
  "of",
  "open",
  "or",
  "private",
  "rec",
  "sig",
  "struct",
  "switch",
  "then",
  "to",
  "true",
  "try",
  "type",
  "val",
  "virtual",
  "when",
  "while",
  "with",
]);

const staticModules = [
  "Lucide",
  "LucideIconLookup",
  "LucideIconRenderer",
  "LucideIconTypes",
];

const attrConstructors = {
  cx: "Cx",
  cy: "Cy",
  d: "D",
  fill: "Fill",
  height: "Height",
  points: "Points",
  r: "R",
  rx: "Rx",
  ry: "Ry",
  width: "Width",
  x: "X",
  x1: "X1",
  x2: "X2",
  y: "Y",
  y1: "Y1",
  y2: "Y2",
};

const tagConstructors = {
  circle: "Circle",
  ellipse: "Ellipse",
  line: "Line",
  path: "Path",
  polygon: "Polygon",
  polyline: "Polyline",
  rect: "Rect",
};

function reasonString(value) {
  return JSON.stringify(value);
}

function chunkKey(name) {
  const first = name[0];
  return /[0-9]/.test(first) ? "0_9" : first.toUpperCase();
}

function toIdentifier(slug) {
  const base = slug.replace(/[^a-zA-Z0-9]+/g, "_").replace(/^_+|_+$/g, "");
  if (base.length === 0) {
    return "icon_value";
  }
  if (!/^[A-Za-z_]/.test(base)) {
    return `icon_${base}`;
  }
  if (reservedIdentifiers.has(base)) {
    return `${base}_icon`;
  }
  return base;
}

function renderChildNode(child, index) {
  const [tag, attrs] = child;
  const tagConstructor = tagConstructors[tag];
  if (!tagConstructor) {
    throw new Error(`Unsupported tag: ${tag}`);
  }
  const key = attrs.key ?? `${tag}-${index}`;
  const attrLines = Object.entries(attrs)
    .filter(([name]) => name !== "key")
    .map(([name, value]) => {
      const constructor = attrConstructors[name];
      if (!constructor) {
        throw new Error(`Unsupported attr: ${name}`);
      }
      return `      LucideIconTypes.${constructor}(${reasonString(value)}),`;
    });
  return [
    "  {",
    `    key: ${reasonString(key)},`,
    `    tag: LucideIconTypes.${tagConstructor},`,
    "    attrs: [|",
    ...attrLines,
    "    |],",
    "  },",
  ].join("\n");
}

async function loadCanonicalIcons(slugs) {
  const icons = [];
  for (const slug of [...slugs].sort()) {
    const mod = await import(pathToFileURL(path.join(iconsDir, `${slug}.js`)).href);
    if (!mod.__iconNode) {
      throw new Error(`Icon module missing __iconNode: ${slug}`);
    }
    icons.push({slug, identifier: toIdentifier(slug), iconNode: mod.__iconNode});
  }
  return icons;
}

async function loadNameMappings() {
  const source = await fs.readFile(dynamicImportsPath, "utf8");
  const pattern = /^\s*"([^"]+)": \(\) => import\('\.\/icons\/([^']+)\.js'\),$/gm;
  const mappings = [];
  let match;
  while ((match = pattern.exec(source)) !== null) {
    mappings.push({name: match[1], slug: match[2]});
  }
  return mappings;
}

function buildGeneratedNamesModule(names) {
  const lines = names.map(name => `  ${reasonString(name)},`);
  return ["let names = [|", ...lines, "|];", ""].join("\n");
}

function buildNodesModule(chunkIcons) {
  if (chunkIcons.length === 0) {
    return "";
  }
  const lines = [];
  for (const icon of chunkIcons) {
    lines.push(`let ${icon.identifier}: LucideIconTypes.iconNode = [|`);
    lines.push(...icon.iconNode.map((child, index) => renderChildNode(child, index)).flatMap(block => block.split("\n")));
    lines.push("|];");
    lines.push("");
  }
  return lines.join("\n");
}

function buildLookupModule(chunkMappings, iconBySlug) {
  const lines = ["let get = name =>", "  switch (name) {"];
  for (const mapping of chunkMappings) {
    const icon = iconBySlug.get(mapping.slug);
    if (!icon) {
      throw new Error(`Missing canonical icon for slug ${mapping.slug}`);
    }
    const targetChunk = chunkKey(icon.slug);
    lines.push(
      `  | ${reasonString(mapping.name)} => Some(LucideGeneratedNodes_${targetChunk}.${icon.identifier})`
    );
  }
  lines.push("  | _ => None");
  lines.push("  };");
  lines.push("");
  return lines.join("\n");
}

function chunkEntries(entries, keyOf) {
  const chunkMap = new Map();
  for (const entry of entries) {
    const key = keyOf(entry);
    const values = chunkMap.get(key) ?? [];
    values.push(entry);
    chunkMap.set(key, values);
  }
  return [...chunkMap.entries()].sort(([left], [right]) => left.localeCompare(right));
}

async function writeFile(targetPath, content) {
  if (checkMode) {
    const current = await fs.readFile(targetPath, "utf8").catch(() => null);
    if (current !== content) {
      throw new Error(`Generated file is out of date: ${path.relative(repoRoot, targetPath)}`);
    }
    return;
  }
  await fs.writeFile(targetPath, content, "utf8");
}

async function main() {
  const mappings = await loadNameMappings();
  const icons = await loadCanonicalIcons(new Set(mappings.map(mapping => mapping.slug)));
  const iconBySlug = new Map(icons.map(icon => [icon.slug, icon]));
  const iconChunkMap = new Map(chunkEntries(icons, icon => chunkKey(icon.slug)));
  const mappingChunkMap = new Map(chunkEntries(mappings, mapping => chunkKey(mapping.name)));

  const generatedFiles = new Map();
  generatedFiles.set(path.join(jsDir, "LucideGeneratedNames.re"), buildGeneratedNamesModule(mappings.map(mapping => mapping.name)));

  for (const chunk of chunkKeys) {
    const chunkIcons = iconChunkMap.get(chunk) ?? [];
    generatedFiles.set(
      path.join(jsDir, `LucideGeneratedNodes_${chunk}.re`),
      buildNodesModule(chunkIcons)
    );
  }

  for (const chunk of chunkKeys) {
    const chunkMappings = mappingChunkMap.get(chunk) ?? [];
    generatedFiles.set(
      path.join(jsDir, `LucideGeneratedLookup_${chunk}.re`),
      buildLookupModule(chunkMappings, iconBySlug)
    );
  }

  const moduleNames = [
    ...staticModules,
    "LucideGeneratedNames",
    ...chunkKeys.map(chunk => `LucideGeneratedNodes_${chunk}`),
    ...chunkKeys.map(chunk => `LucideGeneratedLookup_${chunk}`),
  ].sort();

  const moduleList = moduleNames.join("\n") + "\n";
  generatedFiles.set(path.join(jsDir, "modules.sexp"), moduleList);
  generatedFiles.set(path.join(nativeDir, "modules.sexp"), moduleList);

  for (const [filePath, content] of generatedFiles) {
    await writeFile(filePath, content);
  }
}

main().catch(error => {
  console.error(error);
  process.exit(1);
});
