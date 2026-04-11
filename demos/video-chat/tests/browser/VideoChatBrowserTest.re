open Js.Promise;

[@mel.send]
external includes: (string, string) => bool = "includes";

[@mel.send]
external startsWith: (string, string) => bool = "startsWith";

[@mel.send]
external slice: (string, int) => string = "slice";

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

let rec waitForBodyTextAbsence = (~page, ~unexpected, ~label, ~attemptsLeft) =>
  if (attemptsLeft <= 0) {
    BrowserTestUtils.textOrEmpty(page, "body")
    |> then_(text =>
         reject(
           BrowserTestUtils.makeError(
             label ++ " timed out waiting for body to remove: " ++ unexpected ++ ". Last body: " ++ text,
           ),
         )
       );
  } else {
    BrowserTestUtils.textOrEmpty(page, "body")
    |> then_(text =>
         if (!(text->includes(unexpected))) {
           Js.log("[PASS] " ++ label);
           resolve();
         } else {
           BrowserTestUtils.sleep(100)
           |> then_(_ => waitForBodyTextAbsence(~page, ~unexpected, ~label, ~attemptsLeft=attemptsLeft - 1));
         }
       );
  };

let getMockSnapshotJson = page =>
  page
  ->Playwright.evaluateString(
      "JSON.stringify(globalThis.__RESYNC_VIDEO_CHAT_MEDIA_CAPTURE_MOCK.snapshot())",
    );

let newTestPage = browser =>
  browser
  ->Playwright.newPage
  |> then_(page =>
        page
        ->Playwright.addInitScript(MediaCaptureMock.installScriptSource)
       |> then_(_ => resolve(page))
     );

let requirePage = (label, pageOption) =>
  switch (pageOption) {
  | Some(page) => resolve(page)
  | None => reject(BrowserTestUtils.makeError(label ++ " page was not initialized"))
  };

let readRoomId = page =>
  BrowserTestUtils.textOrEmpty(page, "#room-heading")
  |> then_(heading => {
       let prefix = "Room: ";
       BrowserTestUtils.assertTrue(
         ~label="Room heading prefix",
         heading->startsWith(prefix),
         ~details="expected heading to start with 'Room: ' but got: " ++ heading,
       )
       |> then_(_ => {
            let roomId = heading->slice(String.length(prefix));
            BrowserTestUtils.assertTrue(
              ~label="Created room id present",
              String.length(roomId) > 0,
              ~details="room id was empty",
            )
            |> then_(_ => resolve(roomId));
          });
     });

let runRoomCreationScenario = (~browser, ~baseUrl) => {
  Js.log("Running room creation scenario...");
  newTestPage(browser)
  |> then_(page =>
       page
       ->Playwright.goto(baseUrl ++ "/")
       |> then_(_ => page->Playwright.waitForSelector("#home-page"))
       |> then_(_ => page->Playwright.click("#create-room-button"))
       |> then_(_ => page->Playwright.waitForSelector("#room-page"))
       |> then_(_ => waitForSelectorText(~page, ~selector="#room-peer-count", ~expected="Peers: 1", ~label="Room creation peer count", ~attemptsLeft=50))
       |> then_(_ => readRoomId(page))
       |> then_(roomId =>
             getMockSnapshotJson(page)
             |> then_(snapshotJson =>
                  BrowserTestUtils.assertContains(~label="Mock capture created on room create", ~expected="\"createCount\":1", snapshotJson)
                  |> then_(_ => BrowserTestUtils.assertContains(~label="Mock capture started on room create", ~expected="\"startCount\":1", snapshotJson))
                 |> then_(_ => resolve(roomId))
               )
          )
     );
};

