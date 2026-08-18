(define-module (nyacc lang c99 pprint)
  #:export (pretty-print-c99))
(define (pretty-print-c99 tree . rest)
  (write tree (if (pair? rest) (car rest) (current-output-port))))
