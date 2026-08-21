;; Overflows STACK_SIZE (200000 frames) unless named-let+cond TCO works.
(display 'tco-probe)
(newline)
(let loop ((n 0))
  (cond
   ((> n 220000)
    (display 'tco-cond-ok)
    (newline))
   (else (loop (+ n 1)))))
(let ((n 0) (go #t))
  (let iter ()
    (cond
     ((> n 220000) (set! go #f))
     (else (set! n (+ n 1))))
    (if go (iter)
        (begin (display 'tco-iter-ok) (newline)))))
