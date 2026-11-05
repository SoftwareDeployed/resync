[@react.component]
let make = (~role: string, ~content: string, ~isStreaming=false, ~mode="streaming", ~className="") => {
  let roleClass =
    role == "assistant" ? "chat-message--assistant" : "chat-message--user";
  <div className={"chat-message " ++ roleClass ++ " " ++ className} role>
    {role == "assistant"
       ? Streamdown.make(~mode, ~isAnimating=isStreaming, ~children=content, ())
       : React.string(content)}
  </div>;
};