let runJoinLeaveScenario = (~browser, ~baseUrl) => {
  Js.log("Running two-peer join/leave scenario...");
  let roomPageRef = ref(None);

  newTestPage(browser)
  |> then_(roomPage => {
       roomPageRef := Some(roomPage);
       roomPage
       ->Playwright.goto(baseUrl ++ "/")
       |> then_(_ => roomPage->Playwright.waitForSelector("#home-page"))
       |> then_(_ => roomPage->Playwright.click("#create-room-button"))
       |> then_(_ => roomPage->Playwright.waitForSelector("#room-page"))
       |> then_(_ => waitForSelectorText(~page=roomPage, ~selector="#room-peer-count", ~expected="Peers: 1", ~label="Creator joins room", ~attemptsLeft=50))
       |> then_(_ => readRoomId(roomPage));
     })
  |> then_(roomId =>
       newTestPage(browser)
       |> then_(joinPage => {
            requirePage("creator", roomPageRef.contents)
            |> then_(roomPage =>
                 joinPage
                 ->Playwright.goto(baseUrl ++ "/")
                 |> then_(_ => joinPage->Playwright.waitForSelector("#home-page"))
                 |> then_(_ => joinPage->Playwright.fill("#join-room-input", roomId))
                 |> then_(_ => joinPage->Playwright.click("#join-room-button"))
                 |> then_(_ => joinPage->Playwright.waitForSelector("#room-page"))
                 |> then_(_ => waitForSelectorText(~page=joinPage, ~selector="#room-peer-count", ~expected="Peers: 2", ~label="Joiner sees both peers", ~attemptsLeft=50))
                 |> then_(_ => waitForSelectorText(~page=roomPage, ~selector="#room-peer-count", ~expected="Peers: 2", ~label="Creator sees both peers", ~attemptsLeft=50))
                 |> then_(_ => joinPage->Playwright.click("#leave-room-button"))
                 |> then_(_ => joinPage->Playwright.waitForSelector("#home-page"))
                 |> then_(_ => waitForSelectorText(~page=roomPage, ~selector="#room-peer-count", ~expected="Peers: 1", ~label="Creator sees peer leave", ~attemptsLeft=50))
               );
          })
     );
};

let runMediaToggleScenario = (~browser, ~baseUrl) => {
  Js.log("Running media toggle scenario...");
  let roomPageRef = ref(None);

  newTestPage(browser)
  |> then_(roomPage => {
       roomPageRef := Some(roomPage);
       roomPage
       ->Playwright.goto(baseUrl ++ "/")
       |> then_(_ => roomPage->Playwright.waitForSelector("#home-page"))
       |> then_(_ => roomPage->Playwright.click("#create-room-button"))
       |> then_(_ => roomPage->Playwright.waitForSelector("#room-page"))
       |> then_(_ => waitForSelectorText(~page=roomPage, ~selector="#room-peer-count", ~expected="Peers: 1", ~label="Media scenario creator joined", ~attemptsLeft=50))
       |> then_(_ => readRoomId(roomPage));
     })
  |> then_(roomId =>
       newTestPage(browser)
       |> then_(joinPage =>
            requirePage("creator", roomPageRef.contents)
            |> then_(roomPage =>
                 joinPage
                 ->Playwright.goto(baseUrl ++ "/")
                 |> then_(_ => joinPage->Playwright.waitForSelector("#home-page"))
                 |> then_(_ => joinPage->Playwright.fill("#join-room-input", roomId))
                 |> then_(_ => joinPage->Playwright.click("#join-room-button"))
                 |> then_(_ => joinPage->Playwright.waitForSelector("#room-page"))
                 |> then_(_ => waitForSelectorText(~page=joinPage, ~selector="#room-peer-count", ~expected="Peers: 2", ~label="Media scenario joiner connected", ~attemptsLeft=50))
                 |> then_(_ => joinPage->Playwright.click("#toggle-video-button"))
                 |> then_(_ => waitForSelectorText(~page=joinPage, ~selector="#toggle-video-button", ~expected="Video Off", ~label="Local video button updates", ~attemptsLeft=50))
                 |> then_(_ => roomPage->Playwright.waitForSelector("#remote-video-paused-overlay"))
                  |> then_(_ => getMockSnapshotJson(joinPage))
                  |> then_(snapshotJson =>
                       BrowserTestUtils.assertContains(~label="Mock video toggle propagated to seam", ~expected="\"lastVideoEnabled\":false", snapshotJson)
                     )
                  |> then_(_ => joinPage->Playwright.click("#toggle-video-button"))
                  |> then_(_ => waitForSelectorText(~page=joinPage, ~selector="#toggle-video-button", ~expected="Video On", ~label="Local video button restores", ~attemptsLeft=50))
                  |> then_(_ => waitForBodyTextAbsence(~page=roomPage, ~unexpected="Video paused", ~label="Remote paused overlay clears", ~attemptsLeft=50))
                  |> then_(_ => joinPage->Playwright.click("#toggle-audio-button"))
                  |> then_(_ => waitForSelectorText(~page=joinPage, ~selector="#toggle-audio-button", ~expected="Audio Off", ~label="Local audio button updates", ~attemptsLeft=50))
                  |> then_(_ => getMockSnapshotJson(joinPage))
                  |> then_(snapshotJson =>
                       BrowserTestUtils.assertContains(~label="Mock audio toggle propagated to seam", ~expected="\"lastAudioEnabled\":false", snapshotJson)
                     )
                 |> then_(_ => joinPage->Playwright.click("#toggle-audio-button"))
                 |> then_(_ => waitForSelectorText(~page=joinPage, ~selector="#toggle-audio-button", ~expected="Audio On", ~label="Local audio button restores", ~attemptsLeft=50))
               )
          )
     );
};

