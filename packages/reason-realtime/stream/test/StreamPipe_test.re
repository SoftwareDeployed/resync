open StreamPipe;

let describe = Test_framework.describe;
let test = Test_framework.test;
let pass = Test_framework.pass;
let fail = Test_framework.fail;

let init = () => {
  describe("StreamPipe", () => {
    test("make + subscribe receives values", () => {
      let received = ref([||]);
      let pipe = make(~subscribe=(next => {
        next(1);
        next(2);
        next(3);
        () => ();
      }));
      let _unsubscribe = subscribe(pipe, a => {
        received := Js.Array.concat(~other=[|a|], received^);
      });
      if (Array.length(received^) == 3
          && Array.get(received^, 0) == 1
          && Array.get(received^, 1) == 2
          && Array.get(received^, 2) == 3) {
        pass()
      } else {
        fail("Expected [1, 2, 3]")
      };
    });

    test("map doubles each value", () => {
      let received = ref([||]);
      let pipe = map(
        make(~subscribe=(next => {
          next(1);
          next(2);
          next(3);
          () => ();
        })),
        a => a * 2,
      );
      let _unsubscribe = subscribe(pipe, a => {
        received := Js.Array.concat(~other=[|a|], received^);
      });
      if (Array.length(received^) == 3
          && Array.get(received^, 0) == 2
          && Array.get(received^, 1) == 4
          && Array.get(received^, 2) == 6) {
        pass()
      } else {
        fail("Expected [2, 4, 6]")
      };
    });

    test("filterMap keeps only even numbers and halves them", () => {
      let received = ref([||]);
      let pipe = filterMap(
        make(~subscribe=(next => {
          next(1);
          next(2);
          next(3);
          next(4);
          () => ();
        })),
        a => {
          if (a mod 2 == 0) {
            Some(a / 2)
          } else {
            None
          }
        },
      );
      let _unsubscribe = subscribe(pipe, a => {
        received := Js.Array.concat(~other=[|a|], received^);
      });
      if (Array.length(received^) == 2
          && Array.get(received^, 0) == 1
          && Array.get(received^, 1) == 2) {
        pass()
      } else {
        fail("Expected [1, 2]")
      };
    });

    test("tap runs side effect without changing values", () => {
      let sideEffect = ref([||]);
      let received = ref([||]);
      let pipe = tap(
        make(~subscribe=(next => {
          next(1);
          next(2);
          () => ();
        })),
        a => {
          sideEffect := Js.Array.concat(~other=[|a|], sideEffect^);
        },
      );
      let _unsubscribe = subscribe(pipe, a => {
        received := Js.Array.concat(~other=[|a|], received^);
      });
      if (Array.length(received^) == 2
          && Array.get(received^, 0) == 1
          && Array.get(received^, 1) == 2
          && Array.length(sideEffect^) == 2
          && Array.get(sideEffect^, 0) == 1
          && Array.get(sideEffect^, 1) == 2) {
        pass()
      } else {
        fail("tap did not work correctly")
      };
    });

    test("nested map + filterMap compose correctly", () => {
      let received = ref([||]);
      let pipe = filterMap(
        map(
          make(~subscribe=(next => {
            next(1);
            next(2);
            next(3);
            next(4);
            () => ();
          })),
          a => a * 3,
        ),
        a => {
          if (a mod 2 == 0) {
            Some(a / 2)
          } else {
            None
          }
        },
      );
      let _unsubscribe = subscribe(pipe, a => {
        received := Js.Array.concat(~other=[|a|], received^);
      });
      /* 1*3=3 (odd, dropped), 2*3=6 -> 3, 3*3=9 (odd, dropped), 4*3=12 -> 6 */
      if (Array.length(received^) == 2
          && Array.get(received^, 0) == 3
          && Array.get(received^, 1) == 6) {
        pass()
      } else {
        fail("Expected [3, 6]")
      };
    });
  });
};
