[@react.component]
let make = (~children=?, ~id="root") => {
  <div id=id>
    {switch (children) {
    | Some(value) => value
    | None => React.null
    }}
  </div>;
};
