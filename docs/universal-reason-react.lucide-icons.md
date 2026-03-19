# universal-reason-react/lucide-icons

> ⚠️ **API Stability**: APIs are **not stable** and are **subject to change**.


Universal Lucide icon rendering for server and client Reason React applications.

## Overview

This package provides a shared icon layer that renders identical SVG markup on both server and client, preventing hydration mismatches and ensuring consistent icon display across your universal application.

## Features

- **Universal Rendering**: Same SVG output on server and client
- **Type-Safe**: Full type coverage for all Lucide icons
- **Customizable**: Size, color, stroke width, and className props
- **Tree-Shakeable**: Only includes icons you use
- **Accessible**: Proper ARIA attributes by default

## Installation

Add to your `dune` file:

```lisp
(libraries
  universal_reason_react_lucide_icons_native  ; For server
  universal_reason_react_lucide_icons_js      ; For client
  server-reason-react.react
  reason-react)
```

## Quick Start

### Basic Usage

```reason
// Using a specific icon
open UniversalReasonReactLucideIcons;

[@react.component]
let make = () => {
  <div>
    <Home size=24 color="#333" />
    <User size={20} strokeWidth={1.5} />
    <Settings className="icon-spin" />
  </div>;
};
```

### Available Icons

All Lucide icons are available as React components:

```reason
// Navigation
<ArrowLeft />
<ArrowRight />
<ChevronDown />
<Menu />
<X />
<Search />

// Actions
<Plus />
<Minus />
<Edit />
<Trash2 />
<Check />
<XCircle />
<Save />

// Status
<AlertCircle />
<CheckCircle />
<Info />
<AlertTriangle />
<Loader />

// Objects
<Home />
<User />
<Settings />
<Mail />
<Calendar />
<File />
<Folder />
<Image />

// Social
<Github />
<Twitter />
<Linkedin />
<Globe />
```

## Props

All icon components accept the following props:

| Prop | Type | Default | Description |
|------|------|---------|-------------|
| `size` | `int` | `24` | Width and height in pixels |
| `color` | `string` | `"currentColor"` | Icon color (CSS color value) |
| `strokeWidth` | `float` | `2.0` | SVG stroke width |
| `className` | `string` | `""` | Additional CSS classes |
| `ariaLabel` | `string` | `icon name` | Accessible label |

## Usage Examples

### Button with Icon

```reason
[@react.component]
let make = (~onClick, ~children) => {
  <button onClick className="btn btn-primary">
    <Plus size={16} className="btn-icon" />
    {React.string(" ")}
    children
  </button>;
};
```

### Navigation Item

```reason
[@react.component]
let make = (~to_, ~icon: module(LucideIcon), ~label) => {
  let iconModule = (val icon: module(LucideIcon));
  
  <UniversalRouter.Link to_ className="nav-link">
    <iconModule size={20} />
    <span className="nav-label"> {React.string(label)} </span>
  </UniversalRouter.Link>;
};

// Usage
<NavItem to_="/home" icon=(module Home) label="Home" />
<NavItem to_="/settings" icon=(module Settings) label="Settings" />
```

### Dynamic Icons

```reason
[@react.component]
let make = (~status) => {
  let (icon, color) =
    switch (status) {
    | `Success => (<CheckCircle color="#22c55e" />, "success")
    | `Error => (<XCircle color="#ef4444" />, "error")
    | `Warning => (<AlertTriangle color="#f59e0b" />, "warning")
    | `Loading => (<Loader className="animate-spin" />, "loading")
    };
    
  <div className={"status-badge " ++ color}>
    icon
    <span> {React.string(statusToString(status))} </span>
  </div>;
};
```

### Icon Sizes

