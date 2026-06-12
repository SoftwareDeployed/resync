open Js.Promise;

[@mel.send]
external includes: (string, string) => bool = "includes";

let rec waitForSelectorText = (~page, ~selector, ~expected, ~label, ~attemptsLeft) =>
  if (attemptsLeft <= 0) {
    BrowserTestUtils.textOrEmpty(page, selector)
    |> then_(text =>
         reject(
           BrowserTestUtils.makeError(
             label
             ++ " timed out waiting for selector "
             ++ selector
             ++ " to contain: "
             ++ expected
             ++ ". Last value: "
             ++ text,
           ),
         )
       );
  } else {
    BrowserTestUtils.textOrEmpty(page, selector)
    |> then_(text =>
         if (text->includes(expected)) {
           Js.log("[PASS] " ++ label);
           resolve();
         } else {
           BrowserTestUtils.sleep(100)
           |> then_(_ =>
                waitForSelectorText(~page, ~selector, ~expected, ~label, ~attemptsLeft=attemptsLeft - 1)
              );
         }
        );
  };

let rec waitForExpressionTrue = (~page, ~expression, ~label, ~attemptsLeft) =>
  if (attemptsLeft <= 0) {
    page->Playwright.evaluateString(expression)
    |> then_(result =>
         reject(
           BrowserTestUtils.makeError(
             label ++ " timed out waiting for expression to become true. Last value: " ++ result,
           ),
         )
       );
  } else {
    page->Playwright.evaluateString(expression)
    |> then_(result =>
         if (result == "true") {
           Js.log("[PASS] " ++ label);
           resolve();
         } else {
           BrowserTestUtils.sleep(100)
           |> then_(_ =>
                waitForExpressionTrue(
                  ~page,
                  ~expression,
                  ~label,
                  ~attemptsLeft=attemptsLeft - 1,
                )
              );
         }
       );
  };

[@mel.module "node:child_process"]
external execSync: (string, Js.Dict.t(string)) => string = "execSync";

[@mel.scope "process"]
external env: Js.Dict.t(string) = "env";

[%%raw {|
function llmChatMockLastUserContent(body) {
  try {
    const parsed = JSON.parse(body);
    const messages = Array.isArray(parsed.messages) ? parsed.messages : [];
    for (let i = messages.length - 1; i >= 0; i--) {
      const message = messages[i] || {};
      if (message.role === "user" && typeof message.content === "string") {
        return message.content;
      }
    }
  } catch (_error) {
  }
  return "";
}

function llmChatMockResponseChunk(content) {
  return JSON.stringify({
    message: { role: "assistant", content },
    done: false
  }) + "\n";
}

function llmChatMockLongResponseChunk(prefix, start, count) {
  const lines = [];
  for (let i = 0; i < count; i++) {
    const n = start + i;
    lines.push(`${prefix} ${n}: streaming content keeps growing across multiple layout passes.`);
  }
  return llmChatMockResponseChunk(lines.join("\n") + "\n");
}

function llmChatMockSseResponseChunk(content) {
  return "data: " + JSON.stringify({
    choices: [{ delta: { content } }]
  }) + "\n\n";
}

function llmChatStreamingProbeInstallScript(expected, notExpected, timeoutMs) {
  return `(() => {
    const expected = ${JSON.stringify(expected)};
    const notExpected = ${JSON.stringify(notExpected)};
    const timeoutMs = ${timeoutMs};
    const target = document.getElementById("message-list");
    const readAssistantText = () =>
      Array.from(document.querySelectorAll(".message--assistant"))
        .map(el => el.textContent || "")
        .join("\\n");

    if (!target) {
      window.__llmChatStreamingProbe = Promise.resolve("missing-message-list");
      return "missing-message-list";
    }
    if (typeof MutationObserver !== "function") {
      window.__llmChatStreamingProbe = Promise.resolve("missing-mutation-observer");
      return "missing-mutation-observer";
    }

    window.__llmChatStreamingProbe = new Promise(resolve => {
      let settled = false;
      let timer = null;
      let observer = null;
      const finish = value => {
        if (settled) return;
        settled = true;
        if (observer) observer.disconnect();
        if (timer !== null) clearTimeout(timer);
        resolve(value);
      };
      const check = () => {
        const text = readAssistantText();
        if (text.includes(expected) && !text.includes(notExpected)) {
          finish("true");
        }
      };

      observer = new MutationObserver(check);
      observer.observe(target, { childList: true, subtree: true, characterData: true });
      timer = setTimeout(() => {
        finish("timeout:" + readAssistantText());
      }, timeoutMs);
      check();
    });
    return "installed";
  })()`;
}

function llmChatStreamingProbeAwaitScript() {
  return `(() => window.__llmChatStreamingProbe || Promise.resolve("missing-probe"))()`;
}

function llmChatNoGhostingProbeInstallScript(expected, tailFragment) {
  return `(() => {
    const expected = ${JSON.stringify(expected)};
    const fragments = [expected, ${JSON.stringify(tailFragment)}].filter(Boolean);
    const target = document.getElementById("message-list");
    if (!target) {
      window.__llmChatNoGhostingProbe = { stop: () => "missing-message-list" };
      return "missing-message-list";
    }

    const state = { firstResponseNode: null, violations: [], stopped: false };
    const describeNode = el =>
      (el.id || "no-id") + ":" + (el.textContent || "").trim().slice(0, 80);
    const assistantNodes = () =>
      Array.from(document.querySelectorAll(".message--assistant"));
    const directTextBlocks = el => {
      const root = el.firstElementChild || el;
      return Array.from(root.children || [])
        .map(child => (child.textContent || "").trim())
        .filter(Boolean);
    };
    const check = () => {
      if (state.stopped) return;
      const legacyStreaming = document.querySelector("#streaming-message");
      const legacyStreamingText = (legacyStreaming?.textContent || "").trim();
      if (legacyStreamingText.length > 0) {
        state.violations.push("legacy-streaming-message:" + legacyStreamingText.slice(0, 80));
      }

      const responseChildren = Array.from(target.children || [])
        .filter(el => fragments.some(fragment => (el.textContent || "").includes(fragment)));
      const invalidResponseChildren = responseChildren
        .filter(el => !el.classList.contains("message--assistant"));
      if (invalidResponseChildren.length > 0) {
        state.violations.push(
          "response-outside-assistant-message:" + invalidResponseChildren.map(describeNode).join(" | ")
        );
      }
      if (responseChildren.length > 1) {
        state.violations.push(
          "response-in-multiple-message-list-children:" + responseChildren.map(describeNode).join(" | ")
        );
      }

      const assistants = assistantNodes();
      const assistantText = assistants
        .map(el => (el.textContent || "").trim())
        .filter(Boolean);
      if (assistantText.length > 1) {
        state.violations.push(
          "multiple-assistant-bubbles:" + assistants.map(describeNode).join(" | ")
        );
      }

      const matching = assistants
        .filter(el => (el.textContent || "").includes(expected));
      if (matching.length > 1) {
        state.violations.push(
          "duplicate-response-nodes:" + matching.map(describeNode).join(" | ")
        );
      }
      matching.forEach(el => {
        const blocks = directTextBlocks(el);
        if (blocks.length > 1) {
          state.violations.push(
            "split-response-blocks:" + blocks.map(text => text.slice(0, 60)).join(" | ")
          );
        }
      });
      if (matching.length > 0) {
        if (state.firstResponseNode === null) {
          state.firstResponseNode = matching[0];
        } else if (!matching.includes(state.firstResponseNode)) {
          state.violations.push(
            "assistant-node-replaced:" + matching.map(describeNode).join(" | ")
          );
        }
      }
    };

    const observer = new MutationObserver(check);
    observer.observe(target, { childList: true, subtree: true, characterData: true });
    window.__llmChatNoGhostingProbe = {
      stop: () => {
        check();
        state.stopped = true;
        observer.disconnect();
        return state.violations.length === 0
          ? "true"
          : "ghosting:" + state.violations.slice(0, 5).join(" | ");
      }
    };
    check();
    return "installed";
  })()`;
}

function llmChatNoGhostingProbeResultScript() {
  return `(() => {
    const probe = window.__llmChatNoGhostingProbe;
    return probe && typeof probe.stop === "function" ? probe.stop() : "missing-probe";
  })()`;
}

function llmChatAssistantHasTextExpression(expected) {
  return `(() => Array.from(document.querySelectorAll(".message--assistant"))
    .some(el => (el.textContent || "").includes(${JSON.stringify(expected)})))().toString()`;
}

function llmChatSeedMessagesScript(baseUrl, threadId, count) {
  const url = `${baseUrl}/api/test/add-message`;
  return `(() => {
    const url = ${JSON.stringify(url)};
    const threadId = ${JSON.stringify(threadId)};
    const makeContent = i =>
      "History message " + i + "\\n" +
      "This seeded paragraph creates enough vertical space for deterministic scroll testing. ".repeat(4);
    return Array.from({ length: ${count} }, (_, i) => i)
      .reduce((promise, i) => promise.then(() =>
        fetch(url, {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({
            thread_id: threadId,
            role: i % 2 === 0 ? "user" : "assistant",
            content: makeContent(i)
          })
        })
      ), Promise.resolve())
      .then(() => "seeded");
  })()`;
}

function llmChatScrollToBottomScript() {
  return `(() => {
    const el = document.getElementById("message-list");
    if (!el) return "missing-message-list";
    el.scrollTop = el.scrollHeight;
    el.dispatchEvent(new Event("scroll"));
    return JSON.stringify({
      top: el.scrollTop,
      bottomDistance: Math.max(0, el.scrollHeight - el.clientHeight - el.scrollTop)
    });
  })()`;
}

function llmChatScrollUpFromBottomScript(offset) {
  return `(() => {
    const el = document.getElementById("message-list");
    if (!el) return "missing-message-list";
    const maxTop = Math.max(0, el.scrollHeight - el.clientHeight);
    el.scrollTop = Math.max(0, maxTop - ${offset});
    el.dispatchEvent(new Event("scroll"));
    return JSON.stringify({
      top: el.scrollTop,
      bottomDistance: Math.max(0, el.scrollHeight - el.clientHeight - el.scrollTop)
    });
  })()`;
}

function llmChatNearBottomExpression() {
  return `(() => {
    const el = document.getElementById("message-list");
    if (!el) return false;
    return el.scrollTop + el.clientHeight >= el.scrollHeight - 12;
  })().toString()`;
}

function llmChatBottomDistanceAtLeastExpression(distance) {
  return `(() => {
    const el = document.getElementById("message-list");
    if (!el) return false;
    return Math.max(0, el.scrollHeight - el.clientHeight - el.scrollTop) >= ${distance};
  })().toString()`;
}

function llmChatScrollStabilityInstallScript() {
  return `(() => {
    const el = document.getElementById("message-list");
    if (!el) return "missing-message-list";

    const state = { samples: [], violations: [], stopped: false, pending: false };
    const assistantText = () =>
      Array.from(document.querySelectorAll(".message--assistant"))
        .map(node => node.textContent || "")
        .join("\\n");
    const sample = () => {
      if (state.stopped) return;
      const maxTop = Math.max(0, el.scrollHeight - el.clientHeight);
      if (maxTop <= 0 || !assistantText().includes("Mock assistant saw")) return;
      const top = el.scrollTop;
      const bottomDistance = maxTop - top;
      const previous = state.samples[state.samples.length - 1];
      if (bottomDistance > 12) {
        state.violations.push("not-pinned:" + JSON.stringify({ top, maxTop, bottomDistance }));
      }
      if (previous && top < previous.top - 4) {
        state.violations.push("scroll-top-decreased:" + JSON.stringify({ previous: previous.top, top }));
      }
      state.samples.push({ top, maxTop, bottomDistance });
    };
    const scheduleSample = () => {
      if (state.pending || state.stopped) return;
      state.pending = true;
      requestAnimationFrame(() => {
        requestAnimationFrame(() => {
          state.pending = false;
          sample();
        });
      });
    };

    const observer = new MutationObserver(scheduleSample);
    observer.observe(el, { childList: true, subtree: true, characterData: true });
    const interval = setInterval(sample, 75);
    window.__llmChatScrollStability = {
      stop: () => {
        sample();
        state.stopped = true;
        observer.disconnect();
        clearInterval(interval);
        if (state.violations.length > 0) {
          return "violations:" + state.violations.slice(0, 5).join(" | ");
        }
        if (state.samples.length < 2) {
          return "insufficient-samples:" + JSON.stringify(state.samples);
        }
        return "true";
      }
    };
    sample();
    return "installed";
  })()`;
}

function llmChatScrollStabilityResultScript() {
  return `(() => {
    const probe = window.__llmChatScrollStability;
    return probe && typeof probe.stop === "function" ? probe.stop() : "missing-probe";
  })()`;
}
|}];

