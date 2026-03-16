[@react.component]
let make = (~children=?, ~lang="en") => {
  <html lang=lang>
    {switch (children) {
    | Some(value) => value
    | None => React.null
    }}
  </html>;
};
