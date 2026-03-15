[@react.component]
let make = (~serializedState="", ~stateId="initial-store", ~scripts=[||]) => {
  let scriptNodes =
    Array.map(src => <script key=src type_="module" src=src />, scripts)
    ->React.array;
  <>
    <script
      type_="text/json"
      id=stateId
      dangerouslySetInnerHTML={"__html": serializedState}
    />
    {scriptNodes}
  </>;
};
