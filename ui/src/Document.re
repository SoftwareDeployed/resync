[@react.component]
let make = (~children=?, ~config: Config.t, ~unit: PeriodList.Unit.t) => {
  // Suppress unused variable warnings - these are used in serialization below
  let _ = config;
  let _ = unit;

  // Serialize minimal state {config, unit} to JSON
  // Server uses Yojson, client would use Js.Json but this runs on server
  let storeJson =
    switch%platform (Runtime.platform) {
    | Server =>
      let period_list = Store.derivePeriodList(config);
      Yojson.Safe.to_string(`Assoc([
        ("inventory", `List(List.map(Config.inventory_item_to_yojson, Array.to_list(config.inventory)))),
        ("premise",
          switch (config.premise) {
          | None => `Null
          | Some(p) => Config.premise_to_yojson(p)
          }
        ),
        ("period_list", `List(List.map(Config.period_to_yojson, Array.to_list(period_list)))),
      ]))
    | Client =>
      // On client, we shouldn't be serializing here - this is server-only
      ""
    };

  <html lang="en">
    <head>
      <link rel="stylesheet" href="/style.css" />
      <script
        type_="text/json"
        id="initial-store"
        dangerouslySetInnerHTML={ "__html": storeJson }
      />
      <script type_="module" src="/app.js" />
      <meta charSet="utf-8" />
      <meta httpEquiv="X-UA-Compatible" content="IE=edge" />
      <meta name="viewport" content="width=device-width, initial-scale=1" />
      <title> "Create Reason React Tailwind"->React.string </title>
    </head>
    <body>
      <div id="root">
        {children->Belt.Option.getWithDefault(React.null)}
      </div>
    </body>
  </html>;
};