```reason
// Size variants
module IconSize = {
  let xs = 12;
  let sm = 16;
  let md = 24;
  let lg = 32;
  let xl = 48;
};

[@react.component]
let make = (~size=`md) => {
  let sizePx =
    switch (size) {
    | `xs => IconSize.xs
    | `sm => IconSize.sm
    | `md => IconSize.md
    | `lg => IconSize.lg
    | `xl => IconSize.xl
    };
    
  <Home size=sizePx />;
};
```

### Accessible Icons

```reason
// Decorative icon (hidden from screen readers)
<Home ariaLabel="" />

// Standalone icon with label
<Home ariaLabel="Home page" />

// Icon with visible text
<button>
  <Home size={16} ariaLabel="" />  {/* Decorative */}
  <span> {React.string("Go home")} </span>
</button>
```

## Best Practices

### 1. Consistent Sizing

Define size constants for your app:

```reason
module Icon = {
  let inline = 14;
  let button = 16;
  let nav = 20;
  let feature = 32;
};
```

### 2. Semantic Icons

Choose icons that convey meaning:

```reason
// Good: Trash icon for delete
<Trash2 onClick={handleDelete} />

// Good: Pencil for edit
<Edit onClick={handleEdit} />

// Bad: Random icon choice
<Star onClick={handleDelete} />  // Confusing!
```

### 3. Loading States

Use the Loader icon with animation:

```reason
let (isLoading, setIsLoading) = React.useState(() => false);

<button disabled=isLoading>
  {isLoading
    ? <Loader className="animate-spin" size={16} />
    : <Save size={16} />}
  {React.string(isLoading ? " Saving..." : " Save")}
</button>
```

### 4. Color Inheritance

Use `currentColor` for flexible theming:

```reason
/* CSS */
.btn-primary .lucide {
  color: white;
}

.btn-secondary .lucide {
  color: #333;
}

/* Component */
<Home />  /* Will inherit color from parent */
```

## Common Patterns

### Icon Button

```reason
// components/IconButton.re
[@react.component]
let make = (~icon, ~onClick, ~title, ~variant=`default) => {
  let className =
    "icon-btn icon-btn-" ++ variantToString(variant);
    
  <button
    onClick
    title
    className
    aria-label=title>
    icon
  </button>;
};

// Usage
<IconButton
  icon={<Trash2 size={18} />}
  onClick={handleDelete}
  title="Delete item"
  variant=`danger
/>
```

### Breadcrumb Navigation

```reason
// components/Breadcrumbs.re
[@react.component]
let make = (~items) => {
  <nav aria-label="Breadcrumb">
    <ol className="breadcrumb">
      {items
       |> List.mapi((index, item) =>
         <li key={string_of_int(index)} className="breadcrumb-item">
           {index > 0
             ? <ChevronRight size={14} className="breadcrumb-separator" />
             : React.null}
           <UniversalRouter.Link to_={item.href}>
             {React.string(item.label)}
           </UniversalRouter.Link>
         </li>
       )
       |> React.list}
    </ol>
  </nav>;
};
```

### Empty State

```reason
// components/EmptyState.re
[@react.component]
let make = (~icon, ~title, ~description, ~action=?) => {
  <div className="empty-state">
    <div className="empty-state-icon">
      {React.cloneElement(
        icon,
        ~props={"size": 48, "strokeWidth": 1.5},
        [||]
      )}
    </div>
    <h3 className="empty-state-title"> {React.string(title)} </h3>
    <p className="empty-state-description">
      {React.string(description)}
    </p>
    {switch (action) {
     | Some((label, onClick)) =>
       <button onClick className="btn btn-primary">
         {React.string(label)}
       </button>
     | None => React.null
     }}
  </div>;
};

// Usage
<EmptyState
  icon={<Inbox />}
  title="No messages"
  description="Your inbox is empty"
  action=("Compose", handleCompose)
/>
```

## Troubleshooting

### Icons Not Rendering

**Problem:** Icons show as empty boxes or don't appear

**Solutions:**
1. Check that correct library is used (native vs js)
2. Verify icon name spelling (case-sensitive)
3. Ensure CSS isn't hiding SVG elements

### Hydration Mismatch

**Problem:** Server and client render different icon markup

**Solutions:**
1. Use the universal icons package (not plain Lucide)
2. Avoid dynamic icon selection during SSR
3. Ensure icon props are deterministic

### Size Issues

**Problem:** Icons render too large or small

**Solutions:**
1. Explicitly set `size` prop
2. Check parent container doesn't constrain SVG
3. Verify CSS isn't overriding dimensions

## Icon Reference

### Full Icon List

For a complete list of available icons, see the [Lucide Icons documentation](https://lucide.dev/icons/).

All icons follow the naming convention:
- PascalCase in Reason (e.g., `ArrowLeft`, `CheckCircle`)
- Kebab-case in original Lucide (e.g., `arrow-left`, `check-circle`)

### Common Icons by Category

**Navigation:**
- `ArrowLeft`, `ArrowRight`, `ArrowUp`, `ArrowDown`
- `ChevronLeft`, `ChevronRight`, `ChevronUp`, `ChevronDown`
- `Menu`, `X`, `Search`, `Home`

**Actions:**
- `Plus`, `Minus`, `Edit`, `Trash2`, `Copy`
- `Check`, `XCircle`, `Save`, `Download`, `Upload`

**Status:**
- `AlertCircle`, `CheckCircle`, `Info`, `AlertTriangle`
- `Loader`, `RefreshCw`, `Activity`

**Objects:**
- `User`, `Users`, `Settings`, `Mail`, `Calendar`
- `File`, `Folder`, `Image`, `Music`, `Video`

## Related Documentation

- [Lucide Icons](https://lucide.dev/) - Official Lucide documentation
- [universal-reason-react/components](universal-reason-react.components.md) - Universal component patterns
- [universal-reason-react/router](universal-reason-react.router.md) - Navigation integration
