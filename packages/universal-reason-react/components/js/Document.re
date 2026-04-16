[@react.component]
let make = (
  ~children=?,
  ~head=?,
  ~beforeMain=?,
  ~afterMain=?,
  ~title="Create Reason React Tailwind",
  ~lang="en",
  ~stylesheets=[|"/style.css"|],
  ~scripts=[|"/app.js"|],
  ~serializedState="",
  ~serializedQueries="",
  ~rootId="root",
) => {
  <DocumentHtml lang=lang>
    <DocumentHead title=title stylesheets=stylesheets>
      {switch (head) {
      | Some(value) => value
      | None => React.null
      }}
    </DocumentHead>
    <body>
      {switch (beforeMain) {
      | Some(value) => value
      | None => React.null
      }}
      <DocumentMain id=rootId>
        {switch (children) {
        | Some(value) => value
        | None => React.null
        }}
      </DocumentMain>
      {switch (afterMain) {
      | Some(value) => value
      | None => React.null
      }}
      <DocumentNextScript serializedState=serializedState scripts=scripts />
      {serializedQueries != ""
        ? <script
            type_="text/json"
            id="query-cache"
            dangerouslySetInnerHTML={"__html": serializedQueries}
          />
        : React.null}
    </body>
  </DocumentHtml>;
};