external mockLastUserContent: string => string = "llmChatMockLastUserContent";
external mockResponseChunk: string => string = "llmChatMockResponseChunk";
external mockLongResponseChunk: (string, int, int) => string = "llmChatMockLongResponseChunk";
external mockSseResponseChunk: string => string = "llmChatMockSseResponseChunk";
external streamingProbeInstallScript: (string, string, int) => string = "llmChatStreamingProbeInstallScript";
external streamingProbeAwaitScript: unit => string = "llmChatStreamingProbeAwaitScript";
external noGhostingProbeInstallScript: (string, string) => string = "llmChatNoGhostingProbeInstallScript";
external noGhostingProbeResultScript: unit => string = "llmChatNoGhostingProbeResultScript";
external assistantHasTextExpression: string => string = "llmChatAssistantHasTextExpression";
external seedMessagesScript: (string, string, int) => string = "llmChatSeedMessagesScript";
external scrollToBottomScript: unit => string = "llmChatScrollToBottomScript";
external scrollUpFromBottomScript: int => string = "llmChatScrollUpFromBottomScript";
external nearBottomExpression: unit => string = "llmChatNearBottomExpression";
external bottomDistanceAtLeastExpression: int => string = "llmChatBottomDistanceAtLeastExpression";
external scrollStabilityInstallScript: unit => string = "llmChatScrollStabilityInstallScript";
external scrollStabilityResultScript: unit => string = "llmChatScrollStabilityResultScript";

let installStreamingProbe = (~page, ~expected, ~notExpected, ~label) =>
  page
  ->Playwright.evaluateString(streamingProbeInstallScript(expected, notExpected, 6000))
  |> then_(result =>
       BrowserTestUtils.assertTrue(
         ~label=label ++ ": MutationObserver probe installed",
         result == "installed",
         ~details="Expected probe installation to return installed, got: " ++ result,
       )
     );

let awaitStreamingProbe = (~page, ~label) =>
  page
  ->Playwright.evaluateString(streamingProbeAwaitScript())
  |> then_(result =>
       BrowserTestUtils.assertTrue(
         ~label,
         result == "true",
         ~details="Expected MutationObserver to see a partial assistant DOM mutation, got: " ++ result,
       )
     );

let installNoGhostingProbe = (~page, ~expected, ~tailFragment, ~label) =>
  page
  ->Playwright.evaluateString(noGhostingProbeInstallScript(expected, tailFragment))
  |> then_(result =>
       BrowserTestUtils.assertTrue(
         ~label=label ++ ": no-ghosting probe installed",
         result == "installed",
         ~details="Expected no-ghosting probe to install, got: " ++ result,
       )
     );

let assertNoGhosting = (~page, ~label) =>
  page
  ->Playwright.evaluateString(noGhostingProbeResultScript())
  |> then_(result =>
       BrowserTestUtils.assertTrue(
         ~label,
         result == "true",
         ~details="Expected no transient streaming bubble beside confirmed assistant message, got: " ++ result,
       )
     );

let currentThreadIdScript =
  "(() => window.location.pathname.split('/').filter(Boolean).slice(-1)[0] || '')()";

let readCurrentThreadId = page => page->Playwright.evaluateString(currentThreadIdScript);

let createThreadScript = (~baseUrl, ~title) =>
  "fetch('"
  ++ baseUrl
  ++ "/api/test/create-thread', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: '{\"title\": \""
  ++ title
  ++ "\"}' }).then(r => r.json()).then(j => j.id)";

type mockRequest;
type mockResponse;
type mockServer;
type addressInfo;

[@mel.module "node:http"]
external createServer: ((mockRequest, mockResponse) => unit) => mockServer = "createServer";

[@mel.module "node:timers"]
external setTimeout: (unit => unit, int) => unit = "setTimeout";

[@mel.send]
external listenOn: (mockServer, int, string, unit => unit) => unit = "listen";

[@mel.send]
external closeServer: mockServer => mockServer = "close";

[@mel.send]
external address: mockServer => addressInfo = "address";

[@mel.get]
external port: addressInfo => int = "port";

[@mel.send]
external requestOnString: (mockRequest, string, string => unit) => mockRequest = "on";

[@mel.send]
external requestOnUnit: (mockRequest, string, unit => unit) => mockRequest = "on";

[@mel.send]
external setEncoding: (mockRequest, string) => unit = "setEncoding";

[@mel.send]
external writeHead: (mockResponse, int, Js.Dict.t(string)) => mockResponse = "writeHead";

[@mel.send]
external write: (mockResponse, string) => bool = "write";

[@mel.send]
external endString: (mockResponse, string) => mockResponse = "end";

let getDbUrl = () =>
  switch (Js.Dict.get(env, "DB_URL")) {
  | Some(url) => url
  | None => "postgres://executor:executor-password@localhost:5432/executor_db"
  };

let execSql = (sql: string) => {
  let dbUrl = getDbUrl();
  let command = "psql \"" ++ dbUrl ++ "\" -c \"" ++ sql ++ "\"";
  let options = Js.Dict.empty();
  Js.Dict.set(options, "encoding", "utf8");
  try ({
    let result = execSync(command, options);
    Js.log2("execSql result:", result);
  }) {
  | _exn => Js.log("execSql: command failed (may be expected)")
  };
};