let runPeerTextChatScenario = (~browser, ~baseUrl) => {
  Js.log("Running peer text chat scenario...");
  let roomPageARef = ref(None);

  newTestPage(browser)
  |> then_(pageA => {
       roomPageARef := Some(pageA);
       pageA
       ->Playwright.goto(baseUrl ++ "/")
       |> then_(_ => pageA->Playwright.waitForSelector("#home-page"))
       |> then_(_ => pageA->Playwright.click("#create-room-button"))
       |> then_(_ => pageA->Playwright.waitForSelector("#room-page"))
       |> then_(_ => waitForSelectorText(~page=pageA, ~selector="#room-peer-count", ~expected="Peers: 1", ~label="Text chat creator joined", ~attemptsLeft=50))
       |> then_(_ => readRoomId(pageA));
     })
  |> then_(roomId =>
       newTestPage(browser)
       |> then_(pageB =>
            requirePage("creator", roomPageARef.contents)
            |> then_(pageA =>
                 pageB
                 ->Playwright.goto(baseUrl ++ "/")
                 |> then_(_ => pageB->Playwright.waitForSelector("#home-page"))
                 |> then_(_ => pageB->Playwright.fill("#join-room-input", roomId))
                 |> then_(_ => pageB->Playwright.click("#join-room-button"))
                 |> then_(_ => pageB->Playwright.waitForSelector("#room-page"))
                 |> then_(_ => waitForSelectorText(~page=pageB, ~selector="#room-peer-count", ~expected="Peers: 2", ~label="Text chat joiner connected", ~attemptsLeft=50))
                 |> then_(_ => waitForSelectorText(~page=pageA, ~selector="#room-peer-count", ~expected="Peers: 2", ~label="Text chat creator sees peer", ~attemptsLeft=50))
                 |> then_(_ => pageA->Playwright.fill("#chat-input", "Hello peer"))
                 |> then_(_ => pageA->Playwright.click("#chat-send"))
                 |> then_(_ => waitForSelectorText(~page=pageB, ~selector="body", ~expected="Hello peer", ~label="Peer receives chat message", ~attemptsLeft=50))
                 |> then_(_ => waitForSelectorText(~page=pageA, ~selector="body", ~expected="Hello peer", ~label="Sender sees own chat message", ~attemptsLeft=50))
               )
           )
     );
};

