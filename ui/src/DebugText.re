[@react.component]
let make = (~text: string) =>
  <p className="text-gray-800"> {React.string(text)} </p>;