let installMalformedSendPromptLedger = () => {
  execSql("DROP TABLE IF EXISTS _resync_actions");
  execSql("DROP TABLE IF EXISTS _resync_actions_send_prompt");
  execSql("CREATE TABLE _resync_actions_send_prompt (action_id text PRIMARY KEY)");
};

let removeMalformedSendPromptLedger = () => {
  execSql("DROP TABLE IF EXISTS _resync_actions_send_prompt");
  execSql(
    "CREATE TABLE IF NOT EXISTS _resync_actions (action_id uuid PRIMARY KEY, mutation_name text NOT NULL, status text NOT NULL CHECK (status IN ('ok', 'failed')), processed_at timestamptz NOT NULL DEFAULT NOW(), error_message text)",
  );
  execSql(
    "CREATE INDEX IF NOT EXISTS idx_resync_actions_lookup ON _resync_actions(action_id, mutation_name)",
  );
};

module MockOllama = {
  type t = {
    server: mockServer,
    lastRequestBody: ref(string),
    endedByPrompt: Js.Dict.t(bool),
  };

  let start = () =>
    Js.Promise.make((~resolve, ~reject as _) => {
      let lastRequestBody = ref("");
      let endedByPrompt = Js.Dict.empty();
      let server =
        createServer((request, response) => {
          let requestBody = ref("");
          request->setEncoding("utf8");
          let _ =
            request->requestOnString("data", chunk =>
              requestBody := requestBody.contents ++ chunk
            );
          let _ =
            request->requestOnUnit("end", () => {
              lastRequestBody := requestBody.contents;
              let prompt = mockLastUserContent(requestBody.contents);
              Js.Dict.set(endedByPrompt, prompt, false);
              let headers = Js.Dict.empty();
              if (prompt->includes("sse streaming regression")) {
                Js.Dict.set(headers, "Content-Type", "text/event-stream");
                let _ = response->writeHead(200, headers);
                let _ = response->write(mockSseResponseChunk("SSE assistant saw "));
                setTimeout(() => {
                  let _ = response->write(mockSseResponseChunk(prompt));
                  setTimeout(() => {
                    let _ = response->write(mockSseResponseChunk(" while streaming"));
                    setTimeout(() => {
                      Js.Dict.set(endedByPrompt, prompt, true);
                      let _ = response->endString("data: [DONE]\n\n");
                      ();
                    }, 500);
                  }, 1200);
                }, 700);
              } else if (
                prompt->includes("scroll stability regression")
                || prompt->includes("scroll opt-out regression")
              ) {
                Js.Dict.set(headers, "Content-Type", "application/x-ndjson");
                let _ = response->writeHead(200, headers);
                let _ = response->write(mockResponseChunk("Mock assistant saw\n"));
                setTimeout(() => {
                  let _ = response->write(mockLongResponseChunk("scroll stream line", 1, 10));
                  setTimeout(() => {
                    let _ = response->write(mockLongResponseChunk("scroll stream line", 11, 10));
                    setTimeout(() => {
                      let _ = response->write(mockLongResponseChunk("scroll stream line", 21, 10));
                      setTimeout(() => {
                        let _ = response->write(
                          mockResponseChunk("scroll final marker\n"),
                        );
                        setTimeout(() => {
                          Js.Dict.set(endedByPrompt, prompt, true);
                          let _ = response->endString("{\"done\":true}\n");
                          ();
                        }, 250);
                      }, 250);
                    }, 250);
                  }, 250);
                }, 250);
              } else {
                Js.Dict.set(headers, "Content-Type", "application/x-ndjson");
                let _ = response->writeHead(200, headers);
                let _ = response->write(mockResponseChunk("Mock assistant saw "));
                let writeTail = () => {
                  let _ = response->write(mockResponseChunk(prompt));
                  setTimeout(() => {
                    let _ = response->write(mockResponseChunk(" while streaming"));
                    setTimeout(() => {
                      Js.Dict.set(endedByPrompt, prompt, true);
                      let _ = response->endString("{\"done\":true}\n");
                      ();
                    }, 300);
                  }, 250);
                };
                if (prompt->includes("strict streaming regression")) {
                  setTimeout(() => {
                    let _ = response->write(mockResponseChunk(prompt));
                    setTimeout(() => {
                      let _ = response->write(mockResponseChunk(" while streaming"));
                      setTimeout(() => {
                        Js.Dict.set(endedByPrompt, prompt, true);
                        let _ = response->endString("{\"done\":true}\n");
                        ();
                      }, 500);
                    }, 1200);
                  }, 700);
                } else {
                  writeTail();
                };
              };
            });
          ();
        });
      server->listenOn(0, "127.0.0.1", () => {
        let selectedPort = server->address->port;
        let url =
          "http://127.0.0.1:" ++ Js.Int.toString(selectedPort) ++ "/api/chat";
        Js.Dict.set(env, "LLM_CHAT_OLLAMA_URL", url);
        resolve(. {server, lastRequestBody, endedByPrompt});
      });
    });

  let stop = mock =>
    {
      let _ = mock.server->closeServer;
      Js.Promise.resolve();
    };

  let assertSawPrompt = (~mock, ~prompt) =>
    BrowserTestUtils.assertTrue(
      ~label="Mock Ollama received prompt",
      mock.lastRequestBody.contents->includes(prompt),
      ~details=
        "Expected mock Ollama request body to include prompt: "
        ++ prompt
        ++ ". Last body: "
        ++ mock.lastRequestBody.contents,
    );

  let hasEndedPrompt = (~mock, ~prompt) =>
    switch (Js.Dict.get(mock.endedByPrompt, prompt)) {
    | Some(true) => true
    | _ => false
    };
};

let cleanup = (~browser, ~server, ~mock) => {
  removeMalformedSendPromptLedger();
  let closeBrowser =
    switch (browser) {
    | Some(activeBrowser) => activeBrowser->Playwright.close |> catch(_ => resolve())
    | None => resolve()
    };
  closeBrowser
  |> then_(_ =>
       switch (server) {
       | Some(activeServer) => LlmChatTestServer.stop(activeServer) |> catch(_ => resolve())
       | None => resolve()
       }
     )
  |> then_(_ =>
       switch (mock) {
       | Some(activeMock) => MockOllama.stop(activeMock) |> catch(_ => resolve())
       | None => resolve()
       }
     );
};

let runThreadListAndInputScenario = (~browser, ~baseUrl) => {
  Js.log("Running thread list and input interaction scenario...");
  browser
  ->Playwright.newPage
  |> then_(page =>
       page
       ->Playwright.goto(baseUrl ++ "/")
       |> then_(_ => page->Playwright.waitForSelector("#thread-list"))
       |> then_(_ =>
            waitForSelectorText(
              ~page,
              ~selector="#thread-list",
              ~expected="New Chat",
              ~label="Thread list renders with New Chat",
              ~attemptsLeft=50,
            )
          )
       |> then_(_ => page->Playwright.waitForSelector("#prompt-input"))
       |> then_(_ => page->Playwright.fill("#prompt-input", "Hello from browser test"))
       |> then_(_ => page->Playwright.waitForSelector("#send-button"))
       |> then_(_ => page->Playwright.click("#send-button"))
       |> then_(_ =>
            waitForSelectorText(
              ~page,
              ~selector="#message-list",
              ~expected="Hello from browser test",
              ~label="User message appears after send",
              ~attemptsLeft=50,
            )
          )
       |> then_(_ => BrowserTestUtils.sleep(500))
       |> then_(_ => page->Playwright.evaluateString("window.location.href"))
     );
};

let runMessageDisplayScenario = (~browser, ~threadUrl) => {
  Js.log("Running message display scenario...");
  browser
  ->Playwright.newPage
  |> then_(page =>
       page
       ->Playwright.goto(threadUrl)
       |> then_(_ => page->Playwright.waitForSelector("#message-list"))
       |> then_(_ =>
            waitForSelectorText(
              ~page,
              ~selector="#message-list",
              ~expected="Hello from browser test",
              ~label="Existing user message renders from snapshot",
              ~attemptsLeft=50,
            )
          )
       |> then_(_ =>
            page->Playwright.evaluateString(
              "document.querySelector('.message--user') !== null ? 'true' : 'false'",
            )
          )
       |> then_(hasUserClass =>
            BrowserTestUtils.assertTrue(
              ~label="User message has correct role class",
              hasUserClass == "true",
              ~details="Expected .message--user element in DOM",
            )
          )
     );
};

