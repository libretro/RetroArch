;;; Directory Local Variables
;;; See Info node `(emacs) Directory Variables' for more information.

(
 (c-mode . ((standard-indent . 3)
            (c-basic-offset . 3)
            (c-file-offsets . ((arglist-intro . ++)
                               (arglist-cont-nonempty . ++)
                               (block-close . 0)
                               (block-open . 0)))
            (eval . (setq-local c-cleanup-list
                                (cl-set-difference c-cleanup-list
                                                   '(brace-else-brace
                                                     brace-elseif-brace))))))
 (objc-mode . ((c-basic-offset . 3)))
 )
