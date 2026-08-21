;;; Compile mes libc+tcc into one .s for the TCC link.
(primitive-load "tcc-mescc-boot.scm")

(define (main args)
  (compile-1 "libc-tcc.c" "libc-tcc.s")
  (display "libc-ok")
  (newline))