let runOllamaStreamingScenario = (~browser, ~baseUrl, ~mock) => {
  Js.log("Running Ollama streaming scenario...");
  installMalformedSendPromptLedger();
  let prompt = "strict streaming regression " ++ UUID.make();
  browser
  ->Playwright.newPage
  |> then_(page =>
       page
       ->Playwright.goto(baseUrl ++ "/")
       |> then_(_ =>
            page->Playwright.evaluateString(
              createThreadScript(~baseUrl, ~title="Strict Streaming Test"),
            )
          )
       |> then_(threadId => page->Playwright.goto(baseUrl ++ "/" ++ threadId))
       |> then_(_ => page->Playwright.waitForSelector("#prompt-input"))
       |> then_(_ =>
            installStreamingProbe(
              ~page,
              ~expected="Mock assistant saw",
              ~notExpected=prompt,
              ~label="Ollama streaming test",
            )
          )
       |> then_(_ =>
            installNoGhostingProbe(
              ~page,
              ~expected="Mock assistant saw",
              ~tailFragment="while streaming",
              ~label="Ollama streaming test",
            )
          )
       |> then_(_ => page->Playwright.fill("#prompt-input", prompt))
       |> then_(_ => page->Playwright.click("#send-button"))
       |> then_(_ =>
            waitForSelectorText(
              ~page,
              ~selector="#message-list",
              ~expected=prompt,
              ~label="Prompt visible in message list",
              ~attemptsLeft=30,
            )
          )
       |> then_(_ =>
            awaitStreamingProbe(
              ~page,
              ~label="Ollama streaming test: MutationObserver saw first partial token before later API chunks",
            )
          )
       |> then_(_ =>
            BrowserTestUtils.assertTrue(
              ~label="Ollama streaming test: upstream response is still open when partial DOM renders",
              !MockOllama.hasEndedPrompt(~mock, ~prompt),
              ~details="Expected partial assistant DOM update before mock endpoint ended",
            )
          )
       |> then_(_ =>
            waitForSelectorText(
              ~page,
              ~selector="#message-list",
              ~expected="Mock assistant saw " ++ prompt ++ " while streaming",
              ~label="Ollama streaming test: final assistant message reconciles",
              ~attemptsLeft=150,
            )
          )
       |> then_(_ =>
            waitForExpressionTrue(
              ~page,
              ~expression="(() => document.querySelector('#streaming-message') == null && document.querySelectorAll('.message--assistant').length === 1)().toString()",
              ~label="Ollama streaming test: one confirmed assistant message remains after completion",
              ~attemptsLeft=150,
            )
          )
       |> then_(_ =>
            assertNoGhosting(
              ~page,
              ~label="Ollama streaming test: tokens never render in a second ghost bubble",
            )
          )
       |> then_(_ => MockOllama.assertSawPrompt(~mock, ~prompt))
       |> then_(_ => {
            removeMalformedSendPromptLedger();
            resolve();
          })
      );
};

let runSseStreamingScenario = (~browser, ~baseUrl, ~mock) => {
  Js.log("Running SSE streaming scenario...");
  removeMalformedSendPromptLedger();
  let prompt = "sse streaming regression " ++ UUID.make();
  browser
  ->Playwright.newPage
  |> then_(page =>
       page
       ->Playwright.goto(baseUrl ++ "/")
       |> then_(_ =>
            page->Playwright.evaluateString(
              createThreadScript(~baseUrl, ~title="SSE Streaming Test"),
            )
          )
       |> then_(threadId => page->Playwright.goto(baseUrl ++ "/" ++ threadId))
       |> then_(_ => page->Playwright.waitForSelector("#prompt-input"))
       |> then_(_ =>
            installStreamingProbe(
              ~page,
              ~expected="SSE assistant saw",
              ~notExpected=prompt,
              ~label="SSE streaming test",
            )
          )
       |> then_(_ =>
            installNoGhostingProbe(
              ~page,
              ~expected="SSE assistant saw",
              ~tailFragment="while streaming",
              ~label="SSE streaming test",
            )
          )
       |> then_(_ => page->Playwright.fill("#prompt-input", prompt))
       |> then_(_ => page->Playwright.click("#send-button"))
       |> then_(_ =>
            waitForSelectorText(
              ~page,
              ~selector="#message-list",
              ~expected=prompt,
              ~label="SSE streaming test: prompt visible in message list",
              ~attemptsLeft=30,
            )
          )
       |> then_(_ =>
            awaitStreamingProbe(
              ~page,
              ~label="SSE streaming test: MutationObserver saw first delta before later API chunks",
            )
          )
       |> then_(_ =>
            BrowserTestUtils.assertTrue(
              ~label="SSE streaming test: upstream response is still open when partial DOM renders",
              !MockOllama.hasEndedPrompt(~mock, ~prompt),
              ~details="Expected partial assistant DOM update before mock endpoint ended",
            )
          )
       |> then_(_ =>
            waitForSelectorText(
              ~page,
              ~selector="#message-list",
              ~expected="SSE assistant saw " ++ prompt ++ " while streaming",
              ~label="SSE streaming test: final assistant message reconciles",
              ~attemptsLeft=150,
            )
          )
       |> then_(_ =>
            waitForExpressionTrue(
              ~page,
              ~expression="(() => document.querySelector('#streaming-message') == null && document.querySelectorAll('.message--assistant').length === 1)().toString()",
              ~label="SSE streaming test: one confirmed assistant message remains after completion",
              ~attemptsLeft=150,
            )
          )
       |> then_(_ =>
            assertNoGhosting(
              ~page,
              ~label="SSE streaming test: tokens never render in a second ghost bubble",
            )
          )
       |> then_(_ => MockOllama.assertSawPrompt(~mock, ~prompt))
      );
};

let runCrossTabRealtimeSyncScenario = (~browser, ~baseUrl) => {
  Js.log("Running cross-tab realtime sync scenario...");
  let prompt =
    "Please give a somewhat detailed multi-sentence answer about websocket streaming so the response is not too short.";
  let pageARef = ref(None);
  browser
  ->Playwright.newPage
  |> then_(pageA => {
       pageARef := Some(pageA);
       pageA
       ->Playwright.goto(baseUrl ++ "/")
       |> then_(_ =>
            pageA->Playwright.evaluateString(
              createThreadScript(~baseUrl, ~title="Cross-tab Test"),
            )
          )
       |> then_(threadId => {
            let threadUrl = baseUrl ++ "/" ++ threadId;
            pageA
            ->Playwright.goto(threadUrl)
            |> then_(_ => pageA->Playwright.waitForSelector("#prompt-input"))
            |> then_(_ =>
                 browser
                 ->Playwright.newPage
                 |> then_(pageB =>
                      pageB
                      ->Playwright.goto(threadUrl)
                      |> then_(_ => pageB->Playwright.waitForSelector("#prompt-input"))
                      |> then_(_ =>
                           installNoGhostingProbe(
                             ~page=pageA,
                             ~expected="Mock assistant saw",
                             ~tailFragment="while streaming",
                             ~label="Cross-tab sync sender",
                           )
                         )
                      |> then_(_ =>
                           installNoGhostingProbe(
                             ~page=pageB,
                             ~expected="Mock assistant saw",
                             ~tailFragment="while streaming",
                             ~label="Cross-tab sync viewer",
                           )
                         )
                      |> then_(_ => pageA->Playwright.fill("#prompt-input", prompt))
                      |> then_(_ => pageA->Playwright.click("#send-button"))
                      |> then_(_ =>
                           waitForSelectorText(
                             ~page=pageA,
                             ~selector="#message-list",
                             ~expected=prompt,
                             ~label="Cross-tab sync: sender sees user prompt",
                             ~attemptsLeft=50,
                           )
                         )
                      |> then_(_ =>
                           waitForSelectorText(
                             ~page=pageB,
                             ~selector="#message-list",
                             ~expected=prompt,
                             ~label="Cross-tab sync: viewer sees user prompt in realtime",
                             ~attemptsLeft=50,
                           )
                         )
                      |> then_(_ =>
                           waitForExpressionTrue(
                             ~page=pageB,
                             ~expression=assistantHasTextExpression("Mock assistant saw"),
                             ~label="Cross-tab sync: viewer sees partial assistant response",
                             ~attemptsLeft=150,
                           )
                         )
                      |> then_(_ =>
                           waitForExpressionTrue(
                             ~page=pageB,
                             ~expression="(() => document.querySelector('#streaming-message') == null && document.querySelectorAll('.message--assistant').length === 1)().toString()",
                             ~label="Cross-tab sync: viewer receives end-of-stream and reconciles to one confirmed assistant message",
                             ~attemptsLeft=250,
                           )
                         )
                      |> then_(_ =>
                           waitForExpressionTrue(
                             ~page=pageA,
                             ~expression="(() => document.querySelector('#streaming-message') == null && document.querySelectorAll('.message--assistant').length === 1)().toString()",
                             ~label="Cross-tab sync: sender receives end-of-stream and reconciles to one confirmed assistant message",
                             ~attemptsLeft=250,
                           )
                         )
                      |> then_(_ =>
                           assertNoGhosting(
                             ~page=pageB,
                             ~label="Cross-tab sync: viewer tokens never render in a second ghost bubble",
                           )
                         )
                      |> then_(_ =>
                           assertNoGhosting(
                             ~page=pageA,
                             ~label="Cross-tab sync: sender tokens never render in a second ghost bubble",
                           )
                         )
                      |> then_(_ =>
                           pageA->Playwright.evaluateString(
                             "document.getElementById('message-list')?.textContent || ''"
                           )
                         )
                      |> then_(textA =>
                           pageB->Playwright.evaluateString(
                             "document.getElementById('message-list')?.textContent || ''"
                           )
                           |> then_(textB =>
                                BrowserTestUtils.assertTrue(
                                  ~label="Cross-tab sync: both tabs converge to same final text",
                                  textA == textB && textA->includes(prompt) == true,
                                  ~details="Expected both tabs to converge to identical final transcript",
                                )
                              )
                         )
                      |> then_(_ =>
                           pageB->Playwright.evaluateString(
                             "document.querySelectorAll('.message--assistant').length.toString()"
                           )
                         )
                      |> then_(assistantCount =>
                           BrowserTestUtils.assertTrue(
                             ~label="Cross-tab sync: final assistant message is not duplicated after reconciliation",
                             assistantCount == "1",
                             ~details="Expected one assistant message bubble, got: " ++ assistantCount,
                           )
                         )
                    )
               )
          })
     })
  |> catch(error =>
       switch (pageARef.contents) {
       | Some(pageA) =>
           pageA->Playwright.evaluateString(
             "document.querySelector('.message[role=\\'assistant\\']') !== null ? 'true' : 'false'"
           )
           |> then_(hasConfirmedAssistant =>
                if (hasConfirmedAssistant == "false") {
                  Js.log2("[FAIL] Cross-tab realtime sync test failed before assistant response:", error);
                  BrowserTestUtils.rejectPromiseError(error);
                } else {
                  Js.log2("[FAIL] Cross-tab realtime sync test failed:", error);
                  reject(BrowserTestUtils.makeError("Cross-tab realtime sync test failed"));
                }
              )
       | None => reject(BrowserTestUtils.makeError("Cross-tab realtime sync test failed before opening sender page"))
       }
     );
};

