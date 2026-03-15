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
    </body>
  </DocumentHtml>;
};
