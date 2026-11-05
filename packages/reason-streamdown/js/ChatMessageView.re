[@react.component]
let make = (~role: string, ~content: string, ~isStreaming=false, ~className="") => {
  let roleClass =
    role == "assistant" ? "chat-message--assistant" : "chat-message--user";
  <div className={"chat-message " ++ roleClass ++ " " ++ className} role>
    {role == "assistant"
       ? Streamdown.make(~isAnimating=isStreaming, ~children=content, ())
       : React.string(content)}
  </div>;
};