let runStreamingScrollScenario = (~browser, ~baseUrl) => {
  Js.log("Running streaming scroll scenario...");
  let prompt =
    "scroll stability regression " ++ UUID.make();

  browser
  ->Playwright.newPage
  |> then_(page =>
       page
       ->Playwright.goto(baseUrl ++ "/")
       |> then_(_ =>
            page->Playwright.evaluateString(
              createThreadScript(~baseUrl, ~title="Streaming Scroll Stability Test"),
            )
          )
       |> then_(threadId =>
            page->Playwright.evaluateString(seedMessagesScript(baseUrl, threadId, 18))
            |> then_(_ => page->Playwright.goto(baseUrl ++ "/" ++ threadId))
          )
       |> then_(_ => page->Playwright.waitForSelector("#prompt-input"))
       |> then_(_ =>
            waitForSelectorText(
              ~page,
              ~selector="#message-list",
              ~expected="History message 17",
              ~label="Streaming scroll: seeded history renders",
              ~attemptsLeft=80,
            )
          )
       |> then_(_ => page->Playwright.evaluateString(scrollToBottomScript()))
       |> then_(_ =>
            page->Playwright.evaluateString(scrollStabilityInstallScript())
          )
       |> then_(result =>
            BrowserTestUtils.assertTrue(
              ~label="Streaming scroll: stability probe installed",
              result == "installed",
              ~details="Expected scroll stability probe to install, got: " ++ result,
            )
          )
       |> then_(_ => page->Playwright.fill("#prompt-input", prompt))
       |> then_(_ => page->Playwright.click("#send-button"))
       |> then_(_ =>
            waitForSelectorText(
              ~page,
              ~selector="#message-list",
              ~expected=prompt,
              ~label="Streaming scroll: user prompt appears",
              ~attemptsLeft=50,
            )
          )
       |> then_(_ =>
            waitForExpressionTrue(
              ~page,
              ~expression=assistantHasTextExpression("Mock assistant saw"),
              ~label="Streaming scroll: partial assistant response appears",
              ~attemptsLeft=150,
            )
          )
       |> then_(_ =>
            waitForExpressionTrue(
              ~page,
              ~expression="(() => { const el = document.getElementById('message-list'); if (!el) return false; return el.scrollTop + el.clientHeight >= el.scrollHeight - 12; })().toString()",
              ~label="Streaming scroll: message list stays pinned near bottom during stream",
              ~attemptsLeft=120,
            )
          )
       |> then_(_ =>
            waitForSelectorText(
              ~page,
              ~selector="#message-list",
              ~expected="scroll final marker",
              ~label="Streaming scroll: stream completes with final marker",
              ~attemptsLeft=250,
            )
          )
       |> then_(_ =>
            page->Playwright.evaluateString(scrollStabilityResultScript())
          )
       |> then_(result =>
            BrowserTestUtils.assertTrue(
              ~label="Streaming scroll: pinned scroll does not jump during stream",
              result == "true",
              ~details="Expected stable pinned scrolling, got: " ++ result,
            )
          )
       |> then_(_ =>
            waitForExpressionTrue(
              ~page,
              ~expression=nearBottomExpression(),
              ~label="Streaming scroll: message list remains pinned near bottom after completion",
              ~attemptsLeft=80,
            )
          )
     )
  |> catch(error => {
       Js.log2("[FAIL] Streaming scroll test failed:", error);
       BrowserTestUtils.rejectPromiseError(error);
     });
};

let runStreamingScrollOptOutScenario = (~browser, ~baseUrl) => {
  Js.log("Running streaming scroll opt-out scenario...");
  let prompt = "scroll opt-out regression " ++ UUID.make();

  browser
  ->Playwright.newPage
  |> then_(page =>
       page
       ->Playwright.goto(baseUrl ++ "/")
       |> then_(_ =>
            page->Playwright.evaluateString(
              createThreadScript(~baseUrl, ~title="Streaming Scroll Opt Out Test"),
            )
          )
       |> then_(threadId =>
            page->Playwright.evaluateString(seedMessagesScript(baseUrl, threadId, 22))
            |> then_(_ => page->Playwright.goto(baseUrl ++ "/" ++ threadId))
          )
       |> then_(_ => page->Playwright.waitForSelector("#prompt-input"))
       |> then_(_ =>
            waitForSelectorText(
              ~page,
              ~selector="#message-list",
              ~expected="History message 21",
              ~label="Streaming scroll opt-out: seeded history renders",
              ~attemptsLeft=80,
            )
          )
       |> then_(_ => page->Playwright.evaluateString(scrollToBottomScript()))
       |> then_(_ => page->Playwright.evaluateString(scrollUpFromBottomScript(260)))
       |> then_(_ =>
            waitForExpressionTrue(
              ~page,
              ~expression=bottomDistanceAtLeastExpression(120),
              ~label="Streaming scroll opt-out: user is scrolled away from bottom before sending",
              ~attemptsLeft=30,
            )
          )
       |> then_(_ => page->Playwright.fill("#prompt-input", prompt))
       |> then_(_ => page->Playwright.click("#send-button"))
       |> then_(_ =>
            waitForExpressionTrue(
              ~page,
              ~expression=assistantHasTextExpression("Mock assistant saw"),
              ~label="Streaming scroll opt-out: partial assistant response appears",
              ~attemptsLeft=150,
            )
          )
       |> then_(_ =>
            waitForExpressionTrue(
              ~page,
              ~expression=bottomDistanceAtLeastExpression(80),
              ~label="Streaming scroll opt-out: partial stream does not yank user to bottom",
              ~attemptsLeft=60,
            )
          )
       |> then_(_ =>
            waitForSelectorText(
              ~page,
              ~selector="#message-list",
              ~expected="scroll final marker",
              ~label="Streaming scroll opt-out: stream completes with final marker",
              ~attemptsLeft=250,
            )
          )
       |> then_(_ =>
            waitForExpressionTrue(
              ~page,
              ~expression=bottomDistanceAtLeastExpression(80),
              ~label="Streaming scroll opt-out: completion does not yank user to bottom",
              ~attemptsLeft=60,
            )
          )
     )
  |> catch(error => {
       Js.log2("[FAIL] Streaming scroll opt-out test failed:", error);
       BrowserTestUtils.rejectPromiseError(error);
     });
};

let runRootRedirectScenario = (~browser, ~baseUrl) => {
  Js.log("Running root redirect scenario...");
  browser
  ->Playwright.newPage
  |> then_(page =>
       page
       ->Playwright.goto(baseUrl ++ "/")
       |> then_(_ => page->Playwright.waitForSelector("#thread-list"))
       |> then_(_ => page->Playwright.evaluateString("window.location.href"))
       |> then_(firstUrl => {
            page
            ->Playwright.goto(baseUrl ++ "/")
            |> then_(_ => page->Playwright.waitForSelector("#thread-list"))
            |> then_(_ => page->Playwright.evaluateString("window.location.href"))
             |> then_(secondUrl =>
                  BrowserTestUtils.assertTrue(
                    ~label="Root redirect: second visit redirects to existing thread",
                    secondUrl == firstUrl,
                    ~details="Expected same thread URL, got first: " ++ firstUrl ++ " second: " ++ secondUrl,
                  )
                )
          })
     )
  |> catch(error => {
       Js.log2("[FAIL] Root redirect test failed:", error);
       BrowserTestUtils.rejectPromiseError(error);
     });
};

