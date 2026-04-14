// TodoMutations.re - Mutation modules for the todo-multiplayer demo
// 
// These modules are created at the component level with store pre-captured
// This avoids calling hooks inside event handlers

let makeAddTodo = store => {
  let dispatch = text => TodoStore.addTodo(store, text);
  dispatch;
};

let makeToggleTodo = store => {
  let dispatch = id => TodoStore.toggleTodo(store, id);
  dispatch;
};

let makeRemoveTodo = store => {
  let dispatch = id => TodoStore.removeTodo(store, id);
  dispatch;
};

let makeFailServerMutation = store => {
  let dispatch = () => TodoStore.failServerMutation(store);
  dispatch;
};

let makeFailClientMutation = store => {
  let dispatch = () => TodoStore.failClientMutation(store);
  dispatch;
};
