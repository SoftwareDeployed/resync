let get = name =>
  switch (name) {
  | "kanban" => Some(LucideGeneratedNodes_K.kanban)
  | "kayak" => Some(LucideGeneratedNodes_K.kayak)
  | "key-round" => Some(LucideGeneratedNodes_K.key_round)
  | "key-square" => Some(LucideGeneratedNodes_K.key_square)
  | "key" => Some(LucideGeneratedNodes_K.key)
  | "keyboard-music" => Some(LucideGeneratedNodes_K.keyboard_music)
  | "keyboard-off" => Some(LucideGeneratedNodes_K.keyboard_off)
  | "keyboard" => Some(LucideGeneratedNodes_K.keyboard)
  | "kanban-square-dashed" => Some(LucideGeneratedNodes_S.square_dashed_kanban)
  | "kanban-square" => Some(LucideGeneratedNodes_S.square_kanban)
  | _ => None
  };