let runThreadDeletionScenario = (~browser, ~baseUrl) => {
  Js.log("Running thread deletion scenario...");
  browser
  ->Playwright.newPage
  |> then_(page =>
       page
       ->Playwright.goto(baseUrl ++ "/")
       |> then_(_ => page->Playwright.waitForSelector("#thread-list"))
       |> then_(_ =>
            waitForSelectorText(
              ~page,
              ~selector="#thread-list",
              ~expected="New Chat",
              ~label="Thread list renders before deletion",
              ~attemptsLeft=50,
            )
          )
       |> then_(_ => page->Playwright.waitForSelector(".thread-delete-button"))
       |> then_(_ =>
            page->Playwright.evaluateString(
              "(document.querySelector('.thread-item')?.id || '').replace('thread-item-', '')"
            )
          )
       |> then_(firstThreadId =>
            page->Playwright.evaluateString(
              "document.querySelectorAll('.thread-item').length.toString()"
            )
            |> then_(countBefore =>
                 page->Playwright.click(".thread-delete-button")
                 |> then_(_ => BrowserTestUtils.sleep(800))
                 |> then_(_ =>
                      page->Playwright.evaluateString(
                        "document.querySelectorAll('.thread-item').length.toString()"
                      )
                    )
                 |> then_(countAfter => {
                      let before = int_of_string(countBefore);
                      let after_ = int_of_string(countAfter);
                      BrowserTestUtils.assertTrue(
                        ~label="Thread count decreased after deletion",
                        after_ < before,
                        ~details=
                          "Before: " ++ countBefore ++ ", After: " ++ countAfter,
                      );
                    })
                 |> then_(_ => {
                      Js.log("Refreshing page to verify server-side deletion...");
                      page->Playwright.goto(baseUrl ++ "/")
                      |> then_(_ => page->Playwright.waitForSelector("#thread-list"))
                      |> then_(_ => BrowserTestUtils.sleep(800))
                      |> then_(_ =>
                           page->Playwright.evaluateString(
                             "Array.from(document.querySelectorAll('.thread-item')).map(el => (el.id || '').replace('thread-item-', '')).join(', ')"
                           )
                         )
                      |> then_(threadIdsAfterRefresh => {
                           let stillPresent = threadIdsAfterRefresh->includes(firstThreadId);
                           BrowserTestUtils.assertTrue(
                             ~label="Deleted thread id absent after refresh",
                             !stillPresent,
                             ~details=
                               "Deleted thread id: " ++ firstThreadId ++ ", Found ids: " ++ threadIdsAfterRefresh,
                           );
                         })
                      |> then_(_ => {
                           /* Prove websocket is still connected by creating a new thread */
                           Js.log("Creating new thread to prove websocket is alive...");
                           page->Playwright.click("#new-thread-button")
                           |> then_(_ => BrowserTestUtils.sleep(800))
                           |> then_(_ =>
                                page->Playwright.evaluateString(
                                  "document.querySelectorAll('.thread-item').length.toString()"
                                )
                              )
                           |> then_(countAfterCreate => {
                                let count = int_of_string(countAfterCreate);
                                BrowserTestUtils.assertTrue(
                                  ~label="New thread created successfully over websocket",
                                  count > 0,
                                  ~details="Thread count after create: " ++ countAfterCreate,
                                );
                              })
                           })
                    })
               )
          )
     )
  |> catch(error => {
       Js.log2("[FAIL] Thread deletion test failed:", error);
       BrowserTestUtils.rejectPromiseError(error);
     })
};

let runDeleteInactiveThreadKeepsActiveScenario = (~browser, ~baseUrl) => {
  Js.log("Running inactive thread deletion scenario...");
  browser
  ->Playwright.newPage
  |> then_(page =>
       page
       ->Playwright.goto(baseUrl ++ "/")
       |> then_(_ => page->Playwright.waitForSelector("#thread-list"))
       |> then_(_ => page->Playwright.click("#new-thread-button"))
       |> then_(_ => BrowserTestUtils.sleep(800))
       |> then_(_ => readCurrentThreadId(page))
       |> then_(inactiveThreadId =>
            page->Playwright.click("#new-thread-button")
            |> then_(_ => BrowserTestUtils.sleep(800))
            |> then_(_ => readCurrentThreadId(page))
            |> then_(activeThreadId =>
                 page->Playwright.click("#delete-thread-" ++ inactiveThreadId)
                 |> then_(_ => BrowserTestUtils.sleep(1000))
                 |> then_(_ => readCurrentThreadId(page))
                 |> then_(afterDeleteThreadId =>
                      BrowserTestUtils.assertTrue(
                        ~label="Inactive delete: active route is preserved",
                        afterDeleteThreadId == activeThreadId,
                        ~details=
                          "Expected active thread "
                          ++ activeThreadId
                          ++ " after deleting inactive thread "
                          ++ inactiveThreadId
                          ++ ", got "
                          ++ afterDeleteThreadId,
                      )
                    )
                 |> then_(_ =>
                      waitForExpressionTrue(
                        ~page,
                        ~expression=
                          "document.querySelector('.thread-item--active')?.id === 'thread-item-"
                          ++ activeThreadId
                          ++ "' ? 'true' : 'false'",
                        ~label="Inactive delete: active indicator remains on current thread",
                        ~attemptsLeft=50,
                      )
                    )
                 |> then_(_ =>
                      waitForExpressionTrue(
                        ~page,
                        ~expression=
                          "document.querySelector('#thread-item-"
                          ++ inactiveThreadId
                          ++ "') === null ? 'true' : 'false'",
                        ~label="Inactive delete: deleted thread removed from list",
                        ~attemptsLeft=50,
                      )
                    )
               )
          )
     )
  |> catch(error => {
       Js.log2("[FAIL] Inactive thread deletion test failed:", error);
       BrowserTestUtils.rejectPromiseError(error);
     });
};

let runDbSyncScenario = (~browser, ~baseUrl) => {
  Js.log("Running DB sync scenario (delete thread via server endpoint)...");
  browser
  ->Playwright.newPage
  |> then_(page =>
       page
       ->Playwright.goto(baseUrl ++ "/")
       |> then_(_ => page->Playwright.waitForSelector("#thread-list"))
       |> then_(_ =>
            waitForSelectorText(
              ~page,
              ~selector="#thread-list",
              ~expected="New Chat",
              ~label="DB sync: thread list renders",
              ~attemptsLeft=50,
            )
          )
       /* Count threads before DB deletion */
       |> then_(_ =>
            page->Playwright.evaluateString(
              "document.querySelectorAll('.thread-item').length.toString()"
            )
          )
       |> then_(countBefore => {
            /* Delete the thread directly via the test endpoint (simulates DB deletion) */
            readCurrentThreadId(page)
            |> then_(threadId => {
                 Js.log2("DB sync: deleting thread", threadId);
                 page
                 ->Playwright.evaluateString(
                   baseUrl ++ "/api/test/delete-thread/" ++ threadId
                   |> js => "fetch('" ++ js ++ "', { method: 'POST' }).then(r => r.text())"
                 )
                 |> then_(_ => BrowserTestUtils.sleep(1000))
                 |> then_(_ =>
                      page->Playwright.evaluateString(
                        "document.querySelectorAll('.thread-item').length.toString()"
                      )
                    )
                 |> then_(countAfter => {
                      let before = int_of_string(countBefore);
                      let after_ = int_of_string(countAfter);
                      BrowserTestUtils.assertTrue(
                        ~label="DB sync: thread removed from UI after direct DB deletion",
                        after_ < before,
                        ~details=
                          "Before: " ++ countBefore ++ ", After: " ++ countAfter,
                      );
                    })
               })
          })
    )
  |> catch(error => {
       Js.log2("[FAIL] DB sync test failed:", error);
       BrowserTestUtils.rejectPromiseError(error);
     })
};

let runDbCreateSyncScenario = (~browser, ~baseUrl) => {
  Js.log("Running DB create sync scenario...");
  browser
  ->Playwright.newPage
  |> then_(page =>
       page
       ->Playwright.goto(baseUrl ++ "/")
       |> then_(_ => page->Playwright.waitForSelector("#thread-list"))
       |> then_(_ =>
            waitForSelectorText(
              ~page,
              ~selector="#thread-list",
              ~expected="New Chat",
              ~label="DB create sync: thread list renders",
              ~attemptsLeft=50,
            )
          )
       |> then_(_ =>
            page->Playwright.evaluateString(
              "document.querySelectorAll('.thread-item').length.toString()"
            )
          )
       |> then_(countBefore => {
            let url = baseUrl ++ "/api/test/create-thread";
            let script =
              "fetch('"
              ++ url
              ++ "', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: '{\"title\": \"DB Sync Test Thread\"}' }).then(r => r.json()).then(j => JSON.stringify(j))";
            page
            ->Playwright.evaluateString(script)
            |> then_(response => {
                 Js.log2("DB create sync: endpoint response", response);
                 /* Verify endpoint returned success with an id */
                 BrowserTestUtils.assertTrue(
                   ~label="DB create sync: endpoint returned success",
                   response->includes("\"id\""),
                   ~details="Expected endpoint response to contain id field",
                 );
               })
            |> then_(_ => BrowserTestUtils.sleep(1000))
            |> then_(_ =>
                 page->Playwright.evaluateString(
                   "document.querySelectorAll('.thread-item').length.toString()"
                 )
               )
            |> then_(countAfter => {
                 let before = int_of_string(countBefore);
                 let after_ = int_of_string(countAfter);
                 BrowserTestUtils.assertTrue(
                   ~label="DB create sync: thread appeared in UI",
                   after_ > before,
                   ~details="Before: " ++ countBefore ++ ", After: " ++ countAfter,
                 );
               })
          })
     )
  |> catch(error => {
       Js.log2("[FAIL] DB create sync test failed:", error);
       BrowserTestUtils.rejectPromiseError(error);
     })
};

