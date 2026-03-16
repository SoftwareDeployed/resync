let iconNodeOfName = name =>
  if (String.length(name) == 0) {
    None;
  } else {
    switch (Char.lowercase_ascii(String.get(name, 0))) {
    | '0'
    | '1'
    | '2'
    | '3'
    | '4'
    | '5'
    | '6'
    | '7'
    | '8'
    | '9' => LucideGeneratedLookup_0_9.get(name)
    | 'a' => LucideGeneratedLookup_A.get(name)
    | 'b' => LucideGeneratedLookup_B.get(name)
    | 'c' => LucideGeneratedLookup_C.get(name)
    | 'd' => LucideGeneratedLookup_D.get(name)
    | 'e' => LucideGeneratedLookup_E.get(name)
    | 'f' => LucideGeneratedLookup_F.get(name)
    | 'g' => LucideGeneratedLookup_G.get(name)
    | 'h' => LucideGeneratedLookup_H.get(name)
    | 'i' => LucideGeneratedLookup_I.get(name)
    | 'j' => LucideGeneratedLookup_J.get(name)
    | 'k' => LucideGeneratedLookup_K.get(name)
    | 'l' => LucideGeneratedLookup_L.get(name)
    | 'm' => LucideGeneratedLookup_M.get(name)
    | 'n' => LucideGeneratedLookup_N.get(name)
    | 'o' => LucideGeneratedLookup_O.get(name)
    | 'p' => LucideGeneratedLookup_P.get(name)
    | 'q' => LucideGeneratedLookup_Q.get(name)
    | 'r' => LucideGeneratedLookup_R.get(name)
    | 's' => LucideGeneratedLookup_S.get(name)
    | 't' => LucideGeneratedLookup_T.get(name)
    | 'u' => LucideGeneratedLookup_U.get(name)
    | 'v' => LucideGeneratedLookup_V.get(name)
    | 'w' => LucideGeneratedLookup_W.get(name)
    | 'x' => LucideGeneratedLookup_X.get(name)
    | 'y' => LucideGeneratedLookup_Y.get(name)
    | 'z' => LucideGeneratedLookup_Z.get(name)
    | _ => None
    };
  };

let isValidName = name =>
  switch (iconNodeOfName(name)) {
  | Some(_) => true
  | None => false
  };
