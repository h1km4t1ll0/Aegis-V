;;; Hart 3 / slot 3: RISC-V backends + tccasm + tcc.c
(primitive-load "tcc-mescc-boot.scm")

(define (main args)
  (compile-1 "riscv64-gen.c" "riscv64-gen.s")
  (compile-1 "riscv64-link.c" "riscv64-link.s")
  (compile-1 "riscv64-asm.c" "riscv64-asm.s")
  (compile-1 "tccasm.c" "tccasm.s")
  (compile-1 "tcc.c" "tcc.s")
  (display "tcc-p3-ok")
  (newline))
