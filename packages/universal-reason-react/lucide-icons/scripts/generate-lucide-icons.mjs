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
const generatedPrefix = "LucideGenerated";
const checkMode = process.argv.includes("--check");
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

function toComponentName(slug) {
  const parts = slug.split(/[^a-zA-Z0-9]+/).filter(Boolean);
  if (parts.length === 0) {
    return "IconValue";
  }
  return `Icon${parts.map(part => part[0].toUpperCase() + part.slice(1)).join("")}`;
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
  const componentNames = new Map();
  const icons = [];
  for (const slug of [...slugs].sort()) {
    const mod = await import(pathToFileURL(path.join(iconsDir, `${slug}.js`)).href);
    if (!mod.__iconNode) {
      continue;
    }
    const componentName = toComponentName(slug);
    const existingSlug = componentNames.get(componentName);
    if (existingSlug && existingSlug !== slug) {
      throw new Error(`Duplicate component name ${componentName} for ${existingSlug} and ${slug}`);
    }
    componentNames.set(componentName, slug);
    icons.push({slug, componentName, iconNode: mod.__iconNode});
  }
  return icons;
}

async function loadIconSlugs() {
  const entries = await fs.readdir(iconsDir, {withFileTypes: true});
  return entries
    .filter(entry => entry.isFile() && entry.name.endsWith(".js"))
    .map(entry => entry.name.replace(/\.js$/, ""))
    .sort();
}

function buildIconsModule(icons) {
  if (icons.length === 0) {
    return "";
  }
  const lines = [];
  for (const icon of icons) {
    lines.push(`module ${icon.componentName} = {`);
    lines.push("  let iconNode: LucideIconTypes.iconNode = [|");
    lines.push(
      ...icon.iconNode
        .map((child, index) => renderChildNode(child, index))
        .flatMap(block => block.split("\n"))
        .map(line => `  ${line}`)
    );
    lines.push("  |];");
    lines.push("");
    lines.push("  [@react.component]");
    lines.push("  let make = (");
    lines.push("    ~absoluteStrokeWidth=?,");
    lines.push("    ~ariaLabel=?,");
    lines.push("    ~className=?,");
    lines.push("    ~color=?,");
    lines.push("    ~size=?,");
    lines.push("    ~strokeWidth=?,");
    lines.push("    ~title=?,");
    lines.push("  ) =>");
    lines.push("    LucideIconRenderer.render(");
    lines.push(`      ~name=${reasonString(icon.slug)},`);
    lines.push("      ~absoluteStrokeWidth?,");
    lines.push("      ~ariaLabel?,");
    lines.push("      ~className?,");
    lines.push("      ~color?,");
    lines.push("      ~size?,");
    lines.push("      ~strokeWidth?,");
    lines.push("      ~title?,");
    lines.push("      iconNode,");
    lines.push("    );");
    lines.push("};");
    lines.push("");
  }
  return lines.join("\n");
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

async function removeStaleGeneratedFiles(directoryPath, expectedFiles) {
  const entries = await fs.readdir(directoryPath, {withFileTypes: true});
  for (const entry of entries) {
    if (!entry.isFile()) {
      continue;
    }

    const filePath = path.join(directoryPath, entry.name);
    const isGeneratedFile =
      entry.name === "modules.sexp" ||
      (entry.name.startsWith(generatedPrefix) && entry.name.endsWith(".re"));

    if (!isGeneratedFile || expectedFiles.has(filePath)) {
      continue;
    }

    if (checkMode) {
      throw new Error(`Generated file is out of date: ${path.relative(repoRoot, filePath)}`);
    }

    await fs.unlink(filePath);
  }
}

async function main() {
  const iconSlugs = await loadIconSlugs();
  const icons = await loadCanonicalIcons(iconSlugs);

  const generatedFiles = new Map();
  generatedFiles.set(path.join(jsDir, "LucideGeneratedIcons.re"), buildIconsModule(icons));

  const moduleNames = [
    ...staticModules,
    "LucideGeneratedIcons",
  ].sort();

  const moduleList = moduleNames.join("\n") + "\n";
  generatedFiles.set(path.join(jsDir, "modules.sexp"), moduleList);
  generatedFiles.set(path.join(nativeDir, "modules.sexp"), moduleList);

  for (const [filePath, content] of generatedFiles) {
    await writeFile(filePath, content);
  }

  const expectedFiles = new Set(generatedFiles.keys());
  await removeStaleGeneratedFiles(jsDir, expectedFiles);
  await removeStaleGeneratedFiles(nativeDir, expectedFiles);
}

main().catch(error => {
  console.error(error);
  process.exit(1);
});
