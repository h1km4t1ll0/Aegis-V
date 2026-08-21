;;; One mes.bin: load mescc once, then -S each TCC TU (no ONE_SOURCE).
(cond-expand
 (mes)
 (guile
  (define %arch (car (string-split %host-type #\-)))
  (define %kernel (car (filter
                        (compose not
                                 (lambda (x) (member x '("pc" "portbld" "unknown"))))
                        (cdr (string-split %host-type #\-)))))))

(define %prefix (or (getenv "MES_PREFIX")
                      (if (string-prefix? "@prefix" ".")
                          ""
                          ".")))

(define %includedir (or (getenv "includedir")
                        (string-append %prefix "/include")))

(define %libdir (or (getenv "libdir")
                    (string-append %prefix "/lib")))

(define %version (if (string-prefix? "@VERSION" "0.27.1") "git"
                     "0.27.1"))

(define %arch (if (string-prefix? "@mes_cpu" "riscv64") %arch
                  "riscv64"))

(define %kernel (if (string-prefix? "@mes_kernel" "linux") %kernel
                    "linux"))

(setenv "%prefix" %prefix)
(setenv "%includedir" %includedir)
(setenv "%libdir" %libdir)
(setenv "%version" %version)
(setenv "%arch" %arch)
(setenv "%kernel" %kernel)

(cond-expand
 (mes
  (if (current-module) (use-modules (mescc))
    (mes-use-module (mescc))))
 (guile
  (use-modules (mescc))))

(define (compile-1 src out)
  (display src)
  (newline)
  (mescc:main (list "mescc" "-S" "-o" out src))
  (display "mescc-ok ")
  (display src)
  (newline))

(define (main args)
  (compile-1 "hi.c" "hi.s")
  (compile-1 "tccpp.c" "tccpp.s")
  (compile-1 "tccgen.c" "tccgen.s")
  (compile-1 "tccelf.c" "tccelf.s")
  (compile-1 "riscv64-gen.c" "riscv64-gen.s")
  (compile-1 "riscv64-link.c" "riscv64-link.s")
  (compile-1 "riscv64-asm.c" "riscv64-asm.s")
  (compile-1 "tccasm.c" "tccasm.s")
  (compile-1 "libtcc.c" "libtcc.s")
  (compile-1 "tcc.c" "tcc.s")
  (display "tcc-mescc-all-ok")
  (newline))
