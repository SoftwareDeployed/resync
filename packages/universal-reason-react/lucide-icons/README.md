# universal-reason-react/lucide-icons

Universal Lucide icon components for Reason React.

This package generates a Reason component for each Lucide icon from
`lucide-react` and renders the same SVG structure on both Melange and
native/server builds.

## Usage

```reason
<Lucide.IconSearch
  size=24
  className="text-slate-400"
  color="currentColor"
  strokeWidth=2
/>
```

## Props

- `size`: icon width and height, defaults to `24`
- `color`: stroke color, defaults to `currentColor`
- `className`: additional classes merged onto `lucide`
- `strokeWidth`: stroke width, defaults to `2`
- `absoluteStrokeWidth`: matches Lucide's absolute stroke width behavior
- `title`: optional accessibility title
- `ariaLabel`: optional accessibility label

## Generated Files

- `js/LucideGeneratedIcons.re` contains generated icon component modules

## Regenerating

```sh
pnpm --dir packages/universal-reason-react/lucide-icons generate
pnpm --dir packages/universal-reason-react/lucide-icons check:generated
```

The generator reads `lucide-react` from the workspace `node_modules` and keeps
the checked-in Reason registry in sync with the installed version.

## License

Generated from `lucide-react` and distributed with the upstream Lucide license
in `LICENSE`.
