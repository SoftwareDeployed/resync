[@react.component]
let make = (
  ~children=?,
  ~title="Create Reason React Tailwind",
  ~stylesheets=[||],
) => {
  let stylesheetNodes =
    Array.map(
      href => <link key=href rel="stylesheet" href=href />,
      stylesheets,
    )
    ->React.array;
  <head>
    <meta charSet="utf-8" />
    <meta httpEquiv="X-UA-Compatible" content="IE=edge" />
    <meta name="viewport" content="width=device-width, initial-scale=1" />
    <title> title->React.string </title>
    {stylesheetNodes}
    {switch (children) {
    | Some(value) => value
    | None => React.null
    }}
  </head>;
};
