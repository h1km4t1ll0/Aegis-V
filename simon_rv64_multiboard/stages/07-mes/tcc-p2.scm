;;; Hart 2 / slot 2: tccelf.c + libtcc.c
(primitive-load "tcc-mescc-boot.scm")

(define (main args)
  (compile-1 "tccelf.c" "tccelf.s")
  (compile-1 "libtcc.c" "libtcc.s")
  (display "tcc-p2-ok")
  (newline))