let runChatIsolationScenario = (~browser, ~baseUrl) => {
  Js.log("Running chat message isolation scenario...");
  let roomPageARef = ref(None);
  let roomPageBRef = ref(None);

  newTestPage(browser)
  |> then_(pageA => {
       roomPageARef := Some(pageA);
       pageA
       ->Playwright.goto(baseUrl ++ "/")
       |> then_(_ => pageA->Playwright.waitForSelector("#home-page"))
       |> then_(_ => pageA->Playwright.click("#create-room-button"))
       |> then_(_ => pageA->Playwright.waitForSelector("#room-page"))
       |> then_(_ => waitForSelectorText(~page=pageA, ~selector="#room-peer-count", ~expected="Peers: 1", ~label="Isolation room 1 creator joined", ~attemptsLeft=50))
       |> then_(_ => readRoomId(pageA));
     })
  |> then_(room1Id =>
       newTestPage(browser)
       |> then_(pageB =>
            requirePage("creator", roomPageARef.contents)
            |> then_(pageA =>
                 pageB
                 ->Playwright.goto(baseUrl ++ "/")
                 |> then_(_ => pageB->Playwright.waitForSelector("#home-page"))
                 |> then_(_ => pageB->Playwright.fill("#join-room-input", room1Id))
                 |> then_(_ => pageB->Playwright.click("#join-room-button"))
                 |> then_(_ => pageB->Playwright.waitForSelector("#room-page"))
                 |> then_(_ => waitForSelectorText(~page=pageB, ~selector="#room-peer-count", ~expected="Peers: 2", ~label="Isolation joiner in room 1", ~attemptsLeft=50))
                 |> then_(_ => waitForSelectorText(~page=pageA, ~selector="#room-peer-count", ~expected="Peers: 2", ~label="Isolation creator sees peer in room 1", ~attemptsLeft=50))
                 |> then_(_ => {
                      roomPageBRef := Some(pageB);
                      resolve(room1Id);
                    })
               )
           )
     )
  |> then_(_room1Id =>
       newTestPage(browser)
       |> then_(pageC =>
            pageC
            ->Playwright.goto(baseUrl ++ "/")
            |> then_(_ => pageC->Playwright.waitForSelector("#home-page"))
            |> then_(_ => pageC->Playwright.click("#create-room-button"))
            |> then_(_ => pageC->Playwright.waitForSelector("#room-page"))
            |> then_(_ => waitForSelectorText(~page=pageC, ~selector="#room-peer-count", ~expected="Peers: 1", ~label="Isolation room 2 created", ~attemptsLeft=50))
            |> then_(_ => requirePage("room1A", roomPageARef.contents))
            |> then_(pageA =>
                 requirePage("room1B", roomPageBRef.contents)
                 |> then_(pageB =>
                      pageA
                      ->Playwright.fill("#chat-input", "Secret room1")
                      |> then_(_ => pageA->Playwright.click("#chat-send"))
                      |> then_(_ => waitForSelectorText(~page=pageB, ~selector="body", ~expected="Secret room1", ~label="Room 1 peer sees message", ~attemptsLeft=50))
                      |> then_(_ => waitForBodyTextAbsence(~page=pageC, ~unexpected="Secret room1", ~label="Room 2 does not see room 1 message", ~attemptsLeft=30))
                    )
               )
          )
     );
};

let cleanup = (~browser, ~server) => {
  let closeBrowser =
    switch (browser) {
    | Some(activeBrowser) => activeBrowser->Playwright.close |> catch(_ => resolve())
    | None => resolve()
    };
  closeBrowser
  |> then_(_ =>
       switch (server) {
       | Some(activeServer) => VideoChatTestServer.stop(activeServer) |> catch(_ => resolve())
       | None => resolve()
       }
     );
};

let run = () => {
  let launchOptions = Playwright.makeLaunchOptions(~headless=true, ());
  let browserRef = ref(None);
  let serverRef = ref(None);

  VideoChatTestServer.start()
  |> then_(server => {
       serverRef := Some(server);
       Playwright.chromium->Playwright.launch(launchOptions);
     })
  |> then_(browser => {
       browserRef := Some(browser);
       switch (serverRef.contents) {
       | Some(server) =>
          runRoomCreationScenario(~browser, ~baseUrl=server.baseUrl)
          |> then_(_ => runJoinLeaveScenario(~browser, ~baseUrl=server.baseUrl))
          |> then_(_ => runMediaToggleScenario(~browser, ~baseUrl=server.baseUrl))
          |> then_(_ => runPeerTextChatScenario(~browser, ~baseUrl=server.baseUrl))
          |> then_(_ => runChatIsolationScenario(~browser, ~baseUrl=server.baseUrl))
         | None => reject(BrowserTestUtils.makeError("server was not initialized"))
       };
     })
  |> then_(result =>
       cleanup(~browser=browserRef.contents, ~server=serverRef.contents)
       |> then_(_ => resolve(result))
     )
  |> catch(error =>
       cleanup(~browser=browserRef.contents, ~server=serverRef.contents)
       |> then_(_ => reject(Obj.magic(error)))
     );
};

let () =
   run()
   |> then_(_ => {
       Js.log("Video chat browser tests passed!");
       BrowserTestUtils.exitProcess(0);
       resolve();
     })
   |> catch(error => {
       Js.log2("Video chat browser tests failed:", error);
       BrowserTestUtils.exitProcess(1);
       resolve();
     })
  |> ignore;
