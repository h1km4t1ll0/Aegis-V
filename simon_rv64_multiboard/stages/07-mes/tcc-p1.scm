;;; Hart 1 / slot 1: tccgen.c
(primitive-load "tcc-mescc-boot.scm")

(define (main args)
  (compile-1 "tccgen.c" "tccgen.s")
  (display "tcc-p1-ok")
  (newline))
