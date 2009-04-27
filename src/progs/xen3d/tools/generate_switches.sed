s/^.*(REMREQ[A-Za-z0-9_]*).*/case \1:\n\t\tstruct \L\1\E* msg = (struct \L\1\E*)header;\n\t\tdispatch_\L\1\E(header);\n\t\tbreak;\n/
