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
      Yojson.Safe.to_string(
        `Assoc([
          ("config", Config.to_yojson(config)),
          ("unit", `String(PeriodList.Unit.tToJs(unit))),
        ])
      )
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
        dangerouslySetInnerHTML={{"__html": storeJson}}
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