let runUiCreateSyncScenario = (~browser, ~baseUrl) => {
  Js.log("Running UI create sync scenario...");
  browser
  ->Playwright.newPage
  |> then_(pageA =>
       pageA
       ->Playwright.goto(baseUrl ++ "/")
       |> then_(_ => pageA->Playwright.waitForSelector("#thread-list"))
       |> then_(_ => pageA->Playwright.evaluateString("window.location.href"))
       |> then_(originalThreadUrl =>
            browser
            ->Playwright.newPage
            |> then_(pageB =>
                 pageB
                 ->Playwright.goto(originalThreadUrl)
                 |> then_(_ => pageB->Playwright.waitForSelector("#thread-list"))
                 |> then_(_ =>
                      pageB->Playwright.evaluateString(
                        "document.querySelectorAll('.thread-item').length.toString()"
                      )
                    )
                 |> then_(countBefore =>
                      pageA->Playwright.click("#new-thread-button")
                      |> then_(_ =>
                           waitForExpressionTrue(
                             ~page=pageA,
                             ~expression="window.location.pathname !== '/' && document.querySelector('#empty-thread-state') !== null ? 'true' : 'false'",
                             ~label="UI create sync: creator navigates to new empty thread",
                             ~attemptsLeft=50,
                           )
                         )
                      |> then_(_ =>
                           waitForExpressionTrue(
                             ~page=pageB,
                             ~expression=
                               "(() => parseInt(document.querySelectorAll('.thread-item').length.toString(), 10) > "
                               ++ countBefore
                               ++ ")().toString()",
                             ~label="UI create sync: other tab receives new thread in list",
                             ~attemptsLeft=100,
                           )
                         )
                      |> then_(_ =>
                           pageA->Playwright.evaluateString(
                             "(() => window.location.pathname.split('/').filter(Boolean).slice(-1)[0] || '')()"
                           )
                         )
                      |> then_(threadId => {
                           let url = baseUrl ++ "/api/test/add-message";
                           let body =
                             "{\"thread_id\": \""
                             ++ threadId
                             ++ "\", \"role\": \"assistant\", \"content\": \"UI create sync message\"}";
                           let script =
                             "fetch('"
                             ++ url
                             ++ "', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: '"
                             ++ body
                             ++ "' }).then(r => r.json()).then(j => JSON.stringify(j))";
                           pageA
                           ->Playwright.evaluateString(script)
                           |> then_(_ =>
                                waitForSelectorText(
                                  ~page=pageA,
                                  ~selector="#message-list",
                                  ~expected="UI create sync message",
                                  ~label="UI create sync: creator receives realtime message on new thread",
                                  ~attemptsLeft=100,
                                )
                              )
                         })
                    )
               )
          )
     )
  |> catch(error => {
       Js.log2("[FAIL] UI create sync test failed:", error);
       BrowserTestUtils.rejectPromiseError(error);
     })
};

let runMessageSyncScenario = (~browser, ~baseUrl) => {
  Js.log("Running message sync scenario...");
  browser
  ->Playwright.newPage
  |> then_(page =>
       page
       ->Playwright.goto(baseUrl ++ "/")
       |> then_(_ => page->Playwright.waitForSelector("#thread-list"))
       |> then_(_ => page->Playwright.waitForSelector("#prompt-input"))
       |> then_(_ => page->Playwright.fill("#prompt-input", "Message sync test prompt"))
       |> then_(_ => page->Playwright.click("#send-button"))
       |> then_(_ =>
            waitForSelectorText(
              ~page,
              ~selector="#message-list",
              ~expected="Message sync test prompt",
              ~label="Message sync: user message appears after send",
              ~attemptsLeft=50,
            )
          )
       |> then_(_ => readCurrentThreadId(page))
       |> then_(threadId => {
            let url = baseUrl ++ "/api/test/add-message";
            let body =
              "{\"thread_id\": \""
              ++ threadId
              ++ "\", \"role\": \"assistant\", \"content\": \"DB sync test message\"}";
            let script =
              "fetch('"
              ++ url
              ++ "', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: '"
              ++ body
              ++ "' }).then(r => r.json()).then(j => JSON.stringify(j))";
            page
            ->Playwright.evaluateString(script)
            |> then_(response => {
                 Js.log2("Message sync: endpoint response", response);
                 BrowserTestUtils.assertTrue(
                   ~label="Message sync: endpoint returned success",
                   response->includes("\"id\""),
                   ~details="Expected endpoint response to contain id field",
                 );
               })
            |> then_(_ => BrowserTestUtils.sleep(1000))
            |> then_(_ =>
                 waitForSelectorText(
                   ~page,
                   ~selector="#message-list",
                   ~expected="DB sync test message",
                   ~label="Message sync: DB-inserted message appears in UI",
                   ~attemptsLeft=50,
                 )
               )
          })
     )
  |> catch(error => {
       Js.log2("[FAIL] Message sync test failed:", error);
       BrowserTestUtils.rejectPromiseError(error);
     })
};

let runEmptyStateScenario = (~browser, ~baseUrl) => {
  Js.log("Running empty state scenario...");
  browser
  ->Playwright.newPage
  |> then_(page => {
       page->Playwright.goto(baseUrl ++ "/")
       |> then_(_ => {
            let url = baseUrl ++ "/api/test/create-thread";
            let script =
              "fetch('"
              ++ url
              ++ "', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: '{\"title\": \"Empty State Test\"}' }).then(r => r.json()).then(j => j.id)";
            page->Playwright.evaluateString(script);
          })
       |> then_(threadId => {
            page->Playwright.goto(baseUrl ++ "/" ++ threadId)
            |> then_(_ => page->Playwright.waitForSelector("#prompt-input"))
            |> then_(_ =>
                 waitForSelectorText(
                   ~page,
                   ~selector="#empty-thread-state",
                   ~expected="Send a message to start the conversation",
                   ~label="Empty state: new thread shows start conversation prompt",
                   ~attemptsLeft=50,
                 )
               )
            |> then_(_ => page->Playwright.fill("#prompt-input", "   "))
            |> then_(_ => page->Playwright.click("#send-button"))
            |> then_(_ => BrowserTestUtils.sleep(300))
            |> then_(_ =>
                 page->Playwright.evaluateString(
                   "document.querySelectorAll('.message--user').length.toString()"
                 )
               )
            |> then_(userMessageCount =>
                 BrowserTestUtils.assertTrue(
                   ~label="Empty state: whitespace-only prompt is ignored",
                   userMessageCount == "0",
                   ~details="Expected no user messages after submitting whitespace-only input",
                 )
               )
            |> then_(_ => page->Playwright.fill("#prompt-input", "First message"))
            |> then_(_ => page->Playwright.click("#send-button"))
            |> then_(_ =>
                 waitForSelectorText(
                   ~page,
                   ~selector="#message-list",
                   ~expected="First message",
                   ~label="Empty state: message appears after send",
                   ~attemptsLeft=50,
                 )
               )
            |> then_(_ =>
                 page->Playwright.evaluateString(
                   "document.querySelector('#empty-thread-state') === null ? 'true' : 'false'"
                 )
               )
            |> then_(emptyGone =>
                 BrowserTestUtils.assertTrue(
                   ~label="Empty state: prompt disappears after first message",
                   emptyGone == "true",
                   ~details="Expected empty-thread-state to be removed from DOM",
                 )
               )
          });
     })
  |> catch(error => {
       Js.log2("[FAIL] Empty state test failed:", error);
       BrowserTestUtils.rejectPromiseError(error);
     });
};

let runDeleteAllThreadsScenario = (~browser, ~baseUrl) => {
  Js.log("Running delete all threads scenario...");
  browser
  ->Playwright.newPage
  |> then_(page =>
       page
       ->Playwright.goto(baseUrl ++ "/")
       |> then_(_ => page->Playwright.waitForSelector("#thread-list"))
       |> then_(_ =>
            page->Playwright.evaluateString(
              "fetch('" ++ baseUrl ++ "/api/test/delete-all-threads', { method: 'POST' }).then(r => r.text())"
            )
          )
       |> then_(_ => BrowserTestUtils.sleep(3000))
       |> then_(_ =>
            waitForSelectorText(
              ~page,
              ~selector="#no-threads-state",
              ~expected="No conversations yet",
              ~label="Delete all threads: UI shows no conversations message",
              ~attemptsLeft=100,
            )
          )
       |> then_(_ =>
            page->Playwright.evaluateString(
              "document.querySelector('#prompt-input') === null ? 'true' : 'false'"
            )
          )
       |> then_(inputHidden =>
           BrowserTestUtils.assertTrue(
             ~label="Delete all threads: prompt input is hidden",
             inputHidden == "true",
             ~details="Expected prompt-input to be removed when no active thread",
            )
          )
     );
};

