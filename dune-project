(lang dune 3.22)

(name executor)

(generate_opam_files true)

(source
 (github Software-Deployed/executor))

(authors "Brian Kaplan <brian@equalslashed.net>")

(license MIT)

(using melange 0.1)

(pin
 (url "git+https://github.com/ml-in-barcelona/server-reason-react.git")
 (package
  (name server-reason-react)))

(pin
 (url "git+https://github.com/reasonml/reason-react.git")
 (package
  (name reason-react)))

(pin
 (url "git+https://github.com/reasonml/reason-react.git")
 (package
  (name reason-react-ppx)))

(pin
 (url "git+https://github.com/Software-Deployed/reason-react-day-picker.git")
 (package
  (name reason-react-day-picker)))

(package
 (name executor)
 (synopsis "Realtime e-commerce platform")
 (description "E-commerce with real time transactions and synchronization")
 (depends
  (ocaml
   (= "5.4.0"))
  caqti-driver-postgresql
  dream
  lwt
  melange
  ocaml-lsp-server
  ocamlformat
  reason
  melange-webapi
  ppxlib
  reason-react-ppx
  reason-react
  reason-react-day-picker
  server-reason-react)
 (allow_empty)
 (tags
  ("web" "commerce" "lease" "subscription" "cart")))

; See the complete stanza docs at https://dune.readthedocs.io/en/stable/reference/dune-project/index.html
