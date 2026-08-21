;;; Hart 0 / slot 0: hi.c warmup + tccpp.c
(primitive-load "tcc-mescc-boot.scm")

(define (main args)
  (compile-1 "hi.c" "hi.s")
  (compile-1 "tccpp.c" "tccpp.s")
  (display "tcc-p0-ok")
  (newline))
