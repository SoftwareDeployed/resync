(lang dune 3.21)

(name resync)

(generate_opam_files true)

(source
 (github SoftwareDeployed/resync))

(authors "Brian Kaplan <brian@equalslashed.net>")

(maintainers "Brian Kaplan <brian@equalslashed.net>")

(license MIT)

(using melange 1.0)

(pin
 (package
  (name gluten-lwt)
  (version 0.5.2))
 (url
  "git+https://github.com/futurGH/gluten.git#956e753dc0ee81dbe53f0979fe89d57d960e7856"))

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
 (url
  "git+https://github.com/SoftwareDeployed/reason-react-day-picker.git#reason-native")
 (package
  (name reason-react-day-picker)))

(package
 (name resync)
 (synopsis "Realtime Universal Reason React")
 (description
  "Tools for creating a realtime universal reason react application")
 (depends
  (ocaml
   (= "5.4.1"))
  (caqti-lwt
   (= "2.2.4"))
  (lwt
   (= "5.9.2"))
  caqti-driver-postgresql
  dream
  melange
  melange-json
  melange-json-native
  ocaml-lsp-server
  ocamlformat
  reason
  melange-webapi
  ppxlib
  ctypes-foreign
  reason-react-ppx
   reason-react
   reason-react-day-picker
   server-reason-react
   uuidm
   (alcotest :with-test)
   (alcobar :with-test)
   (crowbar :with-test))
  (allow_empty)
 (tags
  ("web" "commerce" "lease" "subscription" "cart")))

; See the complete stanza docs at https://dune.readthedocs.io/en/stable/reference/dune-project/index.html
