module Icon = {
  [@react.component]
  let make = (
    ~name,
    ~absoluteStrokeWidth=?,
    ~ariaLabel=?,
    ~className=?,
    ~color=?,
    ~fallback=?,
    ~size=?,
    ~strokeWidth=?,
    ~title=?,
  ) =>
    switch (LucideIconLookup.iconNodeOfName(name)) {
    | Some(iconNode) =>
      LucideIconRenderer.render(
        ~name,
        ~absoluteStrokeWidth?,
        ~ariaLabel?,
        ~className?,
        ~color?,
        ~size?,
        ~strokeWidth?,
        ~title?,
        iconNode,
      )
    | None =>
      switch (fallback) {
      | Some(element) => element
      | None => React.null
      }
    };
};

let names = LucideGeneratedNames.names;
let isValidName = LucideIconLookup.isValidName;
