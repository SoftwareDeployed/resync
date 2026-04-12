/* StoreGuardTree - Declarative guard trees for state/action validation.

   Guard trees branch on state predicates and determine whether an action
   is allowed or denied. They are fully generic over state and action types
   and can be shared between client and server.

   Example:
     let tree =
       WhenTrue(
         (state: myState) => state.current_thread_id != "",
         AcceptAll,
         DenyIf(
           (action: myAction) =>
             switch (action) {
             | SendPrompt(_) | DeleteThread(_) | SelectThread(_) => true
             | _ => false
             },
           "No active thread",
         ),
       );
   */

type t('state, 'action) =
  | WhenTrue('state => bool, t('state, 'action), t('state, 'action))
  | DenyIf('action => bool, string)
  | AllowIf('action => bool)
  | AcceptAll
  | Pass;

type evalResult =
  | Allow
  | Deny(string)
  | Pass;

let rec evaluate = (~state: 'state, ~action: 'action, tree: t('state, 'action)): evalResult =>
  switch (tree) {
  | WhenTrue(predicate, thenBranch, elseBranch) =>
    if (predicate(state)) {
      evaluate(~state, ~action, thenBranch);
    } else {
      evaluate(~state, ~action, elseBranch);
    }
  | DenyIf(predicate, reason) =>
    if (predicate(action)) { Deny(reason) } else { Pass }
  | AllowIf(predicate) =>
    if (predicate(action)) { Allow } else { Pass }
  | AcceptAll => Allow
  | Pass => Pass
  };

/* Convenience constructors for pipeline usage */

let whenTrue = (
  ~condition: 'state => bool,
  ~then_: t('state, 'action),
  ~else_: t('state, 'action)=Pass,
  (),
): t('state, 'action) =>
  WhenTrue(condition, then_, else_);

let denyIf = (~predicate: 'action => bool, ~reason: string, ()): t('state, 'action) =>
  DenyIf(predicate, reason);

let allowIf = (~predicate: 'action => bool, ()): t('state, 'action) =>
  AllowIf(predicate);

let acceptAll: t('state, 'action) = AcceptAll;
let pass: t('state, 'action) = Pass;

/* Evaluate a tree, treating unresolved Pass as Allow by default.
   This gives a blacklist-friendly default where only explicit Deny blocks. */
let resolve = (~state: 'state, ~action: 'action, tree: t('state, 'action)): StoreRuntimeTypes.guardResult =>
  switch (evaluate(~state, ~action, tree)) {
  | Deny(reason) => Deny(reason)
  | Allow => Allow
  | Pass => Allow
  };