let runCrossTabDeleteActiveThreadScenario = (~browser, ~baseUrl) => {
  Js.log("Running cross-tab delete active thread scenario...");
  browser
  ->Playwright.newPage
  |> then_(pageA => {
       pageA
       ->Playwright.goto(baseUrl ++ "/")
       |> then_(_ => pageA->Playwright.waitForSelector("#thread-list"))
       |> then_(_ => pageA->Playwright.evaluateString("window.location.href"))
       |> then_(threadUrl => {
            browser
            ->Playwright.newPage
            |> then_(pageB => {
                 pageB
                 ->Playwright.goto(threadUrl)
                 |> then_(_ => pageB->Playwright.waitForSelector("#thread-list"))
                 |> then_(_ => readCurrentThreadId(pageB))
                 |> then_(threadId => {
                      pageB
                      ->Playwright.click("#delete-thread-" ++ threadId ++ "")
                      |> then_(_ => BrowserTestUtils.sleep(1000))
                      |> then_(_ =>
                           pageA->Playwright.evaluateString(
                             "document.querySelectorAll('.thread-item').length.toString()"
                           )
                         )
                      |> then_(threadCount => {
                           let count = int_of_string(threadCount);
                           if (count == 0) {
                             waitForSelectorText(
                               ~page=pageA,
                               ~selector="#no-threads-state",
                               ~expected="No conversations yet",
                               ~label="Cross-tab delete: shows no threads message when last thread deleted",
                               ~attemptsLeft=50,
                             )
                             |> then_(_ =>
                                  pageA->Playwright.evaluateString(
                                    "document.querySelector('#prompt-input') === null ? 'true' : 'false'"
                                  )
                                )
                             |> then_(inputHidden =>
                                  BrowserTestUtils.assertTrue(
                                    ~label="Cross-tab delete: prompt input hidden when no threads",
                                    inputHidden == "true",
                                    ~details="Expected prompt-input to be removed when no active thread",
                                  )
                                )
                           } else {
                             waitForExpressionTrue(
                               ~page=pageA,
                               ~expression="window.location.pathname !== '/' ? 'true' : 'false'",
                               ~label="Cross-tab delete: navigated to remaining thread",
                               ~attemptsLeft=50,
                             )
                             |> then_(_ =>
                                  waitForExpressionTrue(
                                    ~page=pageA,
                                    ~expression="document.querySelector('.thread-item--active') !== null ? 'true' : 'false'",
                                    ~label="Cross-tab delete: active thread indicator present on remaining thread",
                                    ~attemptsLeft=50,
                                  )
                                )
                           }
                         })
                    })
            })
       })
  })
  |> catch(error => {
       Js.log2("[FAIL] Cross-tab delete active thread test failed:", error);
       BrowserTestUtils.rejectPromiseError(error);
     });
};

let runReconnectionScenario = (~browser, ~baseUrl, ~serverRef) => {
  Js.log("Running WebSocket reconnection scenario...");
  let prompt = "Reconnection test prompt";

  browser
  ->Playwright.newPage
  |> then_(page =>
       page
       ->Playwright.goto(baseUrl ++ "/")
       |> then_(_ => page->Playwright.waitForSelector("#prompt-input"))
       |> then_(_ => page->Playwright.fill("#prompt-input", prompt))
       |> then_(_ => page->Playwright.click("#send-button"))
       |> then_(_ =>
            waitForSelectorText(
              ~page,
              ~selector="#message-list",
              ~expected=prompt,
              ~label="Reconnection: initial message sent successfully",
              ~attemptsLeft=50,
            )
          )
       |> then_(_ => readCurrentThreadId(page))
       |> then_(threadId => {
            Js.log2("Reconnection: threadId", threadId);
            /* Stop server */
            switch (serverRef.contents) {
            | Some(server) =>
                LlmChatTestServer.stop(server)
                |> then_(_ => {
                     serverRef := None;
                     Js.log("Reconnection: server stopped");
                     let msgId = UUID.make();
                     let sql =
                       "INSERT INTO messages (id, thread_id, role, content) VALUES ('"
                       ++ msgId
                       ++ "', '"
                       ++ threadId
                       ++ "', 'assistant', 'Message while disconnected')";
                     execSql(sql);
                     BrowserTestUtils.sleep(1000);
                   })
                |> then_(_ => {
                     Js.log("Reconnection: restarting server...");
                     LlmChatTestServer.start();
                   })
                |> then_(newServer => {
                     serverRef := Some(newServer);
                     BrowserTestUtils.sleep(2500)
                     |> then_(_ =>
                          waitForSelectorText(
                            ~page,
                            ~selector="#message-list",
                            ~expected="Message while disconnected",
                            ~label="Reconnection: client received missed message after reconnect",
                            ~attemptsLeft=100,
                          )
                        )
                   })
            | None =>
                reject(BrowserTestUtils.makeError("Reconnection: no server to stop"))
            };
          })
     )
  |> catch(error => {
       Js.log2("[FAIL] Reconnection test failed:", error);
       BrowserTestUtils.rejectPromiseError(error);
     });
};

let run = () => {
  let launchOptions = Playwright.makeLaunchOptions(~headless=true, ());
  let browserRef = ref(None);
  let serverRef = ref(None);
  let mockRef = ref(None);

  removeMalformedSendPromptLedger();
  MockOllama.start()
  |> then_(mock => {
       mockRef := Some(mock);
       LlmChatTestServer.start();
     })
  |> then_(server => {
       serverRef := Some(server);
       Playwright.chromium->Playwright.launch(launchOptions);
     })
  |> then_(browser => {
       browserRef := Some(browser);
       switch (serverRef.contents) {
       | Some(server) =>
           runRootRedirectScenario(~browser, ~baseUrl=server.baseUrl)
           |> then_(_ =>
                runEmptyStateScenario(~browser, ~baseUrl=server.baseUrl)
                |> then_(_ => runThreadListAndInputScenario(~browser, ~baseUrl=server.baseUrl))
                |> then_(threadUrl =>
                     runMessageDisplayScenario(~browser, ~threadUrl)
                     |> then_(_ => runCrossTabRealtimeSyncScenario(~browser, ~baseUrl=server.baseUrl))
                     |> then_(_ =>
                          switch (mockRef.contents) {
                          | Some(mock) =>
                              runSseStreamingScenario(
                                ~browser,
                                ~baseUrl=server.baseUrl,
                                ~mock,
                              )
                              |> then_(_ =>
                                   runOllamaStreamingScenario(
                                     ~browser,
                                     ~baseUrl=server.baseUrl,
                                     ~mock,
                                   )
                                 )
                          | None =>
                              reject(BrowserTestUtils.makeError("mock Ollama server was not initialized"))
                          }
                        )
                     |> then_(_ => runStreamingScrollScenario(~browser, ~baseUrl=server.baseUrl))
                     |> then_(_ =>
                          runStreamingScrollOptOutScenario(
                            ~browser,
                            ~baseUrl=server.baseUrl,
                          )
                        )
                     |> then_(_ => runThreadDeletionScenario(~browser, ~baseUrl=server.baseUrl))
                     |> then_(_ =>
                          runDeleteInactiveThreadKeepsActiveScenario(
                            ~browser,
                            ~baseUrl=server.baseUrl,
                          )
                        )
                     |> then_(_ => runDbSyncScenario(~browser, ~baseUrl=server.baseUrl))
                     |> then_(_ => runDbCreateSyncScenario(~browser, ~baseUrl=server.baseUrl))
                     |> then_(_ => runUiCreateSyncScenario(~browser, ~baseUrl=server.baseUrl))
                     |> then_(_ => runMessageSyncScenario(~browser, ~baseUrl=server.baseUrl))
                     |> then_(_ => runDeleteAllThreadsScenario(~browser, ~baseUrl=server.baseUrl))
                     |> then_(_ =>
                          runCrossTabDeleteActiveThreadScenario(~browser, ~baseUrl=server.baseUrl)
                        )
                     |> then_(_ =>
                          runReconnectionScenario(~browser, ~baseUrl=server.baseUrl, ~serverRef)
                        )
                       )
                 )
       | None => reject(BrowserTestUtils.makeError("server was not initialized"))
       };
     })
  |> then_(result =>
       cleanup(~browser=browserRef.contents, ~server=serverRef.contents, ~mock=mockRef.contents)
       |> then_(_ => resolve(result))
     )
  |> catch(error =>
       cleanup(~browser=browserRef.contents, ~server=serverRef.contents, ~mock=mockRef.contents)
       |> then_(_ => BrowserTestUtils.rejectPromiseError(error))
     );
};

let () =
   run()
   |> then_(_ => {
       Js.log("LLM Chat browser tests passed!");
       BrowserTestUtils.exitProcess(0);
       resolve();
     })
   |> catch(error => {
       Js.log2("LLM Chat browser tests failed:", error);
       BrowserTestUtils.exitProcess(1);
       resolve();
     })
   |> ignore;
