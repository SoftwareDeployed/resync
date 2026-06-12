let streamText = (streaming: LlmChatStore.streaming_state, id) =>
  streaming.LlmChatStore.activeStreams
  ->Belt.Map.String.get(id)
  ->Belt.Option.getWithDefault("");

let assertCurrentStream = (~label, ~streaming, ~expected) =>
  switch (streaming.LlmChatStore.currentStreamId) {
  | Some(actual) =>
    Alcotest.check(Alcotest.string, label, expected, actual)
  | None => Alcotest.fail(label ++ " was None")
  };

let overlappingStreams = () => {
  let threadId = "thread-1";
  let startedFirst =
    LlmChatStore.reduceStream(
      LlmChatStore.emptyStreamingState,
      LlmChatStore.StreamStarted(threadId, "stream-1"),
    );
  let firstToken =
    LlmChatStore.reduceStream(
      startedFirst,
      LlmChatStore.TokenReceived(threadId, "stream-1", "first"),
    );
  let startedSecond =
    LlmChatStore.reduceStream(
      firstToken,
      LlmChatStore.StreamStarted(threadId, "stream-2"),
    );
  LlmChatStore.reduceStream(
    startedSecond,
    LlmChatStore.TokenReceived(threadId, "stream-2", "second"),
  );
};

let suite =
  (
    "LlmChatStoreStreaming",
    [
      Alcotest.test_case("complete for non-current stream preserves current stream", `Quick, () => {
        let streaming =
          LlmChatStore.reduceStream(
            overlappingStreams(),
            LlmChatStore.StreamComplete("thread-1", "stream-1"),
          );
        Alcotest.check(
          Alcotest.bool,
          "still streaming",
          true,
          streaming.LlmChatStore.isStreaming,
        );
        assertCurrentStream(~label="current stream id", ~streaming, ~expected="stream-2");
        Alcotest.check(
          Alcotest.string,
          "first text retained",
          "first",
          streamText(streaming, "stream-1"),
        );
        Alcotest.check(
          Alcotest.string,
          "second text retained",
          "second",
          streamText(streaming, "stream-2"),
        );
      }),
      Alcotest.test_case("error for non-current stream preserves current stream", `Quick, () => {
        let streaming =
          LlmChatStore.reduceStream(
            overlappingStreams(),
            LlmChatStore.StreamError("thread-1", "stream-1", "failed"),
          );
        Alcotest.check(
          Alcotest.bool,
          "still streaming",
          true,
          streaming.LlmChatStore.isStreaming,
        );
        assertCurrentStream(~label="current stream id", ~streaming, ~expected="stream-2");
        Alcotest.check(
          Alcotest.string,
          "failed stream removed",
          "",
          streamText(streaming, "stream-1"),
        );
        Alcotest.check(
          Alcotest.string,
          "second text retained",
          "second",
          streamText(streaming, "stream-2"),
        );
        switch (streaming.LlmChatStore.streamError) {
        | Some(error) =>
          Alcotest.check(
            Alcotest.string,
            "stream error",
            "failed",
            error.LlmChatStore.message,
          )
        | None => Alcotest.fail("stream error was not recorded")
        };
      }),
    ],
  );

let () = Alcotest.run("llm-chat-store", [suite]);
